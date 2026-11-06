# BhopSurf Auth API

This ASP.NET Core API stores accounts in PostgreSQL. Passwords are hashed on the server with PBKDF2-SHA256. The PostgreSQL connection string must never be placed in the game client or committed to the repository.

## Set up PostgreSQL

Install PostgreSQL locally or create a database with a managed PostgreSQL provider. Create a database (for example, `bhopsurf`) and a database user that can create tables and read/write rows in that database. Keep the database password private.

## Configure the API locally

From this directory, save the connection string in .NET user-secrets, which is stored outside the repository:

```powershell
dotnet user-secrets set "Postgres:ConnectionString" 'Host=localhost;Port=5432;Database=bhopsurf;Username=postgres;Password=<your-password>'
```

Replace the example host, database, username, and password with your PostgreSQL values. For a managed provider, use its connection string and required SSL options.

## Configure confirmation email

Store the Resend API key only in .NET user-secrets (never in source files or the C++ client):

```powershell
dotnet user-secrets set "Email:ResendApiKey" "<your-new-resend-api-key>"
dotnet user-secrets set "Email:PublicBaseUrl" "http://localhost:5000"
```

In Development, the API uses Resend's `BhopSurf <onboarding@resend.dev>` test sender when no sender is configured or the example placeholder is still present. Resend's test sender can only deliver to the email address associated with your Resend account. To send to other recipients, verify a domain in Resend and set `Email:FromAddress` to an address on that domain:

```powershell
dotnet user-secrets set "Email:FromAddress" "BhopSurf <noreply@your-domain.example>"
```

The local base URL works for confirmation only when the email is opened on the same computer that runs the API. For other players, deploy the API over HTTPS and set `Email:PublicBaseUrl` to its public HTTPS origin. On a server, provide the key using the `Email__ResendApiKey` environment variable and configure the other email settings there as well.

If confirmation email sending fails, check the API console for the Resend HTTP status or missing configuration details. The API does not log the Resend key.

Start the API:

```powershell
dotnet run --urls http://localhost:5000
```

At startup, the API creates or migrates the `accounts`, `auth_sessions`, and unique normalized-email index. Existing accounts are kept confirmed. `GET /health` checks that the API is running. The game calls `http://localhost:5000`, so keep the API running while testing the game on the same computer. If the game reports `START LOCAL API`, start this API and verify that PostgreSQL is configured and reachable; the API exits on startup when its connection string is missing.

## API routes

- `POST /api/auth/register` with JSON fields `email` and `password` creates an unconfirmed account and sends a 24-hour confirmation link.
- `GET /api/auth/confirm?token=...` confirms an account using the link in the email.
- `POST /api/auth/resend-confirmation` with JSON field `email` sends a replacement link when an unconfirmed account exists.
- `POST /api/auth/login` with the same fields verifies a confirmed account and returns a 30-day bearer session. An unconfirmed account cannot sign in.
- `GET /api/ranks` with that bearer session returns the account's best score in each aim mode.
- `PUT /api/ranks` with that bearer session updates the three mode scores without lowering existing bests.

Registration returns HTTP 202 while email confirmation is pending. New passwords preserve the casing entered and are case-sensitive; older password hashes remain compatible with the earlier lowercase-normalization behavior. The game provides a show/hide password option, and lowercase letters render as lowercase glyphs. Passwords remain one-way hashes and cannot be viewed or recovered. Rank names are derived from the saved best scores using the game's existing thresholds.

## Hosting for other players

`localhost` only works on the computer running the API. To serve other players, deploy this API with HTTPS and set `Postgres__ConnectionString` as a server environment variable. Restrict database access to the API host where possible. Change `baseUrl` in `../auth_api_config.h` to the API's HTTPS origin and rebuild the game. Never put a PostgreSQL connection string or database password in the C++ client.
