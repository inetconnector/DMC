@echo off
setlocal

set "ROOT=%~dp0"
set "SCRIPT=%ROOT%scripts\windows\manage-knowledge.ps1"
set "POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if not exist "%SCRIPT%" (
  echo Missing knowledge manager: %SCRIPT%
  exit /b 1
)

"%POWERSHELL%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %errorlevel%
