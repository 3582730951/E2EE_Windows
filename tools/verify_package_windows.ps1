param(
  [string]$Dist
)

$ErrorActionPreference = "Stop"

function Resolve-Workspace() {
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

function Require-File([string]$path) {
  if (-not (Test-Path $path)) {
    throw "missing file: $path"
  }
}

function Require-FileAbsent([string]$path) {
  if (Test-Path $path) {
    throw "forbidden file present: $path"
  }
}

function Require-Dir([string]$path) {
  if (-not (Test-Path $path)) {
    throw "missing dir: $path"
  }
}

function Require-NonEmptyDir([string]$path) {
  Require-Dir $path
  if (-not (Get-ChildItem -Path $path -Recurse -File | Select-Object -First 1)) {
    throw "empty dir: $path"
  }
}

function Read-Json([string]$path) {
  Require-File $path
  try {
    return Get-Content -Path $path -Raw | ConvertFrom-Json
  } catch {
    throw "invalid json: $path"
  }
}

function Assert-NotDebugPath([string]$path, [string]$label) {
  if ($path -match '(?i)(^|[\\/])debug([\\/]|$)') {
    throw "$label contains Debug path: $path"
  }
}

function Assert-NoPdb([string]$root, [string]$label) {
  $pdb = Get-ChildItem -Path $root -Recurse -File -Filter "*.pdb" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($pdb) {
    throw "$label contains forbidden pdb: $($pdb.FullName)"
  }
}

function Read-PfxSha256([string]$path) {
  Require-File $path
  $cert = Get-PfxCertificate -FilePath $path
  if (-not $cert) {
    throw "failed to read pfx certificate: $path"
  }
  $der = $cert.Export("Cert")
  return [System.BitConverter]::ToString(
    [System.Security.Cryptography.SHA256]::Create().ComputeHash($der)
  ).Replace("-", "").ToLower()
}

function Verify-Manifest([string]$root) {
  $manifest = Join-Path $root "manifest.sha256"
  Require-File $manifest
  Get-Content $manifest | Where-Object { $_ -match "\S" } | ForEach-Object {
    $parts = $_ -split "\s+", 2
    if ($parts.Count -lt 2) {
      throw "invalid manifest line: $_"
    }
    $expected = $parts[0].ToLower()
    $rel = $parts[1]
    $path = Join-Path $root $rel
    Require-File $path
    $actual = (Get-FileHash -Algorithm SHA256 $path).Hash.ToLower()
    if ($actual -ne $expected) {
      throw "hash mismatch: $rel"
    }
  }
}

function Assert-KcpDisabled([string]$configPath) {
  Require-File $configPath
  $inKcp = $false
  Get-Content -Path $configPath | ForEach-Object {
    $line = $_
    $commentPos = $line.IndexOf('#')
    if ($commentPos -ge 0) {
      $line = $line.Substring(0, $commentPos)
    }
    $line = $line.Trim()
    if (-not $line) {
      return
    }
    if ($line -match '^\[(.+)\]$') {
      $inKcp = ($Matches[1].Trim().ToLower() -eq "kcp")
      return
    }
    if (-not $inKcp) {
      return
    }
    if ($line -match '^(?i)enable\s*=\s*(.+)$') {
      $value = $Matches[1].Trim().ToLower()
      if ($value -in @("1", "true", "on", "yes")) {
        throw "release package forbids [kcp] enable=1"
      }
    }
  }
}

function Assert-BlobBudget([string]$configPath) {
  Require-File $configPath
  $inServer = $false
  $value = $null
  Get-Content -Path $configPath | ForEach-Object {
    $line = $_
    $commentPos = $line.IndexOf('#')
    if ($commentPos -ge 0) {
      $line = $line.Substring(0, $commentPos)
    }
    $line = $line.Trim()
    if (-not $line) {
      return
    }
    if ($line -match '^\[(.+)\]$') {
      $inServer = ($Matches[1].Trim().ToLower() -eq "server")
      return
    }
    if (-not $inServer) {
      return
    }
    if ($line -match '^(?i)offline_blob_temp_budget_bytes\s*=\s*(.+)$') {
      $value = $Matches[1].Trim()
    }
  }
  if (-not $value) {
    throw "server config missing offline_blob_temp_budget_bytes"
  }
  [UInt64]$bytes = 0
  if (-not [UInt64]::TryParse($value, [ref]$bytes)) {
    throw "offline_blob_temp_budget_bytes invalid: $value"
  }
  if ($bytes -lt 67108864) {
    throw "offline_blob_temp_budget_bytes too small in package config"
  }
}

function Require-IniValue([string]$configPath, [string]$section, [string]$key, [string]$expected) {
  Require-File $configPath
  $currentSection = ""
  $actual = $null
  Get-Content -Path $configPath | ForEach-Object {
    $line = $_
    $commentPos = $line.IndexOf('#')
    if ($commentPos -ge 0) {
      $line = $line.Substring(0, $commentPos)
    }
    $line = $line.Trim()
    if (-not $line) {
      return
    }
    if ($line -match '^\[(.+)\]$') {
      $currentSection = $Matches[1].Trim().ToLower()
      return
    }
    if ($currentSection -ne $section.ToLower()) {
      return
    }
    if ($line -match '^(?i)([^=]+)=(.*)$') {
      $name = $Matches[1].Trim().ToLower()
      $value = $Matches[2].Trim()
      if ($name -eq $key.ToLower()) {
        $actual = $value
      }
    }
  }
  if ($null -eq $actual) {
    throw "missing config key [$section] $key in $configPath"
  }
  if ($actual -ne $expected) {
    throw "unexpected config value [$section] $key=$actual (expected $expected)"
  }
}

$workspace = Resolve-Workspace
$distRoot = if ($Dist) { $Dist } else { Join-Path $workspace "dist" }

$clientRoot = Join-Path $distRoot "mi_e2ee_client"
$serverRoot = Join-Path $distRoot "mi_e2ee_server"

Require-Dir $clientRoot
Require-Dir $serverRoot

$metaPath = Join-Path $distRoot "package_build_meta.windows.json"
$meta = Read-Json $metaPath
$allowedConfigs = @("Release", "RelWithDebInfo", "MinSizeRel")
if (-not $allowedConfigs.Contains([string]$meta.build_config)) {
  throw "metadata build_config invalid: $($meta.build_config)"
}
if (-not $meta.sources) {
  throw "metadata sources missing: $metaPath"
}
$requiredSources = @("kt_keygen", "server_exe", "server_launcher", "sdk_dll", "ui_root")
foreach ($name in $requiredSources) {
  $value = [string]$meta.sources.$name
  if ([string]::IsNullOrWhiteSpace($value)) {
    throw "metadata source missing: $name"
  }
}
foreach ($entry in $meta.sources.PSObject.Properties) {
  $src = [string]$entry.Value
  if ([string]::IsNullOrWhiteSpace($src)) {
    throw "metadata source empty: $($entry.Name)"
  }
  Assert-NotDebugPath $src "metadata source '$($entry.Name)'"
}

Require-File (Join-Path $clientRoot "mi_e2ee_client_sdk.dll")
Require-File (Join-Path $clientRoot "mi_e2ee_client_ui_app.exe")
Require-File (Join-Path $clientRoot "mi_e2ee_client_ui.exe")
Require-File (Join-Path $clientRoot "mi_e2ee.exe")
Require-File (Join-Path $clientRoot "config\\client_config.ini")
Require-File (Join-Path $clientRoot "sdk\\c_api_client.h")
Require-File (Join-Path $clientRoot "bindings\\python\\mi_e2ee_client.py")
Require-File (Join-Path $clientRoot "bindings\\rust\\Cargo.toml")
Require-File (Join-Path $clientRoot "dll\\mi_e2ee_client_sdk.dll")
Require-File (Join-Path $clientRoot "ffmpeg.exe")
Require-FileAbsent (Join-Path $clientRoot "mi_e2ee_client.exe")
Require-FileAbsent (Join-Path $clientRoot "e2ee_login.exe")
Require-FileAbsent (Join-Path $clientRoot "e2ee_main_list.exe")
Require-FileAbsent (Join-Path $clientRoot "e2ee_group_chat.exe")
Require-FileAbsent (Join-Path $clientRoot "e2ee_chat_empty.exe")

$clientDb = Join-Path $clientRoot "database"
$rimePrebuilt = Join-Path $clientDb "rime\\prebuilt"
$rimeShare = Join-Path $clientDb "rime\\share"
$openccDir = Join-Path $clientDb "opencc"
Require-NonEmptyDir $rimePrebuilt
Require-NonEmptyDir $rimeShare
Require-NonEmptyDir $openccDir

$rimeDll = Join-Path $clientRoot "dll\\rime.dll"
$rimeAlt = Join-Path $clientRoot "dll\\librime.dll"
if (-not (Test-Path $rimeDll) -and -not (Test-Path $rimeAlt)) {
  throw "missing rime runtime dll"
}
Require-NonEmptyDir (Join-Path $clientRoot "dll\\qml")
Require-NonEmptyDir (Join-Path $clientRoot "translations")
Require-NonEmptyDir (Join-Path $clientRoot "icon")

$esrganRoot = Join-Path $clientRoot "tools\\realesrgan"
$esrganModels = Join-Path $clientDb "ai_models\\realesrgan"
Require-NonEmptyDir $esrganRoot
Require-NonEmptyDir $esrganModels
if (-not (Get-ChildItem -Path $esrganRoot -Filter "*.exe" | Select-Object -First 1)) {
  throw "missing realesrgan executable"
}
if (-not (Get-ChildItem -Path $esrganModels -Filter "*.param" | Select-Object -First 1)) {
  throw "missing realesrgan model .param"
}
if (-not (Get-ChildItem -Path $esrganModels -Filter "*.bin" | Select-Object -First 1)) {
  throw "missing realesrgan model .bin"
}

Require-File (Join-Path $serverRoot "mi_e2ee_server.exe")
Require-File (Join-Path $serverRoot "mi_e2ee_server_app.exe")
Require-File (Join-Path $serverRoot "config\\config.ini")
Require-File (Join-Path $serverRoot "config\\mi_e2ee_server.pfx")
Require-File (Join-Path $serverRoot "config\\kt_signing_key.bin")
Require-File (Join-Path $serverRoot "config\\kt_root_pub.bin")
Require-File (Join-Path $serverRoot "tools\\mi_e2ee_kt_keygen.exe")
Require-File (Join-Path $serverRoot "tools\\mi_e2ee_kt_pubinfo.exe")
Require-File (Join-Path $serverRoot "tools\\mi_e2ee_perf_baseline.exe")
Require-File (Join-Path $serverRoot "tools\\mi_e2ee_ops_health_view.exe")
Require-File (Join-Path $serverRoot "tools\\mi_e2ee_third_party_audit.exe")
Require-File (Join-Path $serverRoot "test_user.txt")

Assert-KcpDisabled (Join-Path $serverRoot "config\\config.ini")
Assert-BlobBudget (Join-Path $serverRoot "config\\config.ini")
Require-IniValue (Join-Path $serverRoot "config\\config.ini") "server" "tls_enable" "1"
Require-IniValue (Join-Path $serverRoot "config\\config.ini") "server" "require_tls" "1"
Require-IniValue (Join-Path $serverRoot "config\\config.ini") "server" "tls_cert" "config/mi_e2ee_server.pfx"
Require-IniValue (Join-Path $clientRoot "config\\client_config.ini") "client" "use_tls" "1"
Require-IniValue (Join-Path $clientRoot "config\\client_config.ini") "client" "require_tls" "1"
Require-IniValue (Join-Path $clientRoot "config\\client_config.ini") "client" "require_pinned_fingerprint" "1"
Require-IniValue (Join-Path $clientRoot "config\\client_config.ini") "client" "tls_verify_mode" "pin"

$fingerprint = ""
Get-Content (Join-Path $clientRoot "config\\client_config.ini") | ForEach-Object {
  if ($_ -match "^pinned_fingerprint=(.+)$") {
    $fingerprint = $Matches[1].Trim()
  }
}
if ($fingerprint -notmatch "^[0-9a-f]{64}$") {
  throw "pinned_fingerprint invalid: $fingerprint"
}
$serverFingerprint = Read-PfxSha256 (Join-Path $serverRoot "config\\mi_e2ee_server.pfx")
if ($serverFingerprint -ne $fingerprint) {
  throw "pinned_fingerprint does not match server certificate"
}

Verify-Manifest $clientRoot
Verify-Manifest $serverRoot

Assert-NoPdb $clientRoot "client package"
Assert-NoPdb $serverRoot "server package"
