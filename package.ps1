param (
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$DistDir = "dist"
$ZipPath = Join-Path $DistDir "zclip.zip"

if (-not (Test-Path (Join-Path "build" $Config "zclip.exe"))) {
    Write-Host "==> Building $Config first..." -ForegroundColor Cyan
    & "$PSScriptRoot\task.ps1" build $Config
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

$Files = @(
    (Join-Path "build" $Config "zclip.exe"),
    "PAIRING.txt",
    "README.md",
    "LICENSE"
) | Where-Object { Test-Path $_ }

if (-not $Files) {
    throw "No files to package."
}

Compress-Archive -Path $Files -DestinationPath $ZipPath -Force
Write-Host "==> Packaged $ZipPath" -ForegroundColor Green