using System.Net;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mail;
using System.Security.Cryptography;
using System.Text;
using Npgsql;

var builder = WebApplication.CreateBuilder(args);
string? connectionString = builder.Configuration["Postgres:ConnectionString"];
if (string.IsNullOrWhiteSpace(connectionString))
    throw new InvalidOperationException("Configure Postgres:ConnectionString with user-secrets or an environment variable.");

await using var dataSource = NpgsqlDataSource.Create(connectionString);
var emailClient = new HttpClient { Timeout = TimeSpan.FromSeconds(15) };
string? resendApiKey = builder.Configuration["Email:ResendApiKey"];
string? emailFromAddress = builder.Configuration["Email:FromAddress"];
if (builder.Environment.IsDevelopment() &&
    (string.IsNullOrWhiteSpace(emailFromAddress) || emailFromAddress.Contains("your-verified-domain", StringComparison.OrdinalIgnoreCase)))
    emailFromAddress = "BhopSurf <onboarding@resend.dev>";
string publicBaseUrl = builder.Configuration["Email:PublicBaseUrl"] ?? "http://localhost:5000";
await using (var connection = await dataSource.OpenConnectionAsync())
{
    await using var createTable = new NpgsqlCommand(
        """
        CREATE TABLE IF NOT EXISTS accounts (
            id UUID PRIMARY KEY,
            email VARCHAR(254) NOT NULL,
            normalized_email VARCHAR(254) NOT NULL,
            password_hash TEXT NOT NULL,
            created_at_utc TIMESTAMPTZ NOT NULL,
            email_confirmed BOOLEAN NOT NULL DEFAULT TRUE,
            confirmation_token_hash TEXT NULL,
            confirmation_expires_at_utc TIMESTAMPTZ NULL,
            aim_best_score INTEGER NOT NULL DEFAULT 0,
            two_balls_best_score INTEGER NOT NULL DEFAULT 0,
            six_balls_best_score INTEGER NOT NULL DEFAULT 0
        );
        """, connection);
    await createTable.ExecuteNonQueryAsync();
    await using var migrateTable = new NpgsqlCommand(
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS email_confirmed BOOLEAN NOT NULL DEFAULT TRUE; " +
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS confirmation_token_hash TEXT NULL; " +
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS confirmation_expires_at_utc TIMESTAMPTZ NULL; " +
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS aim_best_score INTEGER NOT NULL DEFAULT 0; " +
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS two_balls_best_score INTEGER NOT NULL DEFAULT 0; " +
        "ALTER TABLE accounts ADD COLUMN IF NOT EXISTS six_balls_best_score INTEGER NOT NULL DEFAULT 0;", connection);
    await migrateTable.ExecuteNonQueryAsync();

    await using var createSessions = new NpgsqlCommand(
        "CREATE TABLE IF NOT EXISTS auth_sessions (" +
        "token_hash TEXT PRIMARY KEY, " +
        "account_id UUID NOT NULL REFERENCES accounts(id) ON DELETE CASCADE, " +
        "expires_at_utc TIMESTAMPTZ NOT NULL);", connection);
    await createSessions.ExecuteNonQueryAsync();

    await using var createIndex = new NpgsqlCommand(
        "CREATE UNIQUE INDEX IF NOT EXISTS ux_accounts_normalized_email ON accounts (normalized_email);", connection);
    await createIndex.ExecuteNonQueryAsync();
}

var app = builder.Build();
if (!app.Environment.IsDevelopment())
    app.UseHttpsRedirection();

app.MapGet("/health", () => Results.Ok(new { status = "ok" }));

