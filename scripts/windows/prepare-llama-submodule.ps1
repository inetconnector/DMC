param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath($RepositoryRoot)
$relativePath = 'upstream/llama.cpp'
$submodulePath = Join-Path $root 'upstream\llama.cpp'
$gitmodulesPath = Join-Path $root '.gitmodules'

if (-not (Test-Path -LiteralPath $gitmodulesPath -PathType Leaf)) {
    throw "Missing .gitmodules. Clone the repository with submodules enabled."
}

$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($null -eq $git) {
    $git = Get-Command git -ErrorAction SilentlyContinue
}
if ($null -eq $git) {
    throw 'Git was not found. Install Git before building the Android app.'
}

Write-Host "[INFO] Initializing pinned llama.cpp submodule"
& $git.Source -C $root submodule update --init --recursive -- $relativePath
if ($LASTEXITCODE -ne 0) {
    throw "Could not initialize $relativePath."
}

$indexEntry = (& $git.Source -C $root ls-files --stage -- $relativePath | Out-String).Trim()
if ($indexEntry -notmatch '^160000\s+([0-9a-f]{40})\s+\d+\s+') {
    throw "$relativePath is not registered as a pinned Git submodule."
}
$expectedCommit = $Matches[1]
$actualCommit = (& $git.Source -C $submodulePath rev-parse HEAD | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "llama.cpp commit mismatch: expected $expectedCommit, found $actualCommit."
}

$originUrl = (& $git.Source -C $submodulePath remote get-url origin | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $originUrl -notmatch '(^|[:/])inetconnector/llama\.cpp(?:\.git)?$') {
    throw "Unexpected llama.cpp origin: $originUrl"
}

$status = (& $git.Source -C $submodulePath status --porcelain --untracked-files=normal | Out-String).Trim()
if ($status) {
    throw "The pinned llama.cpp submodule is dirty:`n$status"
}

$requiredFiles = @(
    'CMakeLists.txt',
    'LICENSE',
    'DMC_FORK.md',
    'tools\ui\package.json',
    'tools\ui\src\lib\i18n.ts'
)
foreach ($requiredFile in $requiredFiles) {
    $candidate = Join-Path $submodulePath $requiredFile
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Pinned llama.cpp source is incomplete: $candidate"
    }
}

Write-Host "[OK] llama.cpp submodule verified at $actualCommit"
