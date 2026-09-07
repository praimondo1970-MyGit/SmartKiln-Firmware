$ErrorActionPreference = "Stop"

$SimDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SimDir

function Find-CMake {
    $candidates = @(
        "cmake",
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($c in $candidates) {
        if (Get-Command $c -ErrorAction SilentlyContinue) {
            return (Get-Command $c).Source
        }
        if (Test-Path $c) { return $c }
    }
    return $null
}

$cmake = Find-CMake
if (-not $cmake) {
    Write-Host "CMake no encontrado. Instalando con winget..."
    winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
    $cmake = Find-CMake
    if (-not $cmake) {
        throw "CMake sigue sin estar en PATH. Reiniciá la terminal o instalalo manualmente."
    }
}

if (-not (Test-Path "generated/icons/icon_wifi.c")) {
    Write-Host "Generando iconos LVGL 9..."
    python "$SimDir\tools\emit_icons_lv9.py"
}

$buildDir = Join-Path $SimDir "build"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$generator = $null
$extraArgs = @()

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if ($gcc -and $ninja) {
    $generator = "Ninja"
    $extraArgs += "-DCMAKE_C_COMPILER=$($gcc.Source)"
} elseif ($gcc) {
    $generator = "MinGW Makefiles"
    $extraArgs += "-DCMAKE_C_COMPILER=$($gcc.Source)"
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gpp) {
        $extraArgs += "-DCMAKE_CXX_COMPILER=$($gpp.Source)"
    }
}

Write-Host "Configurando CMake..."
if ($generator) {
    & $cmake -S $SimDir -B $buildDir -G $generator @extraArgs -DCMAKE_BUILD_TYPE=Release
} else {
    & $cmake -S $SimDir -B $buildDir -A x64 -DCMAKE_BUILD_TYPE=Release
}

Write-Host "Compilando..."
& $cmake --build $buildDir --config Release -j

Write-Host ""
Write-Host "Listo. Ejecutá:"
Write-Host "  $buildDir\smartkiln_sim.exe"
