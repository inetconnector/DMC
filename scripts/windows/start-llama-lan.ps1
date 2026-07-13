param(
    [string]$RuntimeRoot = (Join-Path $PSScriptRoot "..\..\runtime\llama.cpp"),
    [string]$CacheRoot = (Join-Path $PSScriptRoot "..\..\runtime\huggingface"),
    [string]$ModelId = "Qwen/Qwen2.5-7B-Instruct-GGUF:Q4_K_M",
    [string]$ModelPath = "",
    [string]$Alias = "dmc-local",
    [string]$Tags = "dmc,local,lan,assistant",
    [ValidateSet("on", "off", "auto")]
    [string]$Reasoning = "off",
    [string]$CudaFlavor = "12.4",
    [int]$Port = 8080,
    [int]$ContextSize = 32768,
    [int]$GpuLayers = 999,
    [int]$SmokeTestTokens = 4096,
    [switch]$StayAlive,
    [switch]$SmokeTest
)

$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Resolve-LatestReleaseAssets {
    $release = Invoke-RestMethod -Headers @{ "User-Agent" = "Codex" } `
        -Uri "https://api.github.com/repos/ggml-org/llama.cpp/releases/latest"

    $binaryPattern = "win-cuda-$CudaFlavor-x64.zip"
    $dllPattern = "cudart-llama-bin-win-cuda-$CudaFlavor-x64.zip"

    $binaryAsset = $release.assets | Where-Object {
        $_.name -like "*$binaryPattern" -and $_.name -notlike "cudart*"
    } | Select-Object -First 1

    $dllAsset = $release.assets | Where-Object {
        $_.name -like $dllPattern
    } | Select-Object -First 1

    if (-not $binaryAsset) {
        throw "Could not find a Windows CUDA llama.cpp binary asset for CUDA $CudaFlavor."
    }
    if (-not $dllAsset) {
        throw "Could not find a matching CUDA runtime bundle for CUDA $CudaFlavor."
    }

    [pscustomobject]@{
        Tag = $release.tag_name
        Binary = $binaryAsset
        Dlls = $dllAsset
    }
}

function Download-And-Extract {
    param(
        [Parameter(Mandatory = $true)]$Asset,
        [Parameter(Mandatory = $true)][string]$DownloadDir,
        [Parameter(Mandatory = $true)][string]$ExtractDir
    )

    Ensure-Directory -Path $DownloadDir
    Ensure-Directory -Path $ExtractDir

    $zipPath = Join-Path $DownloadDir $Asset.name
    if (-not (Test-Path $zipPath)) {
        Write-Host "[download] $($Asset.name)"
        Invoke-WebRequest -Uri $Asset.browser_download_url -OutFile $zipPath
    } else {
        Write-Host "[cache] $($Asset.name)"
    }

    Write-Host "[extract] $($Asset.name)"
    Expand-Archive -Path $zipPath -DestinationPath $ExtractDir -Force
}

function Find-ServerExe {
    param([Parameter(Mandatory = $true)][string]$Root)

    $server = Get-ChildItem -Path $Root -Recurse -Filter llama-server.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if (-not $server) {
        throw "Could not find llama-server.exe under $Root."
    }

    return $server.FullName
}

function Resolve-LocalOllamaModelPath {
    $manifestRoot = Join-Path $env:USERPROFILE ".ollama\models\manifests"
    $blobRoot = Join-Path $env:USERPROFILE ".ollama\models\blobs"
    $candidates = @(
        @{
            Name = "hf.co/empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF:Q5_K_M"
            Manifest = Join-Path $manifestRoot "hf.co\empero-ai\Qwythos-9B-Claude-Mythos-5-1M-GGUF\Q5_K_M"
        },
        @{
            Name = "qwen2.5-coder:7b"
            Manifest = Join-Path $manifestRoot "registry.ollama.ai\library\qwen2.5-coder\7b"
        },
        @{
            Name = "gpt-oss:20b"
            Manifest = Join-Path $manifestRoot "registry.ollama.ai\library\gpt-oss\20b"
        },
        @{
            Name = "qwen2.5-coder:14b"
            Manifest = Join-Path $manifestRoot "registry.ollama.ai\library\qwen2.5-coder\14b"
        },
        @{
            Name = "gemma4:e2b"
            Manifest = Join-Path $manifestRoot "registry.ollama.ai\library\gemma4\e2b"
        }
    )

    foreach ($candidate in $candidates) {
        if (-not (Test-Path $candidate.Manifest)) {
            continue
        }
        $manifest = Get-Content $candidate.Manifest -Raw | ConvertFrom-Json
        $modelLayer = $manifest.layers | Where-Object {
            $_.mediaType -eq "application/vnd.ollama.image.model"
        } | Select-Object -First 1
        if (-not $modelLayer) {
            continue
        }
        $digest = $modelLayer.digest -replace "^sha256:", ""
        $blobPath = Join-Path $blobRoot "sha256-$digest"
        if (Test-Path $blobPath) {
            return [pscustomobject]@{
                Name = $candidate.Name
                Path = $blobPath
            }
        }
    }

    return $null
}

function Wait-ForServer {
    param(
        [Parameter(Mandatory = $true)][int]$Port,
        [int]$MaxAttempts = 90
    )

    $uri = "http://127.0.0.1:$Port/v1/models"
    for ($i = 0; $i -lt $MaxAttempts; $i++) {
        try {
            Invoke-RestMethod -Uri $uri -Method Get -TimeoutSec 5 | Out-Null
            return
        } catch {
            Start-Sleep -Seconds 2
        }
    }

    throw "Server did not become ready at $uri"
}

function New-LongPrompt {
    param([Parameter(Mandatory = $true)][int]$TokenCount)

    $tokens = for ($i = 1; $i -le $TokenCount; $i++) {
        "marker-$i"
    }

    @"
The first marker is marker-1.
The last marker is marker-$TokenCount.

$($tokens -join ' ')

Question: repeat the first marker and the last marker exactly.
"@
}

Ensure-Directory -Path $RuntimeRoot
Ensure-Directory -Path $CacheRoot

$downloadRoot = Join-Path $RuntimeRoot "downloads"
$extractRoot = Join-Path $RuntimeRoot "release"
$logRoot = Join-Path $RuntimeRoot "logs"
Ensure-Directory -Path $downloadRoot
Ensure-Directory -Path $extractRoot
Ensure-Directory -Path $logRoot

$serverExe = Get-ChildItem -Path $extractRoot -Recurse -Filter llama-server.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1

if (-not $serverExe) {
    $assets = Resolve-LatestReleaseAssets
    $versionRoot = Join-Path $extractRoot $assets.Tag
    Ensure-Directory -Path $versionRoot

    Download-And-Extract -Asset $assets.Binary -DownloadDir $downloadRoot -ExtractDir $versionRoot
    Download-And-Extract -Asset $assets.Dlls -DownloadDir $downloadRoot -ExtractDir $versionRoot
    $serverExe = Find-ServerExe -Root $versionRoot
} else {
    $serverExe = $serverExe.FullName
}

$env:HF_HOME = $CacheRoot
$env:HUGGINGFACE_HUB_CACHE = (Join-Path $CacheRoot "hub")
$env:XDG_CACHE_HOME = $CacheRoot

$modelArgs = @()
if ($ModelPath) {
    $resolvedModel = [pscustomobject]@{
        Name = "explicit-local-model"
        Path = (Resolve-Path $ModelPath).Path
    }
} else {
    $resolvedModel = Resolve-LocalOllamaModelPath
}

if ($resolvedModel) {
    Write-Host "[model-source] local gguf: $($resolvedModel.Name)"
    Write-Host "[model-path] $($resolvedModel.Path)"
    $modelArgs = @("-m", $resolvedModel.Path)
} else {
    Write-Host "[model-source] hugging face: $ModelId"
    $modelArgs = @("-hf", $ModelId)
}

$stdoutLog = Join-Path $logRoot "llama-server.out.log"
$stderrLog = Join-Path $logRoot "llama-server.err.log"

$args = @()
$args += $modelArgs
$args += "--host"
$args += "0.0.0.0"
$args += "--port"
$args += $Port
$args += "--alias"
$args += $Alias
$args += "--tags"
$args += $Tags
$args += "--reasoning"
$args += $Reasoning
$args += "-c"
$args += $ContextSize
$args += "-ngl"
$args += $GpuLayers

if ($resolvedModel) {
    $selectedModelName = $Alias
} else {
    $selectedModelName = $Alias
}

Write-Host "[start] $serverExe"
Write-Host "[model] $selectedModelName"
Write-Host "[alias] $Alias"
Write-Host "[reasoning] $Reasoning"
Write-Host "[port] $Port"
Write-Host "[context] $ContextSize"

$proc = Start-Process -FilePath $serverExe `
    -ArgumentList $args `
    -WorkingDirectory (Split-Path $serverExe) `
    -WindowStyle Hidden `
    -PassThru `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $stderrLog

Write-Host "[pid] $($proc.Id)"

Wait-ForServer -Port $Port

if ($SmokeTest) {
    $body = @{
        model = $selectedModelName
        messages = @(
            @{
                role = "user"
                content = New-LongPrompt -TokenCount $SmokeTestTokens
            }
        )
        max_tokens = 32
        temperature = 0
    } | ConvertTo-Json -Depth 8

    $response = Invoke-RestMethod `
        -Method Post `
        -Uri "http://127.0.0.1:$Port/v1/chat/completions" `
        -ContentType "application/json" `
        -Body $body

    $message = $response.choices[0].message
    $text = $message.content
    if (-not $text) {
        $text = $message.reasoning_content
    }
    if ($text) {
        Write-Host "[smoke-test] $text"
    } else {
        Write-Host "[smoke-test] no text returned"
    }
}

$lanIps = (& ipconfig) |
    Select-String -Pattern '(\d{1,3}\.){3}\d{1,3}' |
    ForEach-Object { $_.Matches[0].Value } |
    Where-Object { $_ -ne "127.0.0.1" -and $_ -notlike "169.254*" } |
    Select-Object -Unique

Write-Host "Open on the laptop: http://127.0.0.1:$Port/"
foreach ($ip in $lanIps) {
    Write-Host "Open on the LAN: http://$ip`:$Port/"
}

if ($StayAlive) {
    Write-Host "[hold] waiting for llama-server to exit"
    Wait-Process -Id $proc.Id
}
