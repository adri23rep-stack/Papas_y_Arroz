<#
.SYNOPSIS
 Stage an isolated, UNMODIFIED official Veyra.exe 2.1.9 for Nova.
.DESCRIPTION
 Requires original official Veyra NVIDIA/AMD portable, Nova pilot, Python 3.10+.
 No source edits or SDKs. Supports local file, pause, fullscreen, stop ONLY.
.EXAMPLE
 .\tools\Stage-NovaVeyraWin32.ps1 -OfficialPortable C:\Veyra219 -NovaExe C:\Nova\XuperTVNovaQt.exe -Output C:\NovaVeyraWin32Test -CheckOnly
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
$original=Join-Path $donor 'Veyra.exe'
$proxy=Join-Path $repo 'tools\nova_veyra_win32_proxy.py'
$jobs=Join-Path $repo 'tools\nova_veyra_qml_proxy.py'
$sha256='c8a53dc778331c696f5a488400885ef842adb849e1b22a9fbea07520a9ecb8f0'
foreach($file in @($original,$nova,$proxy,$jobs)) {
  if(-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required file missing: $file" }
}
if(-not (Get-Command python.exe -ErrorAction SilentlyContinue)) {
  throw 'Python 3.10+ is required for this test.'
}
$found=(Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash.ToLowerInvariant()
if($found -ne $sha256) {
  throw "Veyra.exe does not match the 2.1.9 official SHA256. Refusing unknown binary ($found)"
}
if($out.TrimEnd('\').Equals($donor.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase) -or
   $out.StartsWith($donor.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or
   $out.TrimEnd('\').Equals((Split-Path -Parent $nova).TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase) -or
   $out.StartsWith((Split-Path -Parent $nova).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
  throw 'Output must be outside original Veyra/Nova installation.'
}
if(Test-Path -LiteralPath $out) { throw "Output folder already exists: $out" }
if($CheckOnly) {
 Write-Output 'Official Win32 Veyra.exe 2.1.9 verified for isolated staging.'
 Write-Output 'Supports local media + basic buttons; no remote GPU/FPS/subtitle settings.'
 return
}
New-Item -ItemType Directory -Path $out | Out-Null
$veyradir=Join-Path $out 'VeyraWin32'
New-Item -ItemType Directory -Path $veyradir | Out-Null
foreach($folder in @('runtime','shaders','media','licenses','docs')) {
 $src=Join-Path $donor $folder
 if(Test-Path -LiteralPath $src -PathType Container) {
   Copy-Item -LiteralPath $src -Destination (Join-Path $veyradir $folder) -Recurse -Force
 }
}
$localRuntime=Join-Path $donor 'runtime_local'
$localStage=Join-Path $veyradir 'runtime_local'
New-Item -ItemType Directory -Path $localStage | Out-Null
foreach($folder in @('nvidia','intel','amd','config')) {
 $src=Join-Path $localRuntime $folder
 if(Test-Path -LiteralPath $src -PathType Container) {
   Copy-Item -LiteralPath $src -Destination (Join-Path $localStage $folder) -Recurse -Force
 }
}
Get-ChildItem -LiteralPath $donor -File | Where-Object {
 $_.Extension -ieq '.dll' -or $_.Name -in @('LICENSE','README.md','README_EN.md','THIRD_PARTY_NOTICES.md')
} | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $veyradir -Force }
Copy-Item -LiteralPath $original -Destination (Join-Path $veyradir 'Veyra.exe')
Set-Content -LiteralPath (Join-Path $veyradir 'NovaWin32Pilot.marker') -Value 'Isolated Nova experimental Veyra copy' -Encoding UTF8
Copy-Item -LiteralPath $proxy -Destination (Join-Path $out 'nova_veyra_win32_proxy.py')
Copy-Item -LiteralPath $jobs -Destination (Join-Path $out 'nova_veyra_qml_proxy.py')
$launcher=@'
$ErrorActionPreference='Stop'
$python=(Get-Command python.exe -ErrorAction Stop).Source
$env:NOVA_VEYRA_PILOT='1'
$env:NOVA_VEYRA_EXE=$python
$env:NOVA_VEYRA_PROXY_SCRIPT=Join-Path $PSScriptRoot 'nova_veyra_win32_proxy.py'
$env:NOVA_VEYRA_WIN32_EXE=Join-Path $PSScriptRoot 'VeyraWin32\Veyra.exe'
$env:NOVA_VEYRA_ALLOW_NETWORK='0'
$env:NOVA_VEYRA_ALLOW_SIGNED_NETWORK='0'
$nova='__NOVA_EXE__'
Set-Location (Split-Path -Parent $nova)
& $nova
'@
$launcher=$launcher.Replace('__NOVA_EXE__',$nova.Replace("'","''"))
Set-Content -LiteralPath (Join-Path $out 'Start-NovaVeyraWin32.ps1') -Value $launcher -Encoding UTF8
Write-Host "Created local-only experimental Nova+Win32 Veyra copy: $out"
Write-Host 'Launch Start-NovaVeyraWin32.ps1 and open a local file in Nova.'
Write-Host 'Live TV, seek, continue watching, audio/subtitles and GPU controls remain in MPV/Veyra UI.'
