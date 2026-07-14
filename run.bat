@echo off
setlocal

set "ROOT=%~dp0"
set "START_SCRIPT=%ROOT%scripts\windows\start-llama-lan.ps1"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "LOCAL_URL=http://127.0.0.1:8080/"

if not exist "%START_SCRIPT%" (
  echo Missing launcher script: %START_SCRIPT%
  exit /b 1
)

echo Starting DMC local server...
"%POWERSHELL%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%START_SCRIPT%" %*
if errorlevel 1 (
  echo DMC start failed.
  exit /b %errorlevel%
)

echo %* | findstr /I /C:"-DryRun" >nul
if not errorlevel 1 (
  echo Dry run complete.
  exit /b 0
)

echo Opening chat UI: %LOCAL_URL%
start "" "%LOCAL_URL%"
