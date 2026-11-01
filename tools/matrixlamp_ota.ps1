# Send a compiled Matrix Lamp .bin to the lamp over WiFi.
# The password is the OTA_PASSWORD environment variable. Do not pass it on the command line.
param(
  [Parameter(Mandatory = $true)][string]$Bin,
  [Parameter(Mandatory = $true)][string]$HostName
)
$ErrorActionPreference = "Stop"
$pass = $env:OTA_PASSWORD
if (-not $pass) {
  $root = Split-Path (Split-Path (Split-Path -LiteralPath $Bin))
  $secrets = Join-Path $root "sketches\MatrixLamp_R4_Wifi\secrets.h"
  if (Test-Path -LiteralPath $secrets) {
    $textSecrets = Get-Content -Raw -LiteralPath $secrets
    if ($textSecrets -match '(?m)^[ \t]*#define[ \t]+OTA_PASSWORD[ \t]+"([^"]*)"') {
      $pass = $Matches[1]
    }
  }
}
if (-not $pass) {
  $pass = Read-Host "OTA password"
}
if (-not $pass) {
  Write-Host "No OTA password. Set OTA_PASSWORD, or put it in secrets.h."
  exit 1
}
if (-not (Test-Path -LiteralPath $Bin)) {
  Write-Host "FAILED: missing $Bin"
  exit 1
}
$bytes = [System.IO.File]::ReadAllBytes($Bin)
$crc = [uint32]4294967295
foreach ($b in $bytes) {
  $crc = [uint32]($crc -bxor [uint32]$b)
  for ($i = 0; $i -lt 8; $i++) {
    $bit = $crc -band 1
    $crc = [uint32](($crc -shr 1) -band 0x7FFFFFFF)
    if ($bit -ne 0) { $crc = [uint32]($crc -bxor 0xEDB88320) }
  }
}
$crc = [uint32]((-bnot $crc) -band 0xFFFFFFFF)
$crcHex = "{0:x8}" -f $crc

$text = [System.Text.Encoding]::ASCII.GetString($bytes)
$at = $text.IndexOf("MLFW:")
if ($at -lt 0) {
  Write-Host "FAILED: the .bin has no MLFW version marker."
  exit 1
}
$mark = $text.Substring($at + 5)
$nul = $mark.IndexOf([char]0)
if ($nul -ge 0) { $mark = $mark.Substring(0, $nul) }
$bar = $mark.IndexOf("|")
if ($bar -lt 1) {
  Write-Host "FAILED: bad MLFW marker."
  exit 1
}
$ver = $mark.Substring(0, $bar)
$built = $mark.Substring($bar + 1)

Write-Host ("Sending {0} bytes to http://{1}/update" -f $bytes.Length, $HostName)
$curl = Join-Path $env:SystemRoot "System32\curl.exe"
if (-not (Test-Path $curl)) { $curl = "curl.exe" }
$body = Join-Path $env:TEMP "matrixlamp-ota-body.txt"
& $curl -fS --http1.1 --max-time 180 `
  -H "X-OTA-Password: $pass" `
  -H "X-OTA-Crc32: $crcHex" `
  -H "Content-Type: application/octet-stream" `
  --data-binary "@$Bin" `
  -o $body `
  "http://$HostName/update"
$curlRc = $LASTEXITCODE
if (Test-Path $body) {
  Write-Host ("Lamp said: " + ((Get-Content -Raw $body) -replace "`r","").Trim())
}
if ($curlRc -ne 0) {
  Write-Host "curl exited $curlRc. The lamp may be rebooting; checking the version."
}
for ($n = 0; $n -lt 30; $n++) {
  Start-Sleep -Seconds 2
  try {
    $state = Invoke-RestMethod -Uri "http://$HostName/api/state" -TimeoutSec 4
    if ($state.fw -eq $ver -and $state.built -eq $built) {
      Write-Host "SUCCESS: lamp is running $ver built $built"
      exit 0
    }
  } catch {
    Write-Host "waiting for the lamp"
  }
}
Write-Host "FAILED: the lamp did not report $ver built $built."
exit 1
