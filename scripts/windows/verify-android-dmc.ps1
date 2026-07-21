param(
    [Parameter(Mandatory = $true)]
    [string]$ApkPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedApk = (Resolve-Path -LiteralPath $ApkPath).Path
Add-Type -AssemblyName System.IO.Compression.FileSystem

$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedApk)
try {
    $entry = $archive.GetEntry('lib/arm64-v8a/libai-chat.so')
    if ($null -eq $entry) {
        throw 'APK does not contain lib/arm64-v8a/libai-chat.so.'
    }

    $memory = [System.IO.MemoryStream]::new()
    try {
        $stream = $entry.Open()
        try {
            $stream.CopyTo($memory)
        } finally {
            $stream.Dispose()
        }
        $nativeText = [System.Text.Encoding]::ASCII.GetString($memory.ToArray())
    } finally {
        $memory.Dispose()
    }

    $requiredMarkers = @(
        'DMC_RUNTIME enabled=1',
        'DMC_RUNTIME self-test passed',
        'DMC_RUNTIME rebuild=',
        'DMC_RUNTIME generation continuation:',
        'GENERATION limit='
    )
    foreach ($marker in $requiredMarkers) {
        if (-not $nativeText.Contains($marker)) {
            throw "APK is missing required DMC marker: $marker"
        }
    }
} finally {
    $archive.Dispose()
}

Write-Host "[OK] Verified DMC runtime markers in $resolvedApk"
