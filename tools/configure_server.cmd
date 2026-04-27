@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%.."
set "CONFIG_DIR=%ROOT%\config"
set "CONFIG_PATH=%CONFIG_DIR%\config.ini"
set "VERIFY_SCRIPT=%SCRIPT_DIR%verify_server_config.py"
if "%PYTHON_BIN%"=="" set "PYTHON_BIN=python"

if "%~1"=="--print-menu" goto :menu
if "%~1"=="--help" goto :usage

:loop
call :menu
set "choice="
set /p choice=Select [1-9]: 
if "%choice%"=="" set "choice=9"
if "%choice%"=="1" goto :first_time
if "%choice%"=="2" goto :reconfigure
if "%choice%"=="3" goto :generate_cert
if "%choice%"=="4" goto :import_cert
if "%choice%"=="5" goto :rotate_pin
if "%choice%"=="6" goto :kt_menu
if "%choice%"=="7" goto :validate
if "%choice%"=="8" goto :showcert
if "%choice%"=="9" exit /b 0
if /I "%choice%"=="q" exit /b 0
echo Invalid selection
goto :loop

:menu
echo MI E2EE Server Configuration
echo.
echo 1^) First-time setup
echo 2^) Reconfigure server
echo 3^) Generate self-signed certificate and pin
echo 4^) Import CA/server certificate
echo 5^) Rotate client pinned fingerprint
echo 6^) Initialize / rotate Key Transparency key
echo 7^) Validate current configuration
echo 8^) Show current certificate fingerprint and SAS
echo 9^) Exit
echo.
exit /b 0

:usage
call :menu
echo.
echo Interactive Windows helper. It writes only under the selected server/client config directories.
exit /b 0

:ensure_dirs
if not exist "%CONFIG_DIR%" mkdir "%CONFIG_DIR%"
if not exist "%ROOT%\database\offline_store" mkdir "%ROOT%\database\offline_store"
exit /b 0

:first_time
call :ensure_dirs
set "config=%CONFIG_PATH%"
set /p config=Config path [%config%]: 
if "%config%"=="" set "config=%CONFIG_PATH%"
echo Auth mode:
echo 1^) Demo mode
echo 2^) MySQL mode
set /p auth_choice=Select [1-2]: 
if "%auth_choice%"=="2" goto :setup_mysql
goto :setup_demo

:setup_demo
set "listen_port=9000"
set "offline_dir=database/offline_store"
set /p listen_port=Server listen port [%listen_port%]: 
if "%listen_port%"=="" set "listen_port=9000"
set /p offline_dir=Offline store directory [%offline_dir%]: 
if "%offline_dir%"=="" set "offline_dir=database/offline_store"
call :generate_kt_if_missing
> "%config%" (
  echo [mode]
  echo mode=1
  echo [server]
  echo list_port=%listen_port%
  echo rotation_threshold=10000
  echo offline_dir=%offline_dir%
  echo debug_log=0
  echo offline_blob_temp_budget_bytes=4294967296
  echo tls_enable=1
  echo require_tls=1
  echo tls_cert=config/mi_e2ee_server.pfx
  echo kt_signing_key=kt_signing_key.bin
  echo state_protection=none
  echo ops_enable=0
  echo ops_allow_remote=0
  echo [kcp]
  echo enable=0
  echo allow_insecure=0
  echo listen_port=0
)
call :generate_cert_no_loop
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%config%" --privacy-strict
goto :loop

:setup_mysql
set "listen_port=9000"
set "offline_dir=database/offline_store"
set "mysql_host=127.0.0.1"
set "mysql_port=3306"
set "mysql_db=mi_e2ee"
set /p listen_port=Server listen port [%listen_port%]: 
if "%listen_port%"=="" set "listen_port=9000"
set /p offline_dir=Offline store directory [%offline_dir%]: 
if "%offline_dir%"=="" set "offline_dir=database/offline_store"
set /p mysql_host=MySQL host [%mysql_host%]: 
if "%mysql_host%"=="" set "mysql_host=127.0.0.1"
set /p mysql_port=MySQL port [%mysql_port%]: 
if "%mysql_port%"=="" set "mysql_port=3306"
set /p mysql_db=MySQL database [%mysql_db%]: 
if "%mysql_db%"=="" set "mysql_db=mi_e2ee"
set /p mysql_user=MySQL username: 
set /p mysql_password=MySQL password: 
call :generate_kt_if_missing
> "%config%" (
  echo [mode]
  echo mode=0
  echo [mysql]
  echo mysql_ip=%mysql_host%
  echo mysql_port=%mysql_port%
  echo mysql_database=%mysql_db%
  echo mysql_username=%mysql_user%
  echo mysql_password=%mysql_password%
  echo [server]
  echo list_port=%listen_port%
  echo rotation_threshold=10000
  echo offline_dir=%offline_dir%
  echo debug_log=0
  echo offline_blob_temp_budget_bytes=4294967296
  echo tls_enable=1
  echo require_tls=1
  echo tls_cert=config/mi_e2ee_server.pfx
  echo kt_signing_key=kt_signing_key.bin
  echo state_protection=none
  echo ops_enable=0
  echo ops_allow_remote=0
  echo [kcp]
  echo enable=0
  echo allow_insecure=0
  echo listen_port=0
)
call :generate_cert_no_loop
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%config%" --privacy-strict
goto :loop

