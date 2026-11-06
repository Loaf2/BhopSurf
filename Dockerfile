FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY AuthApi/AuthApi.csproj AuthApi/
RUN dotnet restore AuthApi/AuthApi.csproj
COPY AuthApi/ AuthApi/
WORKDIR /src/AuthApi
RUN dotnet publish AuthApi.csproj -c Release -o /app/publish /p:UseAppHost=false

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app/publish .
ENV ASPNETCORE_URLS=http://0.0.0.0:10000
EXPOSE 10000
ENTRYPOINT ["dotnet", "AuthApi.dll"]
