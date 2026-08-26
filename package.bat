@echo off
setlocal
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

if not exist build\%CONFIG%\zclip.exe (
    echo [PACKAGE] Building %CONFIG% first...
    call task.bat build %CONFIG%
    if errorlevel 1 exit /b %errorlevel%
)

if not exist dist mkdir dist
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Compress-Archive -Path 'build\%CONFIG%\zclip.exe','PAIRING.txt','README.md','LICENSE' -DestinationPath 'dist\zclip.zip' -Force"
if errorlevel 1 exit /b %errorlevel%

echo [PACKAGE] Created dist\zclip.zip
endlocal