app.MapPost("/api/auth/register", async (AuthRequest request, ILoggerFactory loggerFactory, CancellationToken cancellationToken) =>
{
    string? normalizedEmail = NormalizeEmail(request.Email);
    if (normalizedEmail is null || request.Password is null || request.Password.Length < 6 || request.Password.Length > 128)
        return Results.BadRequest(new { error = "Invalid email or password." });

    Guid accountId = Guid.NewGuid();
    string email = request.Email!.Trim();
    string passwordHash = PasswordHashing.Hash(request.Password);
    string confirmationToken = CreateConfirmationToken();
    DateTime expiresAtUtc = DateTime.UtcNow.AddHours(24);

    try
    {
        await using var connection = await dataSource.OpenConnectionAsync(cancellationToken);
        await using var command = new NpgsqlCommand(
            "INSERT INTO accounts (id, email, normalized_email, password_hash, created_at_utc, email_confirmed, confirmation_token_hash, confirmation_expires_at_utc) " +
            "VALUES (@id, @email, @normalized_email, @password_hash, @created_at_utc, FALSE, @token_hash, @expires_at_utc);", connection);
        command.Parameters.AddWithValue("id", accountId);
        command.Parameters.AddWithValue("email", email);
        command.Parameters.AddWithValue("normalized_email", normalizedEmail);
        command.Parameters.AddWithValue("password_hash", passwordHash);
        command.Parameters.AddWithValue("created_at_utc", DateTime.UtcNow);
        command.Parameters.AddWithValue("token_hash", HashConfirmationToken(confirmationToken));
        command.Parameters.AddWithValue("expires_at_utc", expiresAtUtc);
        await command.ExecuteNonQueryAsync(cancellationToken);
        bool emailSent = await SendConfirmationEmailAsync(emailClient, resendApiKey, emailFromAddress,
            publicBaseUrl, email, confirmationToken, loggerFactory.CreateLogger("ConfirmationEmail"), cancellationToken);
        if (!emailSent)
            return Results.Problem("Account created, but the confirmation email could not be sent. Check the email configuration and request another confirmation email.",
                statusCode: StatusCodes.Status503ServiceUnavailable);
        return Results.Accepted(value: new { email, message = "Confirmation email sent." });
    }
    catch (PostgresException exception) when (exception.SqlState == PostgresErrorCodes.UniqueViolation)
    {
        return Results.Conflict(new { error = "An account with that email already exists." });
    }
}).AllowAnonymous();

app.MapGet("/api/auth/confirm", async (string? token, CancellationToken cancellationToken) =>
{
    if (string.IsNullOrWhiteSpace(token) || token.Length > 128)
        return Results.BadRequest("Invalid or expired confirmation link.");

    await using var connection = await dataSource.OpenConnectionAsync(cancellationToken);
    await using var command = new NpgsqlCommand(
        "UPDATE accounts SET email_confirmed = TRUE, confirmation_token_hash = NULL, confirmation_expires_at_utc = NULL " +
        "WHERE confirmation_token_hash = @token_hash AND confirmation_expires_at_utc > @now_utc AND email_confirmed = FALSE;", connection);
    command.Parameters.AddWithValue("token_hash", HashConfirmationToken(token));
    command.Parameters.AddWithValue("now_utc", DateTime.UtcNow);
    int updated = await command.ExecuteNonQueryAsync(cancellationToken);
    return updated == 1
        ? Results.Ok("Email confirmed. You can now sign in.")
        : Results.BadRequest("Invalid or expired confirmation link.");
}).AllowAnonymous();

app.MapPost("/api/auth/resend-confirmation", async (ResendConfirmationRequest request, ILoggerFactory loggerFactory, CancellationToken cancellationToken) =>
{
    string? normalizedEmail = NormalizeEmail(request.Email);
    if (normalizedEmail is null)
        return Results.BadRequest(new { error = "Invalid email." });

    string confirmationToken = CreateConfirmationToken();
    string? accountEmail = null;
    await using (var connection = await dataSource.OpenConnectionAsync(cancellationToken))
    await using (var command = new NpgsqlCommand(
        "UPDATE accounts SET confirmation_token_hash = @token_hash, confirmation_expires_at_utc = @expires_at_utc " +
        "WHERE normalized_email = @normalized_email AND email_confirmed = FALSE RETURNING email;", connection))
    {
        command.Parameters.AddWithValue("token_hash", HashConfirmationToken(confirmationToken));
        command.Parameters.AddWithValue("expires_at_utc", DateTime.UtcNow.AddHours(24));
        command.Parameters.AddWithValue("normalized_email", normalizedEmail);
        accountEmail = await command.ExecuteScalarAsync(cancellationToken) as string;
    }

    if (accountEmail is null)
    {
        Console.Error.WriteLine("Confirmation resend skipped: no matching unconfirmed account was found.");
    }
    else if (!await SendConfirmationEmailAsync(emailClient, resendApiKey, emailFromAddress, publicBaseUrl,
        accountEmail, confirmationToken, loggerFactory.CreateLogger("ConfirmationEmail"), cancellationToken))
    {
        Console.Error.WriteLine("Confirmation resend failed. See the preceding Resend diagnostic.");
        return Results.Problem("The confirmation email could not be sent. Check the API terminal for a safe diagnostic.",
            statusCode: StatusCodes.Status503ServiceUnavailable);
    }
    else
    {
        Console.Error.WriteLine("Resend accepted the confirmation email request.");
    }
    return Results.Accepted(value: new { message = "If an unconfirmed account exists for that email, a confirmation email has been sent." });
}).AllowAnonymous();

