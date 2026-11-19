@echo off
setlocal
set "NOVA_PILOT_DIR=%~dp0"
powershell.exe -NoProfile -Command "$ErrorActionPreference='Stop'; $root=Join-Path $env:USERPROFILE 'NovaVeyraNativeBuild'; $alpha=Get-ChildItem -LiteralPath $root -Directory -Filter 'NovaVeyraNative-*' | Where-Object { (Test-Path (Join-Path $_.FullName 'nova_veyra_native.dll')) -and (Test-Path (Join-Path $_.FullName 'Start-NovaVeyraNative.ps1')) } | Sort-Object LastWriteTime -Descending | Select-Object -First 1; if(!$alpha){throw 'No se encontro la Alpha instalada'}; & (Join-Path $env:NOVA_PILOT_DIR 'tools\Enable-NovaVeyraLiveBeta.ps1') -AlphaDirectory $alpha.FullName -NovaExecutable (Join-Path $env:NOVA_PILOT_DIR 'XuperTVNovaQt.exe'); & (Join-Path $alpha.FullName 'Start-NovaVeyraLiveBeta.ps1')"
if errorlevel 1 pause
