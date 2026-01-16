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

$workspace = Resolve-Workspace
$distRoot = if ($Dist) { $Dist } else { Join-Path $workspace "dist" }

$clientRoot = Join-Path $distRoot "mi_e2ee_client"
$serverRoot = Join-Path $distRoot "mi_e2ee_server"

Require-Dir $clientRoot
Require-Dir $serverRoot

Require-File (Join-Path $clientRoot "mi_e2ee_client_sdk.dll")
Require-File (Join-Path $clientRoot "config\\client_config.ini")
Require-File (Join-Path $clientRoot "sdk\\c_api_client.h")
Require-File (Join-Path $clientRoot "bindings\\python\\mi_e2ee_client.py")
Require-File (Join-Path $clientRoot "bindings\\rust\\Cargo.toml")
Require-File (Join-Path $clientRoot "dll\\mi_e2ee_client_sdk.dll")
Require-File (Join-Path $clientRoot "ffmpeg.exe")

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

$fingerprint = ""
Get-Content (Join-Path $clientRoot "config\\client_config.ini") | ForEach-Object {
  if ($_ -match "^pinned_fingerprint=(.+)$") {
    $fingerprint = $Matches[1].Trim()
  }
}
if ($fingerprint -notmatch "^[0-9a-f]{64}$") {
  throw "pinned_fingerprint invalid: $fingerprint"
}

Verify-Manifest $clientRoot
Verify-Manifest $serverRoot
