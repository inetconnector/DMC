param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedArtifact = (Resolve-Path -LiteralPath $ArtifactPath).Path
Add-Type -AssemblyName System.IO.Compression.FileSystem

$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedArtifact)
try {
    $entryNames = @($archive.Entries | ForEach-Object { $_.FullName })
    foreach ($forbiddenEntry in @('assets/source-catalog.json', 'base/assets/source-catalog.json')) {
        if ($entryNames -contains $forbiddenEntry) {
            throw "Play artifact contains excluded knowledge catalog: $forbiddenEntry"
        }
    }

    $requiredMarkers = @(
        'inetmind_full_unlock',
        'https://apps.inetconnector.com/inetmind/v1'
    )
    $forbiddenMarkers = @(
        'ICD-10',
        'ICD-11',
        'icd10',
        'icd11',
        'BfArM',
        'Orphadata',
        'medical classifications',
        'medizinische Klassifikationen'
    )
    $requiredFound = @{}
    foreach ($marker in $requiredMarkers) {
        $requiredFound[$marker] = $false
    }

    foreach ($entry in $archive.Entries) {
        if ($entry.Length -eq 0 -or $entry.Length -gt 64MB) {
            continue
        }
        if (
            -not $entry.FullName.EndsWith('.dex') -and
            -not $entry.FullName.EndsWith('.arsc') -and
            -not $entry.FullName.EndsWith('.xml') -and
            -not $entry.FullName.EndsWith('.html') -and
            -not $entry.FullName.EndsWith('.js') -and
            -not $entry.FullName.EndsWith('.json')
        ) {
            continue
        }

        $memory = [System.IO.MemoryStream]::new()
        try {
            $stream = $entry.Open()
            try {
                $stream.CopyTo($memory)
            } finally {
                $stream.Dispose()
            }
            $bytes = $memory.ToArray()
            $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
            $unicode = [System.Text.Encoding]::Unicode.GetString($bytes)
            foreach ($marker in $forbiddenMarkers) {
                if ($ascii.Contains($marker) -or $unicode.Contains($marker)) {
                    throw "Play artifact contains excluded marker '$marker' in $($entry.FullName)"
                }
            }
            foreach ($marker in $requiredMarkers) {
                if ($ascii.Contains($marker) -or $unicode.Contains($marker)) {
                    $requiredFound[$marker] = $true
                }
            }
        } finally {
            $memory.Dispose()
        }
    }

    foreach ($marker in $requiredMarkers) {
        if (-not $requiredFound[$marker]) {
            throw "Play artifact is missing required marker: $marker"
        }
    }
} finally {
    $archive.Dispose()
}

Write-Host "[OK] Verified medical-free Play artifact and required billing/trial markers: $resolvedArtifact"
