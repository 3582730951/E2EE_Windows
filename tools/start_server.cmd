@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%.."
if "%PYTHON_BIN%"=="" set "PYTHON_BIN=python"
if "%MI_E2EE_SERVER_EXE%"=="" set "MI_E2EE_SERVER_EXE=%ROOT%\mi_e2ee_server.exe"

if "%~1"=="--print-menu" goto :menu
if "%~1"=="--help" goto :usage

:loop
call :menu
set "choice="
set /p choice=Select [1-5]:
if "%choice%"=="" set "choice=1"
if "%choice%"=="1" goto :start
if "%choice%"=="2" goto :configure
if "%choice%"=="3" goto :verify
if "%choice%"=="4" goto :stress
if "%choice%"=="5" exit /b 0
if /I "%choice%"=="q" exit /b 0
echo Invalid selection
goto :loop

:menu
echo MI E2EE Server Launcher
echo.
echo 1^) Start server
echo 2^) Configure then start
echo 3^) Validate configuration
echo 4^) Stress test server
echo 5^) Quit
echo.
echo Defaults: executable=mi_e2ee_server.exe, config=config\config.ini, host=127.0.0.1, port=9000
exit /b 0

:usage
call :menu
echo.
echo Interactive Windows helper.
exit /b 0

:verify
set "config=%ROOT%\config\config.ini"
set /p config=Config path [%config%]: 
if "%config%"=="" set "config=%ROOT%\config\config.ini"
"%PYTHON_BIN%" "%SCRIPT_DIR%verify_server_config.py" --config "%config%" --privacy-strict
goto :loop

:start
set "config=%ROOT%\config\config.ini"
set /p config=Config path [%config%]: 
if "%config%"=="" set "config=%ROOT%\config\config.ini"
"%MI_E2EE_SERVER_EXE%" "%config%"
goto :loop

:stress
set "host=127.0.0.1"
set "port=9000"
set /p host=Host [%host%]: 
if "%host%"=="" set "host=127.0.0.1"
set /p port=Port [%port%]: 
if "%port%"=="" set "port=9000"
"%PYTHON_BIN%" "%SCRIPT_DIR%stress_server.py" --host "%host%" --port "%port%"
goto :loop

:configure
call "%SCRIPT_DIR%configure_server.cmd"
goto :loop
