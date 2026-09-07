# Compila y flashea el firmware ESP32-S3 con el dashboard V2 (diseño del simulador).
param(
    [string]$Env = "esp32-s3-devkitc-1",
    [string]$Port = "",
    [switch]$Monitor,
    [switch]$SkipAssets
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Find-PlatformIO {
    $pio = Get-Command pio -ErrorAction SilentlyContinue
    if ($pio) { return $pio.Source }
    $fallback = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
    if (Test-Path $fallback) { return $fallback }
    throw "PlatformIO no encontrado. Instalalo desde https://platformio.org o abri el proyecto en VS Code/Cursor con la extension PlatformIO."
}

$pio = Find-PlatformIO

if (-not $SkipAssets) {
    Write-Host "==> Generando iconos LVGL 8..."
    python "$Root\scripts\generate_icons.py"
    Write-Host "==> Generando degradado de herradura..."
    python "$Root\scripts\emit_arc_grad_lv8.py"
}

$uploadArgs = @("run", "-e", $Env, "-t", "upload")
if ($Port) {
    $uploadArgs += @("--upload-port", $Port)
}

Write-Host "==> Compilando y flasheando ($Env)..."
& $pio @uploadArgs
if ($LASTEXITCODE -ne 0) {
    throw "Fallo el upload (exit $LASTEXITCODE)"
}

Write-Host ""
Write-Host "Firmware subido correctamente."

if ($Monitor) {
    Write-Host "==> Abriendo monitor serial..."
    $monitorArgs = @("device", "monitor", "-e", $Env)
    if ($Port) {
        $monitorArgs += @("--port", $Port)
    }
    & $pio @monitorArgs
}
