@echo off
setlocal
cd /d "%~dp0"

echo Setting up the project-local Windows development tools...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup.ps1"
set "setup_exit=%ERRORLEVEL%"

if not "%setup_exit%"=="0" (
  echo.
  echo Setup did not complete. Read the newest log under .setup\logs\ for details.
  pause
)

exit /b %setup_exit%
