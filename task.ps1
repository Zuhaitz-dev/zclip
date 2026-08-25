param (
    [Parameter(Position=0)]
    [ValidateSet("build", "run", "clean", "format", "tidy", "test", "all")]
    [string]$Action = "build",

    [Parameter(Position=1)]
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"

function Ensure-Configured {
    if (-not (Test-Path $BuildDir)) {
        Write-Host "==> Generating build tree ($Config)..." -ForegroundColor Cyan
        cmake -B $BuildDir -S . -G "Visual Studio 18" -A x64
    }
}

switch ($Action) {
    "build" {
        Ensure-Configured
        Write-Host "==> Compiling zclip ($Config)..." -ForegroundColor Green
        cmake --build $BuildDir --config $Config
    }
    "run" {
        Ensure-Configured
        Write-Host "==> Compiling & Running zclip..." -ForegroundColor Green
        cmake --build $BuildDir --config $Config
        $ExePath = Join-Path $BuildDir $Config "zclip.exe"
        if (Test-Path $ExePath) {
            & $ExePath
        } else {
            Write-Error "Executable not found at $ExePath"
        }
    }
    "format" {
        Ensure-Configured
        Write-Host "==> Running clang-format in-place..." -ForegroundColor Yellow
        cmake --build $BuildDir --target format --config $Config
    }
    "tidy" {
        Ensure-Configured
        Write-Host "==> Running clang-tidy static analysis..." -ForegroundColor Yellow
        cmake --build $BuildDir --target tidy --config $Config
    }
    "clean" {
        Write-Host "==> Removing build directory..." -ForegroundColor Red
        if (Test-Path $BuildDir) {
            Remove-Item -Recurse -Force $BuildDir
        }
        Write-Host "Cleaned." -ForegroundColor Green
    }
    "all" {
        Ensure-Configured
        Write-Host "==> Formatting, building, and linting..." -ForegroundColor Cyan
        try { cmake --build $BuildDir --target format --config $Config } catch {}
        cmake --build $BuildDir --config $Config
        try { cmake --build $BuildDir --target tidy --config $Config } catch {}
    }
}
