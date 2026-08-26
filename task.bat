@echo off
setlocal enabledelayedexpansion

set ACTION=%1
if "%ACTION%"=="" set ACTION=build
set CONFIG=%2
if "%CONFIG%"=="" set CONFIG=Release

if "%ACTION%"=="clean" goto clean
if "%ACTION%"=="build" goto build
if "%ACTION%"=="run" goto run
if "%ACTION%"=="format" goto format
if "%ACTION%"=="tidy" goto tidy
if "%ACTION%"=="all" goto all

echo Unknown action: %ACTION%
echo Usage: task.bat [build^|run^|clean^|format^|tidy^|all] [Release^|Debug]
exit /b 1

:configure
if not exist build (
    echo [TASK] Generating build system...
    cmake -B build -S . -G "Visual Studio 18" -A x64
)
exit /b 0

:build
call :configure
echo [TASK] Building zclip (%CONFIG%)...
cmake --build build --config %CONFIG%
exit /b %ERRORLEVEL%

:run
call :build
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
echo [TASK] Running zclip.exe...
.\build\%CONFIG%\zclip.exe
exit /b %ERRORLEVEL%

:format
call :configure
echo [TASK] Running clang-format...
cmake --build build --target format --config %CONFIG%
exit /b %ERRORLEVEL%

:tidy
echo [TASK] Locating clang-tidy...
where clang-tidy >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] clang-tidy executable not found in PATH.
    exit /b 1
)

echo [TASK] Running clang-tidy analysis recursively...
for %%f in (src\*.cpp) do (
    echo Linting %%f...
    clang-tidy %%f --config-file=.clang-tidy -- -std=c++23 -Iinclude -isystem "C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared" -isystem "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\include" -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    if !ERRORLEVEL! NEQ 0 exit /b !ERRORLEVEL!
)
exit /b 0

echo [TASK] Runn

:clean
echo [TASK] Cleaning build directory...
if exist build rmdir /s /q build
echo Done.
exit /b 0

:all
call :format
call :build
call :tidy
exit /b 0