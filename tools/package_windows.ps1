param(
  [string]$Workspace,
  [string]$Dist
)

$ErrorActionPreference = "Stop"

function Resolve-Workspace([string]$root) {
  if ($root) {
    return (Resolve-Path $root).Path
  }
  if ($env:GITHUB_WORKSPACE) {
    return (Resolve-Path $env:GITHUB_WORKSPACE).Path
  }
  $scriptDir = $PSScriptRoot
  if (-not $scriptDir -and $PSCommandPath) {
    $scriptDir = Split-Path -Parent $PSCommandPath
  }
  if (-not $scriptDir -and $MyInvocation.MyCommand.Path) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
  }
  if (-not $scriptDir) {
    $scriptDir = (Get-Location).Path
  }
  return (Resolve-Path (Join-Path $scriptDir "..")).Path
}

function Find-File([string]$root, [string]$pattern) {
  $item = Get-ChildItem -Path $root -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($item) {
    return $item.FullName
  }
  return $null
}

function Ensure-Dir([string]$path) {
  New-Item -ItemType Directory -Force -Path $path | Out-Null
}

function Write-Manifest([string]$root) {
  $rootFull = (Resolve-Path $root).Path
  $manifest = Join-Path $rootFull "manifest.sha256"
  $lines = @()
  Get-ChildItem -Path $rootFull -File -Recurse | Where-Object { $_.FullName -ne $manifest } | ForEach-Object {
    $rel = $_.FullName.Substring($rootFull.Length).TrimStart("\", "/")
    $hash = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower()
    $lines += "$hash  $rel"
  }
  $lines | Set-Content -Path $manifest -Encoding ASCII
}

$workspace = Resolve-Workspace $Workspace
if ($Dist) {
  $distRoot = (Resolve-Path $Dist).Path
} else {
  $distRoot = Join-Path $workspace "dist"
}

$clientRoot = Join-Path $distRoot "mi_e2ee_client"
$serverRoot = Join-Path $distRoot "mi_e2ee_server"
$clientDll = Join-Path $clientRoot "dll"
$clientConfig = Join-Path $clientRoot "config"
$clientDb = Join-Path $clientRoot "database"
$clientSdk = Join-Path $clientRoot "sdk"
$clientBindings = Join-Path $clientRoot "bindings"
$clientBindingsPy = Join-Path $clientBindings "python"
$clientBindingsRust = Join-Path $clientBindings "rust"
$serverDll = Join-Path $serverRoot "dll"
$serverConfig = Join-Path $serverRoot "config"
$serverDb = Join-Path $serverRoot "database"
$serverTools = Join-Path $serverRoot "tools"

Ensure-Dir $clientDll
Ensure-Dir $clientConfig
Ensure-Dir $clientDb
Ensure-Dir $clientSdk
Ensure-Dir $clientBindingsPy
Ensure-Dir $clientBindingsRust
Ensure-Dir $serverDll
Ensure-Dir $serverConfig
Ensure-Dir $serverDb
Ensure-Dir $serverTools
Ensure-Dir (Join-Path $serverDb "offline_store")

$prebuiltSrc = Join-Path $workspace "build\\rime_prebuilt\\user"
$prebuiltDst = Join-Path $clientDb "rime\\prebuilt"
if (Test-Path $prebuiltSrc) {
  Ensure-Dir $prebuiltDst
  Copy-Item (Join-Path $prebuiltSrc "*") $prebuiltDst -Recurse -Force
}
$overlayShare = Join-Path $workspace "build\\rime_overlay\\share"
$overlayDst = Join-Path $clientDb "rime\\share"
if (Test-Path $overlayShare) {
  Ensure-Dir $overlayDst
  Copy-Item (Join-Path $overlayShare "*") $overlayDst -Recurse -Force
}

$keysDir = Join-Path $distRoot "keys"
Ensure-Dir $keysDir
$ktKeygen = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_kt_keygen.exe"
if (-not $ktKeygen) {
  throw "mi_e2ee_kt_keygen.exe not found under build/server"
}
& $ktKeygen --out-dir $keysDir --force

$pfxPath = Join-Path $keysDir "mi_e2ee_server.pfx"
$cert = New-SelfSignedCertificate -DnsName "MI_E2EE_Server" -CertStoreLocation "Cert:\\CurrentUser\\My"
$pwd = New-Object System.Security.SecureString
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pwd | Out-Null
$cert = Get-PfxCertificate -FilePath $pfxPath
$der = $cert.Export("Cert")
$hash = [System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash($der)).Replace("-", "").ToLower()

$serverExe = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_server.exe"
$serverLauncher = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_server_launcher.exe"
if (-not $serverExe -or -not $serverLauncher) {
  throw "server executables not found under build/server"
}
Copy-Item $serverExe (Join-Path $serverRoot "mi_e2ee_server_app.exe") -Force
Copy-Item $serverLauncher (Join-Path $serverRoot "mi_e2ee_server.exe") -Force
Copy-Item (Join-Path (Split-Path $serverExe -Parent) "*.dll") $serverDll -Force

$crtNames = @(
  "concrt140.dll",
  "msvcp140.dll",
  "msvcp140_1.dll",
  "msvcp140_2.dll",
  "msvcp140_atomic_wait.dll",
  "msvcp140_codecvt_ids.dll",
  "vcruntime140.dll",
  "vcruntime140_1.dll",
  "vcruntime140_threads.dll",
  "vccorlib140.dll"
)
foreach ($name in $crtNames) {
  $src = Join-Path $serverDll $name
  if (Test-Path $src) {
    Copy-Item $src $serverRoot -Force
  }
}
Copy-Item (Join-Path $workspace "tools\\mi_e2ee_harden_acl.cmd") (Join-Path $serverRoot "mi_e2ee_harden_acl.cmd") -Force
$demoUsers = Find-File (Join-Path $workspace "build\\server") "test_user.txt"
if (-not $demoUsers) {
  throw "test_user.txt not found under build/server"
}
Copy-Item $demoUsers $serverRoot -Force

$ktPubinfo = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_kt_pubinfo.exe"
$perfBaseline = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_perf_baseline.exe"
$opsHealth = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_ops_health_view.exe"
$thirdPartyAudit = Find-File (Join-Path $workspace "build\\server") "mi_e2ee_third_party_audit.exe"
if (-not $ktPubinfo) {
  throw "mi_e2ee_kt_pubinfo.exe not found under build/server"
}
if (-not $perfBaseline) {
  throw "mi_e2ee_perf_baseline.exe not found under build/server"
}
if (-not $opsHealth) {
  throw "mi_e2ee_ops_health_view.exe not found under build/server"
}
if (-not $thirdPartyAudit) {
  throw "mi_e2ee_third_party_audit.exe not found under build/server"
}
Copy-Item $ktKeygen $serverTools -Force
Copy-Item $ktPubinfo $serverTools -Force
Copy-Item $perfBaseline $serverTools -Force
Copy-Item $opsHealth $serverTools -Force
Copy-Item $thirdPartyAudit $serverTools -Force

Copy-Item (Join-Path $keysDir "kt_signing_key.bin") $serverConfig -Force
Copy-Item (Join-Path $keysDir "kt_root_pub.bin") $serverConfig -Force
Copy-Item $pfxPath $serverConfig -Force
Copy-Item (Join-Path $keysDir "kt_root_pub.bin") $clientConfig -Force

$serverConfigLines = @(
  "[mode]",
  "mode=0",
  "[mysql]",
  "mysql_ip=localhost",
  "mysql_port=3306",
  "mysql_database=mi_e2ee",
  "mysql_username=root",
  "mysql_password=123456",
  "[server]",
  "list_port=9000",
  "rotation_threshold=10000",
  "offline_dir=database/offline_store",
  "debug_log=0",
  "tls_enable=1",
  "require_tls=1",
  "tls_cert=config/mi_e2ee_server.pfx",
  "kt_signing_key=kt_signing_key.bin"
)
$serverConfigLines | Set-Content -Path (Join-Path $serverConfig "config.ini") -Encoding ASCII

$clientConfigLines = @(
  "[client]",
  "server_ip=127.0.0.1",
  "server_port=9000",
  "use_tls=1",
  "require_tls=1",
  "trust_store=server_trust.ini",
  "require_pinned_fingerprint=1",
  "pinned_fingerprint=$hash",
  "auth_mode=opaque",
  "",
  "[proxy]",
  "type=none",
  "host=",
  "port=0",
  "username=",
  "password=",
  "",
  "[device_sync]",
  "enabled=0",
  "role=primary",
  "key_path=e2ee_state/device_sync_key.bin",
  "",
  "[kt]",
  "require_signature=1",
  "root_pubkey_path=kt_root_pub.bin"
)
$clientConfigLines | Set-Content -Path (Join-Path $clientConfig "client_config.ini") -Encoding ASCII
"" | Set-Content -Path (Join-Path $clientDb "server_trust.ini") -Encoding ASCII

$uiRoot = Join-Path $workspace "build\\client\\ui\\Release"
Copy-Item (Join-Path $uiRoot "mi_e2ee_client_ui_app.exe") $clientRoot -Force
Copy-Item (Join-Path $uiRoot "mi_e2ee_client_ui.exe") $clientRoot -Force
Copy-Item (Join-Path $uiRoot "mi_e2ee.exe") $clientRoot -Force

$sdkDll = Find-File (Join-Path $workspace "build\\client") "mi_e2ee_client_sdk.dll"
if ($sdkDll) {
  Copy-Item $sdkDll $clientRoot -Force
  Copy-Item $sdkDll $clientDll -Force
}

$uiRcc = Join-Path $uiRoot "ui_resources.rcc"
if (Test-Path $uiRcc) {
  Copy-Item $uiRcc (Join-Path $clientDll "ui_resources.rcc") -Force
}
Get-ChildItem -Path $uiRoot -File -Filter "*.dll" |
  Where-Object { $_.Name -notlike "qmldbg_*" } | ForEach-Object {
    Copy-Item $_.FullName $clientDll -Force
  }

$rimeRuntime = $env:RIME_RUNTIME_DIR
if (-not $rimeRuntime) {
  $rimeRuntime = Join-Path $workspace "build\\rime_runtime"
}
if (Test-Path $rimeRuntime) {
  Get-ChildItem -Path $rimeRuntime -File -Filter "*.dll" | ForEach-Object {
    Copy-Item $_.FullName $clientDll -Force
  }
}
if (Test-Path (Join-Path $rimeRuntime "opencc")) {
  Copy-Item (Join-Path $rimeRuntime "opencc") (Join-Path $clientDb "opencc") -Recurse -Force
}

$ffmpegRuntime = $env:FFMPEG_RUNTIME_DIR
if (-not $ffmpegRuntime) {
  $ffmpegExe = Find-File (Join-Path $workspace "build\\ffmpeg") "ffmpeg.exe"
  if ($ffmpegExe) {
    $ffmpegRuntime = Split-Path $ffmpegExe -Parent
  }
}
if ($ffmpegRuntime -and (Test-Path $ffmpegRuntime)) {
  Get-ChildItem -Path $ffmpegRuntime -File | ForEach-Object {
    Copy-Item $_.FullName $clientRoot -Force
  }
}

$esrganRuntime = $env:REALESRGAN_RUNTIME_DIR
$esrganModels = $env:REALESRGAN_MODEL_DIR
if ($esrganRuntime -and (Test-Path $esrganRuntime)) {
  $esrganDst = Join-Path $clientRoot "tools\\realesrgan"
  Ensure-Dir $esrganDst
  Get-ChildItem -Path $esrganRuntime -File | ForEach-Object {
    Copy-Item $_.FullName $esrganDst -Force
  }
}
if ($esrganModels -and (Test-Path $esrganModels)) {
  $modelDst = Join-Path $clientDb "ai_models\\realesrgan"
  Ensure-Dir $modelDst
  Get-ChildItem -Path $esrganModels -File -Filter "*.param" | ForEach-Object {
    Copy-Item $_.FullName $modelDst -Force
  }
  Get-ChildItem -Path $esrganModels -File -Filter "*.bin" | ForEach-Object {
    Copy-Item $_.FullName $modelDst -Force
  }
}

$skipDirs = @("qml", "translations", "icon", "assets", "runtime", "qmltooling")
Get-ChildItem -Path $uiRoot -Directory | Where-Object { $skipDirs -notcontains $_.Name } | ForEach-Object {
  Copy-Item $_.FullName $clientDll -Recurse -Force
}
if (Test-Path (Join-Path $uiRoot "qml")) {
  Copy-Item (Join-Path $uiRoot "qml") (Join-Path $clientDll "qml") -Recurse -Force
}
if (Test-Path (Join-Path $uiRoot "translations")) {
  Copy-Item (Join-Path $uiRoot "translations") (Join-Path $clientRoot "translations") -Recurse -Force
}
if (Test-Path (Join-Path $uiRoot "icon")) {
  Copy-Item (Join-Path $uiRoot "icon") (Join-Path $clientRoot "icon") -Recurse -Force
}
if (Test-Path (Join-Path $uiRoot "assets")) {
  Copy-Item (Join-Path $uiRoot "assets") (Join-Path $clientRoot "assets") -Recurse -Force
}
$sourceIcons = Join-Path $workspace "client\\ui\\icons"
if (Test-Path $sourceIcons) {
  Copy-Item $sourceIcons (Join-Path $clientRoot "icon") -Recurse -Force
}
$sourceRef = Join-Path $workspace "client\\assets\\ref"
if (Test-Path $sourceRef) {
  Copy-Item $sourceRef (Join-Path $clientRoot "assets\\ref") -Recurse -Force
}
if (Test-Path (Join-Path $uiRoot "runtime")) {
  $runtimeRoot = Join-Path $uiRoot "runtime"
  Get-ChildItem -Path $runtimeRoot -File | ForEach-Object {
    if ($_.Extension -ieq ".dll" -and $_.Name -notlike "qmldbg_*") {
      Copy-Item $_.FullName $clientDll -Force
    } else {
      Copy-Item $_.FullName $clientDb -Force
    }
  }
  Get-ChildItem -Path $runtimeRoot -Directory | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $clientDb $_.Name) -Recurse -Force
  }
}

