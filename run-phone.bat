@echo off
setlocal

set "ROOT=%~dp0"
set "FIREWALL_SCRIPT=%ROOT%scripts\windows\open-llama-lan-8080.ps1"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "RUN_SCRIPT=%ROOT%run.bat"

if not exist "%FIREWALL_SCRIPT%" (
  echo Missing firewall script: %FIREWALL_SCRIPT%
  exit /b 1
)

if not exist "%RUN_SCRIPT%" (
  echo Missing launcher script: %RUN_SCRIPT%
  exit /b 1
)

echo %* | findstr /I /C:"-DryRun" >nul
if errorlevel 1 (
  echo Preparing LAN access...
  "%POWERSHELL%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%FIREWALL_SCRIPT%"
  if errorlevel 1 (
    echo LAN setup failed.
    exit /b %errorlevel%
  )
) else (
  echo Dry run requested, skipping LAN firewall setup.
)

echo Starting DMC for phone access...
echo %* | findstr /I /C:"-Use64KContext" /C:"-Use128KContext" /C:"-Use256KContext" /C:"-ContextSize" >nul
if errorlevel 1 (
  call "%RUN_SCRIPT%" -Use64KContext %*
) else (
  call "%RUN_SCRIPT%" %*
)