:reconfigure
if not exist "%CONFIG_PATH%" (
  echo No config found; running first-time setup.
  goto :first_time
)
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMddHHmmss"') do set "stamp=%%i"
copy /Y "%CONFIG_PATH%" "%CONFIG_PATH%.bak.%stamp%" > nul
echo Reconfigure:
echo 1^) Auth mode
echo 2^) Listen port
echo 3^) Offline directory
echo 4^) TLS certificate
echo 5^) Ops health
echo 6^) Reset first-time setup
echo 7^) Back
set /p reconfig_choice=Select [1-7]: 
if "%reconfig_choice%"=="1" goto :reconfigure_auth
if "%reconfig_choice%"=="2" goto :reconfigure_port
if "%reconfig_choice%"=="3" goto :reconfigure_offline
if "%reconfig_choice%"=="4" goto :reconfigure_tls
if "%reconfig_choice%"=="5" goto :reconfigure_ops
if "%reconfig_choice%"=="6" goto :first_time
goto :loop

:reconfigure_auth
echo Auth mode:
echo 1^) Demo mode
echo 2^) MySQL mode
set /p mode_choice=Select [1-2]: 
if "%mode_choice%"=="2" (
  set /p mysql_host=MySQL host [127.0.0.1]: 
  if "%mysql_host%"=="" set "mysql_host=127.0.0.1"
  set /p mysql_port=MySQL port [3306]: 
  if "%mysql_port%"=="" set "mysql_port=3306"
  set /p mysql_db=MySQL database [mi_e2ee]: 
  if "%mysql_db%"=="" set "mysql_db=mi_e2ee"
  set /p mysql_user=MySQL username: 
  set /p mysql_password=MySQL password: 
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $text=Get-Content $p -Raw; $text=$text -replace '(?ms)^\[mode\].*?(?=^\[|$)', \"[mode]`nmode=0`n\"; if($text -notmatch '(?m)^\[mode\]'){ $text=\"[mode]`nmode=0`n\"+$text }; $mysql=\"[mysql]`nmysql_ip=%mysql_host%`nmysql_port=%mysql_port%`nmysql_database=%mysql_db%`nmysql_username=%mysql_user%`nmysql_password=%mysql_password%`n\"; if($text -match '(?ms)^\[mysql\].*?(?=^\[|$)'){ $text=$text -replace '(?ms)^\[mysql\].*?(?=^\[|$)', $mysql } else { $text=$text -replace '(?m)^\[server\]', $mysql+'[server]' }; Set-Content -Encoding utf8 $p $text"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $text=Get-Content $p -Raw; $text=$text -replace '(?ms)^\[mode\].*?(?=^\[|$)', \"[mode]`nmode=1`n\"; $text=$text -replace '(?ms)^\[mysql\].*?(?=^\[|$)', ''; Set-Content -Encoding utf8 $p $text"
)
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%CONFIG_PATH%" --privacy-strict
goto :loop

:reconfigure_port
set /p listen_port=Server listen port [9000]: 
if "%listen_port%"=="" set "listen_port=9000"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $text=Get-Content $p -Raw; $text=$text -replace '(?m)^list_port=.*$', 'list_port=%listen_port%'; Set-Content -Encoding utf8 $p $text"
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%CONFIG_PATH%" --privacy-strict
goto :loop

:reconfigure_offline
set /p offline_dir=Offline store directory [database/offline_store]: 
if "%offline_dir%"=="" set "offline_dir=database/offline_store"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $text=Get-Content $p -Raw; $text=$text -replace '(?m)^offline_dir=.*$', 'offline_dir=%offline_dir%'; Set-Content -Encoding utf8 $p $text"
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%CONFIG_PATH%" --privacy-strict
goto :loop

:reconfigure_tls
echo TLS certificate:
echo 1^) Generate self-signed certificate
echo 2^) Import existing certificate
set /p tls_choice=Select [1-2]: 
if "%tls_choice%"=="2" goto :import_cert
goto :generate_cert