$imeRoot = Join-Path $workspace "build\\client\\ui\\ime_rime\\Release"
$imeCandidates = @(
  (Join-Path $workspace "build\\client\\ui\\Release\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\Release\\runtime\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\ime_rime\\Release\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ime_rime\\Release\\mi_ime_rime.dll")
)
$imeDllPath = $imeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $imeDllPath) {
  $imeDllPath = Find-File (Join-Path $workspace "build\\client") "mi_ime_rime.dll"
}
if ($imeDllPath) {
  Copy-Item $imeDllPath $clientDll -Force
  Copy-Item $imeDllPath $clientRoot -Force
}

$rimeCandidates = @(
  (Join-Path $imeRoot "rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\Release\\runtime\\rime.dll")
)
$rimeDllPath = $rimeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($rimeDllPath) {
  Copy-Item $rimeDllPath $clientDll -Force
}
if (Test-Path (Join-Path $imeRoot "opencc")) {
  Copy-Item (Join-Path $imeRoot "opencc") (Join-Path $clientDb "opencc") -Recurse -Force
}

Get-ChildItem -Path $clientRoot -Recurse -Filter "qmldbg_*.dll" | Remove-Item -Force

Copy-Item (Join-Path $workspace "sdk\\c_api_client.h") $clientSdk -Force
Copy-Item (Join-Path $workspace "bindings\\python\\mi_e2ee_client.py") $clientBindingsPy -Force
Copy-Item (Join-Path $workspace "bindings\\python\\example_basic.py") $clientBindingsPy -Force
Copy-Item (Join-Path $workspace "bindings\\rust\\Cargo.toml") $clientBindingsRust -Force
Copy-Item (Join-Path $workspace "bindings\\rust\\Cargo.lock") $clientBindingsRust -Force
Copy-Item (Join-Path $workspace "bindings\\rust\\build.rs") $clientBindingsRust -Force
Copy-Item (Join-Path $workspace "bindings\\rust\\src") $clientBindingsRust -Recurse -Force
Copy-Item (Join-Path $workspace "bindings\\rust\\examples") $clientBindingsRust -Recurse -Force
if (Test-Path (Join-Path $workspace "bindings\\README.md")) {
  Copy-Item (Join-Path $workspace "bindings\\README.md") $clientBindings -Force
}

Write-Manifest $clientRoot
Write-Manifest $serverRoot

$clientZip = Join-Path $distRoot "mi_e2ee_client.zip"
$serverZip = Join-Path $distRoot "mi_e2ee_server.zip"
Compress-Archive -Path $clientRoot -DestinationPath $clientZip -Force
Compress-Archive -Path $serverRoot -DestinationPath $serverZip -Force
