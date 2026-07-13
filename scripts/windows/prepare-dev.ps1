param(
    [switch]$InstallPythonDeps,
    [switch]$BuildCpp,
    [switch]$RunPyTests,
    [switch]$RunCppTests
)

$ErrorActionPreference = "Stop"

function Get-CommandPath {
    param([Parameter(Mandatory=$true)][string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $cmd) {
        return $null
    }
    if ($cmd.Path) {
        return $cmd.Path
    }
    return $cmd.Source
}

function Show-Status {
    param([string]$Name)
    $path = Get-CommandPath $Name
    if ($path) {
        Write-Host "[ok] $Name -> $path"
    } else {
        Write-Host "[missing] $Name"
    }
    return $path
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root

$pythonPath = Show-Status python
if (-not $pythonPath) {
    $pythonPath = Show-Status py
}
$cmakePath = Show-Status cmake
$clPath = Show-Status cl
$msbuildPath = Show-Status msbuild

if ($pythonPath -and $InstallPythonDeps) {
    & $pythonPath -m pip install --upgrade pip
    & $pythonPath -m pip install -e ".[dev]"
}

if ($pythonPath -and $RunPyTests) {
    & $pythonPath -m pytest -q
}

if ($BuildCpp -or $RunCppTests) {
    if (-not $cmakePath) {
        throw "cmake is required for the C++ build."
    }
    if (-not $clPath -and -not $msbuildPath) {
        throw "A Visual Studio C++ build environment is required for the C++ build."
    }
    & $cmakePath --preset vs2022-x64
    & $cmakePath --build --preset release
}

if ($RunCppTests) {
    & (Join-Path $root "cpp\build\Release\dmc_test.exe")
}
