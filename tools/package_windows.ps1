param(
  [string]$Workspace,
  [string]$Dist,
  [string]$BuildConfig = "Release",
  [string]$MysqlUsername,
  [string]$MysqlPassword
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

function Normalize-BuildConfig([string]$value) {
  switch -Regex ($value) {
    '^(?i)release$' { return "Release" }
    '^(?i)relwithdebinfo$' { return "RelWithDebInfo" }
    '^(?i)minsizerel$' { return "MinSizeRel" }
    default { throw "unsupported BuildConfig: $value (allowed: Release, RelWithDebInfo, MinSizeRel)" }
  }
}

function Assert-NotDebugPath([string]$path, [string]$label) {
  if ($path -match '(?i)(^|[\\/])debug([\\/]|$)') {
    throw "$label must not come from Debug: $path"
  }
}

function Assert-BuildConfigPath([string]$path, [string]$config, [string]$label) {
  Assert-NotDebugPath $path $label
  $segment = [Regex]::Escape($config)
  if ($path -notmatch "(?i)(^|[\\/])$segment([\\/]|$)") {
    throw "$label must come from ${config}: $path"
  }
}

function Find-ConfigFile([string]$root, [string]$pattern, [string]$config, [string]$label) {
  if (-not (Test-Path $root)) {
    throw "$label search root not found: $root"
  }
  $segment = [Regex]::Escape($config)
  $item = Get-ChildItem -Path $root -Recurse -Filter $pattern -ErrorAction SilentlyContinue |
    Where-Object {
      $_.FullName -notmatch '(?i)(^|[\\/])debug([\\/]|$)' -and
      $_.FullName -match "(?i)(^|[\\/])$segment([\\/]|$)"
    } |
    Sort-Object FullName |
    Select-Object -First 1
  if (-not $item) {
    throw "$label not found under $root for config $config"
  }
  Assert-BuildConfigPath $item.FullName $config $label
  return $item.FullName
}

function Require-Dir([string]$path) {
  if (-not (Test-Path $path)) {
    throw "missing dir: $path"
  }
}

function Ensure-Dir([string]$path) {
  New-Item -ItemType Directory -Force -Path $path | Out-Null
}

function Remove-IfExists([string]$path) {
  if (Test-Path $path) {
    Remove-Item -Path $path -Recurse -Force
  }
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
$BuildConfig = Normalize-BuildConfig $BuildConfig
$packageSources = [ordered]@{}

$mysqlUsernameValue = if ($MysqlUsername) {
  $MysqlUsername
} elseif ($env:MI_E2EE_MYSQL_USERNAME) {
  $env:MI_E2EE_MYSQL_USERNAME
} else {
  ""
}
$mysqlPasswordValue = if ($MysqlPassword) {
  $MysqlPassword
} elseif ($env:MI_E2EE_MYSQL_PASSWORD) {
  $env:MI_E2EE_MYSQL_PASSWORD
} else {
  ""
}
if ([string]::IsNullOrWhiteSpace($mysqlUsernameValue) -or
    [string]::IsNullOrWhiteSpace($mysqlPasswordValue)) {
  throw "mysql credentials required: pass -MysqlUsername/-MysqlPassword or set MI_E2EE_MYSQL_USERNAME and MI_E2EE_MYSQL_PASSWORD"
}
if (($mysqlUsernameValue -ieq "root") -and
    ($mysqlPasswordValue -eq "123456" -or $mysqlPasswordValue -ieq "pass")) {
  throw "weak mysql credentials are forbidden in package config"
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
$keysDir = Join-Path $distRoot "keys"

Remove-IfExists $clientRoot
Remove-IfExists $serverRoot
Remove-IfExists $keysDir
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

$forbiddenClientArtifacts = @(
  "mi_e2ee_client.exe",
  "e2ee_login.exe",
  "e2ee_main_list.exe",
  "e2ee_group_chat.exe",
  "e2ee_chat_empty.exe"
)
foreach ($name in $forbiddenClientArtifacts) {
  Remove-IfExists (Join-Path $clientRoot $name)
}

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

Ensure-Dir $keysDir
$ktKeygen = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_kt_keygen.exe" $BuildConfig "kt_keygen"
$packageSources["kt_keygen"] = $ktKeygen
& $ktKeygen --out-dir $keysDir --force

$pfxPath = Join-Path $keysDir "mi_e2ee_server.pfx"
$cert = New-SelfSignedCertificate `
  -Subject "CN=MI_E2EE_Server" `
  -TextExtension @("2.5.29.17={text}DNS=localhost&IPAddress=127.0.0.1") `
  -CertStoreLocation "Cert:\\CurrentUser\\My"
$pwd = New-Object System.Security.SecureString
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pwd | Out-Null
$cert = Get-PfxCertificate -FilePath $pfxPath
$der = $cert.Export("Cert")
$hash = [System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash($der)).Replace("-", "").ToLower()

$serverExe = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_server.exe" $BuildConfig "server app"
$serverLauncher = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_server_launcher.exe" $BuildConfig "server launcher"
$packageSources["server_exe"] = $serverExe
$packageSources["server_launcher"] = $serverLauncher
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
if ($demoUsers) {
  Assert-NotDebugPath $demoUsers "test_user"
  $packageSources["test_user"] = $demoUsers
  Copy-Item $demoUsers $serverRoot -Force
} else {
  "u:p" | Set-Content -Path (Join-Path $serverRoot "test_user.txt") -Encoding ASCII
}

$ktPubinfo = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_kt_pubinfo.exe" $BuildConfig "kt_pubinfo"
$perfBaseline = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_perf_baseline.exe" $BuildConfig "perf_baseline"
$opsHealth = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_ops_health_view.exe" $BuildConfig "ops_health_view"
$thirdPartyAudit = Find-ConfigFile (Join-Path $workspace "build\\server") "mi_e2ee_third_party_audit.exe" $BuildConfig "third_party_audit"
$packageSources["kt_pubinfo"] = $ktPubinfo
$packageSources["perf_baseline"] = $perfBaseline
$packageSources["ops_health_view"] = $opsHealth
$packageSources["third_party_audit"] = $thirdPartyAudit
Copy-Item $ktKeygen $serverTools -Force
Copy-Item $ktPubinfo $serverTools -Force
Copy-Item $perfBaseline $serverTools -Force
Copy-Item $opsHealth $serverTools -Force
Copy-Item $thirdPartyAudit $serverTools -Force

Copy-Item (Join-Path $keysDir "kt_signing_key.bin") $serverConfig -Force
Copy-Item (Join-Path $keysDir "kt_root_pub.bin") $serverConfig -Force
Copy-Item $pfxPath $serverConfig -Force
Copy-Item (Join-Path $keysDir "kt_root_pub.bin") $clientConfig -Force
$metadataKeyPath = Join-Path $serverConfig "metadata_key.bin"
$metadataKey = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Fill($metadataKey)
[System.IO.File]::WriteAllBytes($metadataKeyPath, $metadataKey)

$serverConfigLines = @(
  "[mode]",
  "mode=0",
  "[mysql]",
  "mysql_ip=localhost",
  "mysql_port=3306",
  "mysql_database=mi_e2ee",
  "mysql_username=$mysqlUsernameValue",
  "mysql_password=$mysqlPasswordValue",
  "[server]",
  "list_port=9000",
  "rotation_threshold=10000",
  "offline_dir=database/offline_store",
  "debug_log=0",
  "offline_blob_temp_budget_bytes=4294967296",
  "tls_enable=1",
  "require_tls=1",
  "tls_cert=config/mi_e2ee_server.pfx",
  "kt_signing_key=kt_signing_key.bin",
  "metadata_protection=none",
  "metadata_key_path=config/metadata_key.bin",
  "[kcp]",
  "enable=0",
  "allow_insecure=0"
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
  "tls_verify_mode=pin",
  "tls_ca_bundle_path=",
  "tls_verify_hostname=1",
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
  "enabled=1",
  "role=primary",
  "key_path=e2ee_state/device_sync_key.bin",
  "",
  "[kt]",
  "require_signature=1",
  "root_pubkey_path=kt_root_pub.bin"
)
$clientConfigLines | Set-Content -Path (Join-Path $clientConfig "client_config.ini") -Encoding ASCII
"" | Set-Content -Path (Join-Path $clientDb "server_trust.ini") -Encoding ASCII

$uiRoot = Join-Path $workspace "build\\client\\ui\\$BuildConfig"
Require-Dir $uiRoot
Assert-BuildConfigPath $uiRoot $BuildConfig "ui root"
$packageSources["ui_root"] = $uiRoot
Copy-Item (Join-Path $uiRoot "mi_e2ee_client_ui_app.exe") $clientRoot -Force
Copy-Item (Join-Path $uiRoot "mi_e2ee_client_ui.exe") $clientRoot -Force
Copy-Item (Join-Path $uiRoot "mi_e2ee.exe") $clientRoot -Force

$sdkDll = Find-ConfigFile (Join-Path $workspace "build\\client") "mi_e2ee_client_sdk.dll" $BuildConfig "sdk dll"
$packageSources["sdk_dll"] = $sdkDll
Copy-Item $sdkDll $clientRoot -Force
Copy-Item $sdkDll $clientDll -Force

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

$imeRoot = Join-Path $workspace "build\\client\\ui\\ime_rime\\$BuildConfig"
if (Test-Path $imeRoot) {
  Assert-BuildConfigPath $imeRoot $BuildConfig "ime root"
  $packageSources["ime_root"] = $imeRoot
}
$imeCandidates = @(
  (Join-Path $workspace "build\\client\\ui\\$BuildConfig\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\$BuildConfig\\runtime\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\ime_rime\\$BuildConfig\\mi_ime_rime.dll"),
  (Join-Path $workspace "build\\client\\ime_rime\\$BuildConfig\\mi_ime_rime.dll")
)
$imeDllPath = $imeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($imeDllPath) {
  Assert-BuildConfigPath $imeDllPath $BuildConfig "mi_ime_rime"
  $packageSources["ime_dll"] = $imeDllPath
  Copy-Item $imeDllPath $clientDll -Force
  Copy-Item $imeDllPath $clientRoot -Force
}

$rimeCandidates = @(
  (Join-Path $imeRoot "rime.dll"),
  (Join-Path $workspace "build\\client\\ui\\$BuildConfig\\runtime\\rime.dll")
)
$rimeDllPath = $rimeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($rimeDllPath) {
  Assert-BuildConfigPath $rimeDllPath $BuildConfig "rime runtime dll"
  $packageSources["rime_dll"] = $rimeDllPath
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

$normalizedSources = [ordered]@{}
foreach ($entry in $packageSources.GetEnumerator()) {
  $sourcePath = [string]$entry.Value
  Assert-NotDebugPath $sourcePath "source $($entry.Key)"
  $resolvedSource = $sourcePath
  try {
    $resolvedSource = (Resolve-Path $sourcePath).Path
  } catch {
    # Keep original path in metadata if source has already been cleaned.
  }
  $normalizedSources[$entry.Key] = $resolvedSource
}

$meta = [ordered]@{
  build_config = $BuildConfig
  generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
  sources = $normalizedSources
}
$metaPath = Join-Path $distRoot "package_build_meta.windows.json"
$meta | ConvertTo-Json -Depth 6 | Set-Content -Path $metaPath -Encoding UTF8

$clientZip = Join-Path $distRoot "mi_e2ee_client.zip"
$serverZip = Join-Path $distRoot "mi_e2ee_server.zip"
Compress-Archive -Path $clientRoot -DestinationPath $clientZip -Force
Compress-Archive -Path $serverRoot -DestinationPath $serverZip -Force