app.MapPost("/api/auth/login", async (AuthRequest request, CancellationToken cancellationToken) =>
{
    string? normalizedEmail = NormalizeEmail(request.Email);
    if (normalizedEmail is null || string.IsNullOrEmpty(request.Password))
        return Results.Unauthorized();

    Guid accountId;
    string email;
    string passwordHash;
    bool emailConfirmed;
    await using (var connection = await dataSource.OpenConnectionAsync(cancellationToken))
    await using (var command = new NpgsqlCommand(
        "SELECT id, email, password_hash, email_confirmed FROM accounts WHERE normalized_email = @normalized_email LIMIT 1;", connection))	
    {
        command.Parameters.AddWithValue("normalized_email", normalizedEmail);
        await using var reader = await command.ExecuteReaderAsync(cancellationToken);
        if (!await reader.ReadAsync(cancellationToken))
            return Results.Unauthorized();
        accountId = reader.GetGuid(0);
        email = reader.GetString(1);
        passwordHash = reader.GetString(2);
        emailConfirmed = reader.GetBoolean(3);
    }
    if (!PasswordHashing.VerifyForLogin(request.Password, passwordHash))
        return Results.Unauthorized();
    if (!emailConfirmed)
        return Results.Json(new { error = "Email confirmation required." }, statusCode: StatusCodes.Status403Forbidden);

    string sessionToken = CreateSessionToken();
    await using (var connection = await dataSource.OpenConnectionAsync(cancellationToken))
    await using (var command = new NpgsqlCommand(
        "INSERT INTO auth_sessions (token_hash, account_id, expires_at_utc) VALUES (@token_hash, @account_id, @expires_at_utc);", connection))
    {
        command.Parameters.AddWithValue("token_hash", HashSessionToken(sessionToken));
        command.Parameters.AddWithValue("account_id", accountId);
        command.Parameters.AddWithValue("expires_at_utc", DateTime.UtcNow.AddDays(30));
        await command.ExecuteNonQueryAsync(cancellationToken);
    }

    return Results.Ok(new { userId = accountId, email, sessionToken });
}).AllowAnonymous();

app.MapGet("/api/ranks", async (HttpRequest request, CancellationToken cancellationToken) =>
{
    Guid? accountId = await GetSessionAccountIdAsync(request, dataSource, cancellationToken);
    if (accountId is null)
        return Results.Unauthorized();

    await using var connection = await dataSource.OpenConnectionAsync(cancellationToken);
    await using var command = new NpgsqlCommand(
        "SELECT aim_best_score, two_balls_best_score, six_balls_best_score FROM accounts WHERE id = @account_id;", connection);
    command.Parameters.AddWithValue("account_id", accountId.Value);
    await using var reader = await command.ExecuteReaderAsync(cancellationToken);
    if (!await reader.ReadAsync(cancellationToken))
        return Results.Unauthorized();
    return Results.Ok(new RankScores(reader.GetInt32(0), reader.GetInt32(1), reader.GetInt32(2)));
});

