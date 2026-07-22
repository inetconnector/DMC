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
$pnpm = $null
if ($null -ne $env:PNPM_EXE -and (Test-Path -LiteralPath $env:PNPM_EXE -PathType Leaf)) {
    $pnpm = Get-Item -LiteralPath $env:PNPM_EXE
}
if ($null -eq $pnpm) {
    $pnpm = Get-Command pnpm.cmd -ErrorAction SilentlyContinue
}
if ($null -eq $pnpm) {
    $pnpm = Get-Command pnpm -ErrorAction SilentlyContinue
}
if ($null -eq $npm -and $null -eq $pnpm) {
    throw 'Neither npm nor pnpm was found. Install Node.js with npm or provide PNPM_EXE.'
}
$pnpmPath = if ($pnpm -is [System.IO.FileInfo]) { $pnpm.FullName } elseif ($null -ne $pnpm) { $pnpm.Source } else { $null }

$node = Get-Command node.exe -ErrorAction SilentlyContinue
if ($null -eq $node -and $null -ne $npm) {
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
$assetsCommand = Join-Path $uiDirectory 'node_modules\.bin\pwa-assets-generator.cmd'
if (-not (Test-Path -LiteralPath $viteCommand -PathType Leaf)) {
    Write-Host '[INFO] Installing locked Android Web UI dependencies'
    Push-Location $uiDirectory
    try {
        if ($null -ne $npm) {
            & $npm.Source ci
        } else {
            # llama.cpp pins this UI with package-lock.json. Use a fixed npm CLI
            # through pnpm rather than resolving a second, incompatible pnpm lock.
            & $pnpmPath dlx npm@11.5.2 ci --ignore-scripts
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Web UI dependency installation failed with code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "[INFO] Building Android Web UI from $uiDirectory"
Push-Location $uiDirectory
try {
    if ($null -ne $npm) {
        & $npm.Source run build
    } else {
        Write-Host '[INFO] npm is unavailable; running the equivalent locked pnpm build steps'
        if (-not (Test-Path -LiteralPath $assetsCommand -PathType Leaf)) {
            throw "PWA asset generator is missing after dependency installation: $assetsCommand"
        }
        & $assetsCommand --root . --config pwa-assets.config.ts
        if ($LASTEXITCODE -eq 0) {
            & $assetsCommand --root . --config pwa-assets-dark.config.ts
        }
        if ($LASTEXITCODE -eq 0) {
            & $nodePath scripts/make-icons-circular.js
        }
        if ($LASTEXITCODE -eq 0) {
            & $viteCommand build
        }
    }
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
