# SPDX-License-Identifier: GPL-3.0-only
<#
.SYNOPSIS
  Update an EXISTING isolated Nova+Veyra Alpha to test public live VFG.
.DESCRIPTION
  The new NovaQt executable must already be built from this feature branch.
  Preserves catalog, sessions, pairing, native Veyra DLL and installed assets.
  Backs up only the old EXE before copying the new one.
  Produces a SEPARATE opt-in launcher; original Alpha launcher is unchanged.
#>
[CmdletBinding(SupportsShouldProcess=$true)]
param(
  [string]$AlphaDirectory='',
  [string]$NovaExecutable='',
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).ProviderPath
if(-not $NovaExecutable){
  $NovaExecutable=Join-Path $repo 'dist\Xuper-TV-NovaQt-Local\XuperTVNovaQt.exe'
}
if(-not(Test-Path -LiteralPath $NovaExecutable -PathType Leaf)){
  throw 'NovaQt EXE not built yet. First run tools\Compilar-Nova.ps1 from the latest source.'
}
$newExe=(Resolve-Path -LiteralPath $NovaExecutable).ProviderPath
if(-not $AlphaDirectory){
  $work=Join-Path $env:USERPROFILE 'NovaVeyraNativeBuild'
  if(-not(Test-Path -LiteralPath $work -PathType Container)){
    throw 'Cannot find Native Alpha workspace; specify -AlphaDirectory.'
  }
  $alpha=Get-ChildItem -LiteralPath $work -Directory -Filter 'NovaVeyraNative-*' |
    Where-Object {
      (Test-Path -LiteralPath (Join-Path $_.FullName 'Start-NovaVeyraNative.ps1') -PathType Leaf) -and
      (Test-Path -LiteralPath (Join-Path $_.FullName 'nova_veyra_native.dll') -PathType Leaf)
    } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if(-not $alpha){throw 'No staged Alpha found. Specify -AlphaDirectory.'}
  $AlphaDirectory=$alpha.FullName
}
$dest=(Resolve-Path -LiteralPath $AlphaDirectory).ProviderPath
foreach($name in @('XuperTVNovaQt.exe','nova_veyra_native.dll','Start-NovaVeyraNative.ps1')){
  if(-not(Test-Path -LiteralPath (Join-Path $dest $name) -PathType Leaf)){
    throw "The selected folder is not a staged native Veyra Alpha: $name missing."
  }
}
$oldExe=Join-Path $dest 'XuperTVNovaQt.exe'
if($oldExe.Equals($newExe,[System.StringComparison]::OrdinalIgnoreCase)){
  throw 'Source EXE and Alpha EXE must be different paths.'
}
$sourceHash=(Get-FileHash -LiteralPath $newExe -Algorithm SHA256).Hash
$oldHash=(Get-FileHash -LiteralPath $oldExe -Algorithm SHA256).Hash
Write-Host "Alpha: $dest"
Write-Host "Updated EXE: $newExe"
Write-Host "Different executable: $($oldHash -ne $sourceHash)"
Write-Host 'Catalog files, device identity, library and pairing will remain untouched.'
if($CheckOnly){Write-Host 'CheckOnly: nothing copied.';return}

$script=@'
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$env:NOVA_VEYRA_NATIVE_DLL=Join-Path $root 'nova_veyra_native.dll'
$env:NOVA_VEYRA_NATIVE_STRICT='1'
$env:NOVA_VEYRA_NATIVE_ALLOW_NETWORK='1'
$env:NOVA_VEYRA_NATIVE_LIVE_BETA='1'
Set-Location $root
& (Join-Path $root 'XuperTVNovaQt.exe')
'@
if(-not $PSCmdlet.ShouldProcess($dest,'Backup old EXE and enable isolated public-live VFG Beta')){
  return
}
$backup=Join-Path $dest ('XuperTVNovaQt.exe.before-live-beta.'+(Get-Date -Format 'yyyyMMdd-HHmmssfff')+'.bak')
Copy-Item -LiteralPath $oldExe -Destination $backup -ErrorAction Stop
try{
  Copy-Item -LiteralPath $newExe -Destination $oldExe -Force -ErrorAction Stop
  Set-Content -LiteralPath (Join-Path $dest 'Start-NovaVeyraLiveBeta.ps1') -Value $script -Encoding UTF8
}catch{
  Copy-Item -LiteralPath $backup -Destination $oldExe -Force -ErrorAction SilentlyContinue
  throw
}
Write-Host 'Alpha updated. Old EXE backed up; existing catalog and Veyra DLL unchanged.' -ForegroundColor Green
Write-Host ('Run beta with: & "'+(Join-Path $dest 'Start-NovaVeyraLiveBeta.ps1')+'"') -ForegroundColor Cyan
Write-Host 'Network sources requiring custom headers or external subtitles use MPV.'
