[CmdletBinding()]
param(
    [string]$ServiceAccountPath = "$env:USERPROFILE\.gplay\keys\dmc-play.json",
    [string]$Profile = "dmc",
    [switch]$SetDefault
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$packageName = "com.inetconnector.dmc"
$gplay = Get-Command gplay -ErrorAction SilentlyContinue
if (-not $gplay) {
    throw "gplay is not installed or is not available on PATH."
}

$resolvedKey = [System.IO.Path]::GetFullPath($ServiceAccountPath)
if (-not (Test-Path -LiteralPath $resolvedKey -PathType Leaf)) {
    throw "Service-account JSON not found: $resolvedKey`nCreate it in Google Cloud Console and keep it outside the repository."
}

try {
    $credential = Get-Content -LiteralPath $resolvedKey -Raw | ConvertFrom-Json
} catch {
    throw "The service-account file is not valid JSON: $resolvedKey"
}

if ($credential.type -ne "service_account") {
    throw "The JSON file is not a Google service-account credential."
}
if ([string]::IsNullOrWhiteSpace([string]$credential.client_email) -or
    -not ([string]$credential.client_email).EndsWith(".gserviceaccount.com")) {
    throw "The service-account credential has no valid client_email."
}
if ([string]::IsNullOrWhiteSpace([string]$credential.private_key)) {
    throw "The service-account credential has no private key."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$packageConfig = Join-Path $repoRoot ".gplay\config.yaml"
if (-not (Test-Path -LiteralPath $packageConfig -PathType Leaf)) {
    throw "Missing repository package pin: $packageConfig"
}
if (-not (Select-String -LiteralPath $packageConfig -SimpleMatch "default_package: $packageName" -Quiet)) {
    throw "The repository gplay configuration is not pinned to $packageName."
}

Write-Host "[INFO] gplay: $($gplay.Source)"
Write-Host "[INFO] Package: $packageName"
Write-Host "[INFO] Profile: $Profile"
Write-Host "[INFO] Service account: $($credential.client_email)"
Write-Host "[INFO] The private key will not be printed."

$previousDefaultProfile = $null
if (-not $SetDefault) {
    try {
        $currentStatus = (& $gplay.Source auth status | ConvertFrom-Json)
        $previousDefaultProfile = [string]$currentStatus.profile
    } catch {
        Write-Warning "Could not determine the current default gplay profile; no default-profile restoration will be attempted."
    }
}

Push-Location $repoRoot
try {
    $setDefaultArgument = "--set-default=$($SetDefault.IsPresent.ToString().ToLowerInvariant())"
    & $gplay.Source auth login --service-account $resolvedKey --profile $Profile $setDefaultArgument
    if ($LASTEXITCODE -ne 0) { throw "gplay auth login failed with exit code $LASTEXITCODE." }

    & $gplay.Source --profile $Profile auth status
    if ($LASTEXITCODE -ne 0) { throw "gplay auth status failed with exit code $LASTEXITCODE." }

    & $gplay.Source --profile $Profile auth doctor
    if ($LASTEXITCODE -ne 0) {
        throw "gplay auth doctor failed. Confirm that the service account was invited to $packageName with app-scoped release and store-listing permissions."
    }

    $appsJson = & $gplay.Source --profile $Profile apps list --output json
    if ($LASTEXITCODE -ne 0) { throw "gplay apps list failed with exit code $LASTEXITCODE." }
    $apps = $appsJson | ConvertFrom-Json
    if (-not ($apps.apps | Where-Object { $_.packageName -eq $packageName })) {
        throw "The profile is valid, but it cannot access $packageName. Grant this service account app-specific access in Play Console."
    }
    Write-Host "[OK] Service account can access $packageName."
} finally {
    if (-not $SetDefault -and
        -not [string]::IsNullOrWhiteSpace($previousDefaultProfile) -and
        $previousDefaultProfile -ne $Profile) {
        & $gplay.Source auth switch --profile $previousDefaultProfile | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Could not restore the previous default gplay profile '$previousDefaultProfile'."
        }
        Write-Host "[OK] Restored default gplay profile: $previousDefaultProfile"
    }
    Pop-Location
}

Write-Host "[OK] DMC Google Play API access is configured and verified."
