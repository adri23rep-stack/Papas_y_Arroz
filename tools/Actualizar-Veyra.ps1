# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param([string]$AlphaDirectory)
$ErrorActionPreference='Stop'
$workspace=Join-Path $env:USERPROFILE 'NovaVeyraNativeBuild'
$exe=Join-Path $PSScriptRoot 'XuperTVNovaQt.exe'
if(-not $AlphaDirectory){
    $alpha=Get-ChildItem -LiteralPath $workspace -Directory -Filter 'NovaVeyraNative-*' |
        Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'Start-NovaVeyraLiveBeta.ps1')} |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $alpha){throw 'No se encontró tu Alpha de Veyra.'}
    $AlphaDirectory=$alpha.FullName
}
$enable=Join-Path $PSScriptRoot 'tools\Enable-NovaVeyraLiveBeta.ps1'
& $enable -AlphaDirectory $AlphaDirectory -NovaExecutable $exe -CheckOnly
# A cache belongs to its source directory. A freshly extracted ZIP may move.
$build=Join-Path $workspace ('out\https-'+[Guid]::NewGuid().ToString('N'))
& (Join-Path $PSScriptRoot 'tools\Complete-NovaVeyraNative.ps1') -BuildOnly -BuildDirectory $build -SkipFfmpegInstall
$newDll=Join-Path $build 'nova_veyra_native.dll'
$dest=Join-Path $AlphaDirectory 'nova_veyra_native.dll'
$backup=$dest+'.before-https.'+[Guid]::NewGuid().ToString('N')+'.bak'
$staged=$dest+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
try {
    Copy-Item -LiteralPath $newDll -Destination $staged
    [IO.File]::Replace($staged,$dest,$backup)
    try {
        & $enable -AlphaDirectory $AlphaDirectory -NovaExecutable $exe
    } catch {
        Copy-Item -LiteralPath $backup -Destination $dest -Force
        throw
    }
} finally {
    if(Test-Path -LiteralPath $staged){Remove-Item -LiteralPath $staged}
}
& (Join-Path $AlphaDirectory 'Start-NovaVeyraLiveBeta.ps1')
