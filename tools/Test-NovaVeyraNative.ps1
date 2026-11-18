# SPDX-License-Identifier: GPL-3.0-only
<#
.SYNOPSIS
  Test the original Veyra GPU pipeline on a real Windows/NVIDIA machine.
.DESCRIPTION
  Strict local file test, no network/playlist/login. Calls the compiled
  Veyra EngineController via native C ABI and reports actual engine counters.
  --fg2x requires confirmed generated frames; requests are not success.
#>
[CmdletBinding()]
param(
  [string]$Video='test.mp4',
  [switch]$Vfg2x
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$exe=Join-Path $root 'nova_veyra_native_smoke.exe'
$dll=Join-Path $root 'nova_veyra_native.dll'
$shader=Join-Path $root 'shaders\ScaleBlit.dxil'
foreach($path in @($exe,$dll,$shader)){
  if(-not(Test-Path -LiteralPath $path -PathType Leaf)){
    throw "Real Veyra GPU test cannot run; missing file: $path"
  }
}
if([IO.Path]::IsPathRooted($Video)){
  $file=[IO.Path]::GetFullPath($Video)
}else{
  $file=[IO.Path]::GetFullPath((Join-Path $root $Video))
}
if(-not(Test-Path -LiteralPath $file -PathType Leaf)){
  throw 'Place your own video named test.mp4 beside the Nova executable, or specify -Video with a local MP4 path.'
}
if([IO.Path]::GetExtension($file) -ine '.mp4'){
  throw 'First Alpha supports only a local progressive MP4 hardware smoke.'
}
Write-Host 'Testing ORIGINAL Veyra EngineController with real video and a Win32 render window.' -ForegroundColor Cyan
Write-Host 'This test will not use MPV, QML, Python or a fake GPU renderer.' -ForegroundColor Cyan
Push-Location $root
try{
  if($Vfg2x){
    & $exe $file '--fg2x'
  }else{
    & $exe $file
  }
  $code=$LASTEXITCODE
}finally{Pop-Location}
if($code -ne 0){
  throw "Veyra hardware smoke failed (exit code $code). Copy the printed transport/frames/generated counters; no fallback was used."
}
Write-Host 'Veyra GPU test PASSED with real frame counters.' -ForegroundColor Green