app.MapPut("/api/ranks", async (RankScores scores, HttpRequest request, CancellationToken cancellationToken) =>
{
    if (scores.AimBestScore < 0 || scores.TwoBallsBestScore < 0 || scores.SixBallsBestScore < 0)
        return Results.BadRequest(new { error = "Scores cannot be negative." });

    Guid? accountId = await GetSessionAccountIdAsync(request, dataSource, cancellationToken);
    if (accountId is null)
        return Results.Unauthorized();

    await using var connection = await dataSource.OpenConnectionAsync(cancellationToken);
    await using var command = new NpgsqlCommand(
        "UPDATE accounts SET " +
        "aim_best_score = GREATEST(aim_best_score, @aim_best_score), " +
        "two_balls_best_score = GREATEST(two_balls_best_score, @two_balls_best_score), " +
        "six_balls_best_score = GREATEST(six_balls_best_score, @six_balls_best_score) " +
        "WHERE id = @account_id " +
        "RETURNING aim_best_score, two_balls_best_score, six_balls_best_score;", connection);
    command.Parameters.AddWithValue("aim_best_score", scores.AimBestScore);
    command.Parameters.AddWithValue("two_balls_best_score", scores.TwoBallsBestScore);
    command.Parameters.AddWithValue("six_balls_best_score", scores.SixBallsBestScore);
    command.Parameters.AddWithValue("account_id", accountId.Value);
    await using var reader = await command.ExecuteReaderAsync(cancellationToken);
    if (!await reader.ReadAsync(cancellationToken))
        return Results.Unauthorized();
    return Results.Ok(new RankScores(reader.GetInt32(0), reader.GetInt32(1), reader.GetInt32(2)));
});

app.Run();

static string? NormalizeEmail(string? email)
{
    if (string.IsNullOrWhiteSpace(email))
        return null;

    string trimmedEmail = email.Trim();
    if (trimmedEmail.Length > 254 || !MailAddress.TryCreate(trimmedEmail, out MailAddress? parsedEmail) ||
        !string.Equals(parsedEmail.Address, trimmedEmail, StringComparison.OrdinalIgnoreCase))
        return null;

    return trimmedEmail.ToLowerInvariant();
}

static string CreateConfirmationToken()
{
    return Convert.ToBase64String(RandomNumberGenerator.GetBytes(32))
        .TrimEnd('=')
        .Replace('+', '-')
        .Replace('/', '_');
}

static string HashConfirmationToken(string token)
{
    return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(token)));
}

static string CreateSessionToken()
{
    return Convert.ToBase64String(RandomNumberGenerator.GetBytes(32))
        .TrimEnd('=')
        .Replace('+', '-')
        .Replace('/', '_');
}

static string HashSessionToken(string token)
{
    return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(token)));
}

static async Task<Guid?> GetSessionAccountIdAsync(
    HttpRequest request,
    NpgsqlDataSource dataSource,
    CancellationToken cancellationToken)
{
    if (!AuthenticationHeaderValue.TryParse(request.Headers["Authorization"], out AuthenticationHeaderValue? authorization) ||
        !string.Equals(authorization.Scheme, "Bearer", StringComparison.OrdinalIgnoreCase) ||
        string.IsNullOrWhiteSpace(authorization.Parameter))
        return null;

    await using var connection = await dataSource.OpenConnectionAsync(cancellationToken);
    await using var command = new NpgsqlCommand(
        "SELECT account_id FROM auth_sessions WHERE token_hash = @token_hash AND expires_at_utc > @now_utc;", connection);
    command.Parameters.AddWithValue("token_hash", HashSessionToken(authorization.Parameter));
    command.Parameters.AddWithValue("now_utc", DateTime.UtcNow);
    object? accountId = await command.ExecuteScalarAsync(cancellationToken);
    return accountId is Guid id ? id : null;
}