:reconfigure_ops
echo Ops health:
echo 1^) Disable
echo 2^) Enable, loopback only
echo 3^) Enable, allow remote
set /p ops_choice=Select [1-3]: 
if "%ops_choice%"=="2" (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $token=[Guid]::NewGuid().ToString('N')+[Guid]::NewGuid().ToString('N'); $text=Get-Content $p -Raw; $text=$text -replace '(?m)^ops_enable=.*$', 'ops_enable=1'; $text=$text -replace '(?m)^ops_allow_remote=.*$', 'ops_allow_remote=0'; if($text -match '(?m)^ops_token='){ $text=$text -replace '(?m)^ops_token=.*$', 'ops_token='+$token } else { $text=$text -replace '(?m)^\[kcp\]', 'ops_token='+$token+\"`n[kcp]\" }; Set-Content -Encoding utf8 $p $text"
) else if "%ops_choice%"=="3" (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $token=[Guid]::NewGuid().ToString('N')+[Guid]::NewGuid().ToString('N'); $text=Get-Content $p -Raw; $text=$text -replace '(?m)^ops_enable=.*$', 'ops_enable=1'; $text=$text -replace '(?m)^ops_allow_remote=.*$', 'ops_allow_remote=1'; if($text -match '(?m)^ops_token='){ $text=$text -replace '(?m)^ops_token=.*$', 'ops_token='+$token } else { $text=$text -replace '(?m)^\[kcp\]', 'ops_token='+$token+\"`n[kcp]\" }; Set-Content -Encoding utf8 $p $text"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $text=Get-Content $p -Raw; $text=$text -replace '(?m)^ops_enable=.*$', 'ops_enable=0'; $text=$text -replace '(?m)^ops_allow_remote=.*$', 'ops_allow_remote=0'; $text=$text -replace '(?m)^ops_token=.*\r?\n?', ''; Set-Content -Encoding utf8 $p $text"
)
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%CONFIG_PATH%" --privacy-strict
goto :loop

:generate_cert
call :generate_cert_no_loop
call :set_server_tls_cert "config/mi_e2ee_server.pfx"
call :configure_client_tls "%CONFIG_DIR%\mi_e2ee_server.pfx"
goto :loop

:generate_cert_no_loop
call :ensure_dirs
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $rsa=[System.Security.Cryptography.RSA]::Create(2048); $req=[System.Security.Cryptography.X509Certificates.CertificateRequest]::new('CN=MI_E2EE_Server',$rsa,[System.Security.Cryptography.HashAlgorithmName]::SHA256,[System.Security.Cryptography.RSASignaturePadding]::Pkcs1); $cert=$req.CreateSelfSigned([DateTimeOffset]::UtcNow.AddDays(-1),[DateTimeOffset]::UtcNow.AddYears(2)); $pfx=$cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx,''); [IO.File]::WriteAllBytes('%CONFIG_DIR%\mi_e2ee_server.pfx',$pfx); $der=$cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert); $hash=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($der)).Replace('-','').ToLower(); Write-Host sha256=$hash"
exit /b %ERRORLEVEL%

:import_cert
call :ensure_dirs
echo Certificate format:
echo 1^) PEM fullchain + private key
echo 2^) Combined PEM
echo 3^) PFX / P12
set /p cert_format=Select [1-3]: 
if "%cert_format%"=="1" (
  set /p cert_path=Certificate/fullchain path: 
  set /p key_path=Private key path: 
  copy /Y "%key_path%" "%CONFIG_DIR%\mi_e2ee_server.pem" > nul
  type "%cert_path%" >> "%CONFIG_DIR%\mi_e2ee_server.pem"
  call :set_server_tls_cert "config/mi_e2ee_server.pem"
  "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --show-cert "%CONFIG_DIR%\mi_e2ee_server.pem"
  call :configure_client_tls "%CONFIG_DIR%\mi_e2ee_server.pem"
) else if "%cert_format%"=="3" (
  set /p pfx_path=PFX/P12 path: 
  set /p pfx_password=PFX password: 
  copy /Y "%pfx_path%" "%CONFIG_DIR%\mi_e2ee_server.pfx" > nul
  call :set_server_tls_cert "config/mi_e2ee_server.pfx"
  "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --show-cert "%CONFIG_DIR%\mi_e2ee_server.pfx"
  call :configure_client_tls "%CONFIG_DIR%\mi_e2ee_server.pfx"
) else (
  set /p pem_path=Combined PEM path: 
  copy /Y "%pem_path%" "%CONFIG_DIR%\mi_e2ee_server.pem" > nul
  call :set_server_tls_cert "config/mi_e2ee_server.pem"
  "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --show-cert "%CONFIG_DIR%\mi_e2ee_server.pem"
  call :configure_client_tls "%CONFIG_DIR%\mi_e2ee_server.pem"
)
goto :loop

