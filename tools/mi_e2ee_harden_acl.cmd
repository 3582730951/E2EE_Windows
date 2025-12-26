@echo off
setlocal enableextensions
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=%~dp0"
set "ACCOUNT=%~2"
if "%ACCOUNT%"=="" set "ACCOUNT=%USERDOMAIN%\%USERNAME%"

echo [mi_e2ee] hardening ACLs on "%TARGET%"
echo [mi_e2ee] granting full control to "%ACCOUNT%", SYSTEM, Administrators

set "rc=0"
icacls "%TARGET%" /inheritance:r /T /C >nul
if errorlevel 1 set "rc=1"
icacls "%TARGET%" /grant:r "%ACCOUNT%":(OI)(CI)F "*S-1-5-18":(OI)(CI)F "*S-1-5-32-544":(OI)(CI)F /T /C >nul
if errorlevel 1 set "rc=1"
icacls "%TARGET%" /remove:g "*S-1-5-32-545" "*S-1-5-32-546" "*S-1-5-11" "*S-1-1-0" /T /C >nul
if errorlevel 1 set "rc=1"

if not "%rc%"=="0" (
  echo [mi_e2ee] ACL harden completed with warnings.
  exit /b 1
)

echo [mi_e2ee] ACL harden done.
exit /b 0
