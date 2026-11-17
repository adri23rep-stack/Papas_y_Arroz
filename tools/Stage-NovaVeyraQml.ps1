<#
.SYNOPSIS
  Make an isolated QML-only Nova control pilot from stock Veyra 2.1.9.
.DESCRIPTION
  Requires the OFFICIAL, extracted Veyra 2.1.9 NVIDIA portable, an already
  compiled Nova pilot and Python 3.10+. NO NVIDIA SDK or Veyra recompilation.
  Copies whitelisted runtime folders to a NEW local directory and injects a
  zero-pixel NovaControl QML file into the copy of Main.qml. Nothing in the
  original official Veyra installation is modified.
.EXAMPLE
  .\tools\Stage-NovaVeyraQml.ps1 -OfficialPortable C:\Veyra219-NVIDIA -NovaExe C:\Nova\XuperTVNovaQt.exe -Output C:\NovaQmlTest -CheckOnly
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$OfficialPortable,
  [Parameter(Mandatory=$true)][string]$NovaExe,
  [Parameter(Mandatory=$true)][string]$Output,
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$donor=[IO.Path]::GetFullPath($OfficialPortable)
$nova=[IO.Path]::GetFullPath($NovaExe)
$out=[IO.Path]::GetFullPath($Output)
$officialExe=Join-Path $donor 'veyra_qml_ui.exe'
$main=Join-Path $donor 'qml\Veyra\Main.qml'
$bridge=Join-Path $repo 'integrations\veyra\qml\NovaControl.qml'
$config=Join-Path $repo 'integrations\veyra\qml\NovaBridgeConfig.js'
$proxy=Join-Path $repo 'tools\nova_veyra_qml_proxy.py'
$expectedOfficialExeSha='c8a53dc778331c696f5a488400885ef842adb849e1b22a9fbea07520a9ecb8f0'
foreach($file in @($officialExe,$main,$bridge,$config,$proxy,$nova,
  (Join-Path $donor 'qt.conf'),(Join-Path $donor 'platforms\qwindows.dll'))) {
  if(-not (Test-Path -LiteralPath $file -PathType Leaf)) {
    throw "Required official runtime/bridge file missing: $file"
  }
}
$actual=(Get-FileHash -LiteralPath $officialExe -Algorithm SHA256).Hash.ToLowerInvariant()
if($actual -ne $expectedOfficialExeSha) {
  throw "Not an approved original Veyra 2.1.9 EXE ($actual). Do not stage mismatched versions."
}
$mainText=[IO.File]::ReadAllText($main)
$anchor=[regex]::Match($mainText,'(?m)^Window \{\r?\n[ \t]+id: root')
if(-not $anchor.Success -or $mainText.Contains('NovaControl {')) {
  throw 'Main.qml does not match the reviewed Veyra 2.1.9 source.'
}
if($out.TrimEnd('\').Equals($donor.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase) -or
   $out.StartsWith($donor.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or
   $out.StartsWith((Split-Path -Parent $nova).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
  throw 'Output must be outside Veyra and Nova original folders.'
}
if(Test-Path -LiteralPath $out) { throw "Output already exists: $out" }
if($CheckOnly) {
  Write-Output 'Stock Veyra 2.1.9 QML pilot can be staged into a fresh folder.'
  Write-Output 'The QML-only network control is still EXPERIMENTAL and untested on RTX 3060.'
  return
}
New-Item -ItemType Directory -Path $out | Out-Null
$stage=Join-Path $out 'Veyra'
New-Item -ItemType Directory -Path $stage | Out-Null
# Keep runtime DLLs, QML, models and notices from the official distribution.
# No app profiles, videos, logs, user captures or configuration files.
$folders=@('qml','platforms','QtQuick','QtQml','QtGraphicalEffects',
  'imageformats','iconengines','styles','tls','translations','runtime',
  'runtime_local','licenses','assets','resources','networkinformation','media')
foreach($name in $folders) {
  $src=Join-Path $donor $name
  if(Test-Path -LiteralPath $src -PathType Container) {
    Copy-Item -LiteralPath $src -Destination (Join-Path $stage $name) -Recurse -Force
  }
}
Get-ChildItem -LiteralPath $donor -File | Where-Object {
  $_.Extension -ieq '.dll' -or $_.Name -eq 'qt.conf'
} | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $stage -Force }
Copy-Item -LiteralPath $officialExe -Destination (Join-Path $stage 'veyra_qml_ui.exe')
$qmlDest=Join-Path $stage 'qml\Veyra'
$destMain=Join-Path $qmlDest 'Main.qml'
# Insert exactly one local, zero-pixel Item. The rest of Veyra's QML is untouched.
$modified=$anchor.Value.Replace('Window {','Window {'+[Environment]::NewLine+'    NovaControl { }')
$mainText=$mainText.Substring(0,$anchor.Index)+$modified+$mainText.Substring($anchor.Index+$anchor.Length)
[IO.File]::WriteAllText($destMain,$mainText,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath $bridge -Destination (Join-Path $qmlDest 'NovaControl.qml')
Copy-Item -LiteralPath $config -Destination (Join-Path $qmlDest 'NovaBridgeConfig.js')
Copy-Item -LiteralPath $proxy -Destination (Join-Path $out 'nova_veyra_qml_proxy.py')
# This launcher leaves all permanent Nova/Veyra configs untouched and passes
# only local executable paths to the Python proxy. Network signed URLs
# require separate explicit opt-in; they are NOT enabled here.
$launcher=@'
$ErrorActionPreference='Stop'
$python=(Get-Command python.exe -ErrorAction Stop).Source
$env:NOVA_VEYRA_PILOT='1'
$env:NOVA_VEYRA_EXE=$python
$env:NOVA_VEYRA_PROXY_SCRIPT=Join-Path $PSScriptRoot 'nova_veyra_qml_proxy.py'
$env:NOVA_VEYRA_QML_ORIGINAL_EXE=Join-Path $PSScriptRoot 'Veyra\veyra_qml_ui.exe'
$env:NOVA_VEYRA_QML_ROOT=Join-Path $PSScriptRoot 'Veyra\qml\Veyra'
$env:NOVA_VEYRA_ALLOW_NETWORK='1'
$env:NOVA_VEYRA_ALLOW_SIGNED_NETWORK='0'
$nova='__NOVA_EXE__'
Set-Location (Split-Path -Parent $nova)
& $nova
'@
$launcher=$launcher.Replace('__NOVA_EXE__',$nova.Replace("'","''"))
Set-Content -LiteralPath (Join-Path $out 'Start-NovaVeyraQml.ps1') -Value $launcher -Encoding UTF8
Write-Host "Experimental QML-only Nova pilot staged at $out"
Write-Host 'Run Start-NovaVeyraQml.ps1; Nova uses the ORIGINAL Veyra EXE from this COPY.'
Write-Host 'No Veyra recompilation needed; use only PUBLIC URLs or local files for first smoke.'
Write-Host 'The QML-only HTTP bridge is experimental; do not merge or publish the runtime copy.'