:set_server_tls_cert
if not exist "%CONFIG_PATH%" exit /b 0
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p='%CONFIG_PATH%'; $cert='%~1'; $text=Get-Content $p -Raw; $text=$text -replace '(?m)^tls_enable=.*$', 'tls_enable=1'; $text=$text -replace '(?m)^require_tls=.*$', 'require_tls=1'; $text=$text -replace '(?m)^tls_cert=.*$', 'tls_cert='+$cert; Set-Content -Encoding utf8 $p $text"
exit /b 0

:configure_client_tls
echo Client TLS verification mode:
echo 1^) Pin only
echo 2^) CA only
echo 3^) Hybrid CA + pin
echo 4^) Skip client update
set /p client_mode=Select [1-4]: 
if "%client_mode%"=="" exit /b 0
if "%client_mode%"=="4" exit /b 0
set /p client_config=Client config path: 
if "%client_config%"=="" exit /b 0
if "%client_mode%"=="1" "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --client-config "%client_config%" --set-client-tls pin --cert "%~1"
if "%client_mode%"=="2" (
  set /p ca_bundle=CA bundle path [%~1]: 
  if "%ca_bundle%"=="" set "ca_bundle=%~1"
  "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --client-config "%client_config%" --set-client-tls ca --tls-ca-bundle-path "%ca_bundle%"
)
if "%client_mode%"=="3" (
  set /p ca_bundle=CA bundle path [%~1]: 
  if "%ca_bundle%"=="" set "ca_bundle=%~1"
  "%PYTHON_BIN%" "%VERIFY_SCRIPT%" --client-config "%client_config%" --set-client-tls hybrid --cert "%~1" --tls-ca-bundle-path "%ca_bundle%"
)
exit /b 0

:rotate_pin
set "server_cert=%CONFIG_DIR%\mi_e2ee_server.pfx"
set /p server_cert=Server certificate path [%server_cert%]: 
if "%server_cert%"=="" set "server_cert=%CONFIG_DIR%\mi_e2ee_server.pfx"
set /p client_config=Client config path: 
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --client-config "%client_config%" --cert "%server_cert%"
goto :loop

:kt_menu
echo KT key action:
echo 1^) Generate if missing
echo 2^) Rotate existing key
echo 3^) Back
set /p kt_choice=Select [1-3]: 
if "%kt_choice%"=="3" goto :loop
if "%kt_choice%"=="2" goto :kt_rotate
call :generate_kt_if_missing
goto :kt_copy_prompt

:kt_rotate
set /p confirm=Type OVERWRITE to continue: 
if not "%confirm%"=="OVERWRITE" goto :loop
call :generate_kt_force
goto :kt_copy_prompt

:generate_kt_if_missing
if exist "%CONFIG_DIR%\kt_signing_key.bin" if exist "%CONFIG_DIR%\kt_root_pub.bin" exit /b 0
call :generate_kt_force
exit /b 0

:generate_kt_force
call :ensure_dirs
if exist "%ROOT%\tools\mi_e2ee_kt_keygen.exe" (
  "%ROOT%\tools\mi_e2ee_kt_keygen.exe" --out-dir "%CONFIG_DIR%" --force
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$d='%CONFIG_DIR%'; New-Item -ItemType Directory -Force -Path $d | Out-Null; $rng=[Security.Cryptography.RandomNumberGenerator]::Create(); $sk=New-Object byte[] 4096; $pk=New-Object byte[] 1952; $rng.GetBytes($sk); $rng.GetBytes($pk); [IO.File]::WriteAllBytes((Join-Path $d 'kt_signing_key.bin'), $sk); [IO.File]::WriteAllBytes((Join-Path $d 'kt_root_pub.bin'), $pk)"
)
exit /b 0

:kt_copy_prompt
echo Copy kt_root_pub.bin to client config directory?
echo 1^) Yes
echo 2^) No
set /p copy_choice=Select [1-2]: 
if "%copy_choice%"=="1" (
  set /p client_dir=Client config directory: 
  if not exist "%client_dir%" mkdir "%client_dir%"
  copy /Y "%CONFIG_DIR%\kt_root_pub.bin" "%client_dir%\kt_root_pub.bin" > nul
)
goto :loop

:validate
set "config=%CONFIG_PATH%"
set /p config=Config path [%config%]: 
if "%config%"=="" set "config=%CONFIG_PATH%"
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --config "%config%" --privacy-strict
goto :loop

:showcert
set "server_cert=%CONFIG_DIR%\mi_e2ee_server.pfx"
set /p server_cert=Server certificate path [%server_cert%]: 
if "%server_cert%"=="" set "server_cert=%CONFIG_DIR%\mi_e2ee_server.pfx"
"%PYTHON_BIN%" "%VERIFY_SCRIPT%" --show-cert "%server_cert%"
goto :loop
