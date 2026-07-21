param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath($RepositoryRoot)
$uiDirectory = Join-Path $root 'upstream\llama.cpp\tools\ui'
$distDirectory = Join-Path $uiDirectory 'dist'
$publicDirectory = Join-Path $root 'upstream\llama.cpp\tools\server\public'
$packageJson = Join-Path $uiDirectory 'package.json'

if (-not (Test-Path -LiteralPath $packageJson -PathType Leaf)) {
    throw "Android Web UI source is missing: $packageJson"
}

$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if ($null -eq $npm) {
    $npm = Get-Command npm -ErrorAction SilentlyContinue
}
if ($null -eq $npm) {
    throw 'npm was not found. Install Node.js before building the Android app.'
}

$node = Get-Command node.exe -ErrorAction SilentlyContinue
if ($null -eq $node) {
    $nodeCandidate = Join-Path (Split-Path -Parent $npm.Source) 'node.exe'
    if (Test-Path -LiteralPath $nodeCandidate -PathType Leaf) {
        $node = Get-Item -LiteralPath $nodeCandidate
    }
}
if ($null -eq $node) {
    throw 'node.exe was not found next to npm or on PATH.'
}
$nodePath = if ($node -is [System.IO.FileInfo]) { $node.FullName } else { $node.Source }

$viteCommand = Join-Path $uiDirectory 'node_modules\.bin\vite.cmd'
if (-not (Test-Path -LiteralPath $viteCommand -PathType Leaf)) {
    Write-Host '[INFO] Installing locked Android Web UI dependencies with npm ci'
    Push-Location $uiDirectory
    try {
        & $npm.Source ci
        if ($LASTEXITCODE -ne 0) {
            throw "npm ci failed with code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "[INFO] Building Android Web UI from $uiDirectory"
Push-Location $uiDirectory
try {
    & $npm.Source run build
    if ($LASTEXITCODE -ne 0) {
        throw "Android Web UI build failed with code $LASTEXITCODE."
    }
} finally {
    Pop-Location
}

$distIndex = Join-Path $distDirectory 'index.html'
if (-not (Test-Path -LiteralPath $distIndex -PathType Leaf)) {
    throw "Android Web UI build produced no index.html: $distIndex"
}

New-Item -ItemType Directory -Path $publicDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $distDirectory '*') -Destination $publicDirectory -Recurse -Force
Write-Host "[OK] Android Web UI synced to $publicDirectory"

$licenseGenerator = Join-Path $root 'scripts\generate-webui-license-notices.mjs'
& $nodePath $licenseGenerator
if ($LASTEXITCODE -ne 0) {
    throw "Web UI third-party license generation failed with code $LASTEXITCODE."
}