static async Task<bool> SendConfirmationEmailAsync(
    HttpClient emailClient,
    string? apiKey,
    string? fromAddress,
    string publicBaseUrl,
    string email,
    string token,
    ILogger logger,
    CancellationToken cancellationToken)
{
    if (string.IsNullOrWhiteSpace(apiKey) || string.IsNullOrWhiteSpace(fromAddress) ||
        fromAddress.Contains("your-verified-domain", StringComparison.OrdinalIgnoreCase))
    {
        string configurationError = string.IsNullOrWhiteSpace(apiKey)
            ? "Confirmation email not sent: Email:ResendApiKey is missing."
            : "Confirmation email not sent: Email:FromAddress is missing or still has the example placeholder.";
        logger.LogError("{EmailError}", configurationError);
        Console.Error.WriteLine(configurationError);
        return false;
    }

    string confirmationUrl = $"{publicBaseUrl.TrimEnd('/')}/api/auth/confirm?token={Uri.EscapeDataString(token)}";
    string encodedUrl = WebUtility.HtmlEncode(confirmationUrl);
    using var request = new HttpRequestMessage(HttpMethod.Post, "https://api.resend.com/emails");
    request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);
    request.Content = JsonContent.Create(new
    {
        from = fromAddress,
        to = new[] { email },
        subject = "Confirm your BhopSurf account",
        html = $"<p>Confirm your BhopSurf account:</p><p><a href=\"{encodedUrl}\">Confirm email</a></p>",
        text = $"Confirm your BhopSurf account: {confirmationUrl}"
    });

    try
    {
        using HttpResponseMessage response = await emailClient.SendAsync(request, cancellationToken);
        if (response.IsSuccessStatusCode)
            return true;

        string providerError = await response.Content.ReadAsStringAsync(cancellationToken);
        logger.LogError("Resend rejected the confirmation email with HTTP {StatusCode}: {ProviderError}",
            (int)response.StatusCode, providerError);
        Console.Error.WriteLine($"Resend rejected the confirmation email with HTTP {(int)response.StatusCode}: {providerError}");
        return false;
    }
    catch (HttpRequestException exception)
    {
        logger.LogError(exception, "The confirmation email request could not reach Resend.");
        Console.Error.WriteLine($"The confirmation email request could not reach Resend: {exception.Message}");
        return false;
    }
    catch (TaskCanceledException exception)
    {
        logger.LogError(exception, "The confirmation email request timed out.");
        Console.Error.WriteLine($"The confirmation email request timed out: {exception.Message}");
        return false;
    }
}

sealed record AuthRequest(string? Email, string? Password);
sealed record ResendConfirmationRequest(string? Email);
sealed record RankScores(int AimBestScore, int TwoBallsBestScore, int SixBallsBestScore);

static class PasswordHashing
{
    private const int Iterations = 600_000;
    private const int SaltSize = 16;
    private const int HashSize = 32;

    public static string Hash(string password)
    {
        byte[] salt = RandomNumberGenerator.GetBytes(SaltSize);
        byte[] hash = Rfc2898DeriveBytes.Pbkdf2(password, salt, Iterations, HashAlgorithmName.SHA256, HashSize);
        return $"v2.{Iterations}.{Convert.ToBase64String(salt)}.{Convert.ToBase64String(hash)}";
    }

    public static bool VerifyForLogin(string password, string encodedHash)
    {
        if (Verify(password, encodedHash))
            return true;

        return !encodedHash.StartsWith("v2.", StringComparison.Ordinal) &&
            Verify(password.ToLowerInvariant(), encodedHash);
    }

    static bool Verify(string password, string encodedHash)
    {
        string[] parts = encodedHash.Split('.');
        bool versionedHash = parts.Length == 4 && parts[0] == "v2";
        if ((!versionedHash && parts.Length != 3) ||
            !int.TryParse(parts[versionedHash ? 1 : 0], out int iterations) || iterations < 100_000 || iterations > 2_000_000)
            return false;

        try
        {
            byte[] salt = Convert.FromBase64String(parts[versionedHash ? 2 : 1]);
            byte[] expectedHash = Convert.FromBase64String(parts[versionedHash ? 3 : 2]);
            byte[] actualHash = Rfc2898DeriveBytes.Pbkdf2(password, salt, iterations, HashAlgorithmName.SHA256, expectedHash.Length);
            return CryptographicOperations.FixedTimeEquals(actualHash, expectedHash);
        }
        catch (FormatException)
        {
            return false;
        }
    }
}
