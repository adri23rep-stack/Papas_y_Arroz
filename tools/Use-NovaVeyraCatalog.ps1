# SPDX-License-Identifier: GPL-3.0-only
<#
.SYNOPSIS
  Enable the user's already configured XuperTV catalog in an isolated Nova+Veyra Alpha.
.DESCRIPTION
  Explicit local opt-in: select an existing service.json from a Nova installation.
  Copies ONLY service.json and optional device_state.json into the staged Alpha.
  Never uploads, prints secrets, updates the original, or modifies git/main.
  This is not a login bypass: the provider must still authorize the device.
.EXAMPLE
  .\tools\Use-NovaVeyraCatalog.ps1
.EXAMPLE
  .\tools\Use-NovaVeyraCatalog.ps1 -SourceDirectory 'C:\NovaActual' -AlphaDirectory 'C:\Alpha'
#>
[CmdletBinding(SupportsShouldProcess=$true)]
param(
  [string]$SourceDirectory='',
  [string]$AlphaDirectory='',
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

if(-not $AlphaDirectory){
  $workspace=Join-Path $env:USERPROFILE 'NovaVeyraNativeBuild'
  if(-not (Test-Path -LiteralPath $workspace -PathType Container)){
    throw 'No local NovaVeyraNativeBuild directory found. Specify -AlphaDirectory.'
  }
  $alpha=Get-ChildItem -LiteralPath $workspace -Directory |
    Where-Object {
      $_.Name -like 'NovaVeyraNative-*' -and
      (Test-Path -LiteralPath (Join-Path $_.FullName 'XuperTVNovaQt.exe') -PathType Leaf) -and
      (Test-Path -LiteralPath (Join-Path $_.FullName 'nova_veyra_native.dll') -PathType Leaf)
    } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if(-not $alpha){throw 'No prepared Nova+Veyra Alpha found. Specify -AlphaDirectory.'}
  $AlphaDirectory=$alpha.FullName
}
$target=(Resolve-Path -LiteralPath $AlphaDirectory -ErrorAction Stop).ProviderPath
foreach($file in @('XuperTVNovaQt.exe','nova_veyra_native.dll','Start-NovaVeyraNative.ps1')){
  if(-not(Test-Path -LiteralPath (Join-Path $target $file) -PathType Leaf)){
    throw "Destination is not a staged Nova+Veyra Alpha: missing $file"
  }
}

if(-not $SourceDirectory){
  Add-Type -AssemblyName System.Windows.Forms
  $picker=New-Object System.Windows.Forms.OpenFileDialog
  $picker.Title='Selecciona service.json de tu Nova que tiene el catalogo XuperTV'
  $picker.Filter='Configuracion de Nova (service.json)|service.json'
  $picker.CheckFileExists=$true
  if($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK){
    Write-Host 'Cancelled: nothing copied.';return
  }
  $SourceDirectory=Split-Path -Parent $picker.FileName
}
$source=(Resolve-Path -LiteralPath $SourceDirectory -ErrorAction Stop).ProviderPath
if($source.TrimEnd('\').Equals($target.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase)){
  throw 'Source and destination are the same folder. No changes made.'
}
$configFile=Join-Path $source 'service.json'
if(-not (Test-Path -LiteralPath $configFile -PathType Leaf)){
  throw 'The chosen folder has no service.json. Choose an already configured Nova installation.'
}
if((Get-Item -LiteralPath $configFile).Length -gt 65536){
  throw 'service.json is larger than the supported installation format. No changes made.'
}
try {
  $cfg=Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
  if(-not $cfg.installation_serial -or -not $cfg.password_salt -or
     -not $cfg.key_base64 -or -not $cfg.portal_hosts){
    throw 'Incomplete service configuration'
  }
}catch{
  throw 'service.json is invalid/incomplete. No files copied.'
}
$names=@('service.json','device_state.json')
$copy=@($names | Where-Object { Test-Path -LiteralPath (Join-Path $source $_) -PathType Leaf })
Write-Host 'Existing XuperTV configuration found.' -ForegroundColor Green
Write-Host "Destination: $target"
Write-Host ('Files available for local transfer: ' + ($copy -join ', '))
Write-Host 'Originals will NOT be changed. A copy may contain private device information.'
if($CheckOnly){Write-Host 'CheckOnly: no changes made.';return}
foreach($name in $copy){
  $src=Join-Path $source $name
  $dst=Join-Path $target $name
  if(Test-Path -LiteralPath $dst){
    throw "Destination already has $name. No existing Alpha configuration will be overwritten."
  }
}
foreach($name in $copy){
  $src=Join-Path $source $name
  $dst=Join-Path $target $name
  if($PSCmdlet.ShouldProcess($dst,"Copy local $name from the chosen Nova installation")){
    Copy-Item -LiteralPath $src -Destination $dst -ErrorAction Stop
  }
}
Write-Host 'Local configuration imported into the Alpha. Restart it with Start-NovaVeyraNative.ps1.' -ForegroundColor Green
Write-Host 'If the provider rejects authentication (for example portal200001), movies/series still cannot load.'
