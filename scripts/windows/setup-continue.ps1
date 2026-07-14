param(
    [string]$ConfigPath = (Join-Path $env:USERPROFILE ".continue\config.yaml"),
    [string]$ApiBase = "http://127.0.0.1:8080/v1",
    [string]$ModelName = "dmc-local",
    [string]$ApiKey = "local"
)

$ErrorActionPreference = "Stop"

$configDir = Split-Path $ConfigPath -Parent
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir | Out-Null
}

$config = @"
name: DMC Continue Local
version: 0.0.1
schema: v1

models:
  - name: DMC Local Chat
    provider: openai
    model: $ModelName
    apiBase: $ApiBase
    apiKey: $ApiKey
    capabilities:
      - tool_use
  - name: DMC Local Autocomplete
    provider: openai
    model: $ModelName
    apiBase: $ApiBase
    apiKey: $ApiKey
    roles:
      - autocomplete
"@

Set-Content -Path $ConfigPath -Value $config -NoNewline

Write-Host "[continue] wrote $ConfigPath"
Write-Host "[continue] apiBase = $ApiBase"
Write-Host "[continue] model = $ModelName"
