@echo off
setlocal
cd /d "%~dp0"
title XuperTV Nova - Compilar en Windows
echo ===========================================
echo    XuperTV NOVA - Compilacion local
echo ===========================================
echo.
echo Se instalaran dependencias oficiales de MSYS2
echo y se compilara XuperTV Nova para este Windows.
echo No se borraran datos personales ni se tocara main.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Compilar-Nova.ps1" -Run
if errorlevel 1 (
    echo.
    echo [ERROR] No se pudo completar. Copia o toma foto del mensaje.
) else (
    echo.
    echo [OK] XuperTV Nova se compilo y se abrio.
)
echo.
pause
