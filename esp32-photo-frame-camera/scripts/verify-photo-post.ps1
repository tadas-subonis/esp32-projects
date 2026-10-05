# POST golden JPEG to PaperColor (join PhotoFrame AP first).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Jpeg = Join-Path $Root "shared\test_assets\colorbars_qvga.jpg"
$FrameHost = if ($env:PHOTO_FRAME_HOST) { $env:PHOTO_FRAME_HOST } else { "192.168.4.1" }
$Url = "http://${FrameHost}/api/v1/photo"

if (-not (Test-Path $Jpeg)) {
    throw "Missing test asset: $Jpeg"
}

$Meta = '{"version":1,"id":"verify-script","caption":"","width":320,"height":240,"jpeg_quality":40}'
$RespFile = Join-Path $env:TEMP "pf_verify_resp.json"

Write-Host "POST $Url"
$code = (& curl.exe -sS -o $RespFile -w "%{http_code}" `
    -F "meta=${Meta};type=application/json" `
    -F "image=@${Jpeg};type=image/jpeg" `
    $Url).Trim()
Write-Host "HTTP $code"
Get-Content $RespFile
if ($code -ne "202") { exit 1 }
