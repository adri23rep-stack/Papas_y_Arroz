<#
.SYNOPSIS
  Build Veyra's ORIGINAL GPU engine as an in-process DLL used by Nova.
.DESCRIPTION
  Unlike the earlier QML/Win32 scripts this does not patch or start Veyra.exe.
  CMake links veyra_engine, which owns FFmpeg + D3D12 + NGX + NVOF + shaders.
  Requires official Veyra 2.1.9 source, MSVC2022 x64, NVIDIA SDK and
  compatible Windows x64 FFmpeg development prefix. NO user secrets.
  Run PowerShell from VS2022 x64 Native Tools Command Prompt.
  The official source is GPLv3; obey GPLv3 for any distributed combined work.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$VeyraSource,
  [Parameter(Mandatory=$true)][string]$FfmpegRoot,
  [Parameter(Mandatory=$true)][string]$DlssSdkRoot,
  [Parameter(Mandatory=$true)][string]$OpticalFlowSdkRoot,
  [Parameter(Mandatory=$true)][string]$NvencHeadersRoot,
  [string]$BuildDirectory = '',
  [switch]$CheckOnly,
  [switch]$ConfigureOnly,
  [switch]$DisableDeinterlace
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$source=[IO.Path]::GetFullPath($VeyraSource)
$ffmpeg=[IO.Path]::GetFullPath($FfmpegRoot)
$dlss=[IO.Path]::GetFullPath($DlssSdkRoot)
$nvof=[IO.Path]::GetFullPath($OpticalFlowSdkRoot)
$nvenc=[IO.Path]::GetFullPath($NvencHeadersRoot)
$build=if($BuildDirectory){[IO.Path]::GetFullPath($BuildDirectory)}else{Join-Path $source 'out\nova-native-gpu'}
$missing=New-Object 'System.Collections.Generic.List[string]'
$req=@(
  (Join-Path $source 'CMakeLists.txt'),
  (Join-Path $source 'LICENSE'),
  (Join-Path $source 'src\engine\EngineController.cpp'),
  (Join-Path $source 'src\pipeline\EnhanceGraph.cpp'),
  (Join-Path $source 'src\ngx\DlssFgBackend.cpp'),
  (Join-Path $source 'src\ngx\DlssSrBackend.cpp'),
  (Join-Path $ffmpeg 'include\libavformat\avformat.h'),
  (Join-Path $ffmpeg 'include\libavcodec\avcodec.h'),
  (Join-Path $ffmpeg 'include\libavutil\avutil.h'),
  (Join-Path $ffmpeg 'lib\avformat.lib'),
  (Join-Path $ffmpeg 'lib\avcodec.lib'),
  (Join-Path $dlss 'include\nvsdk_ngx.h'),
  (Join-Path $dlss 'lib\Windows_x86_64\x64\nvsdk_ngx_s.lib'),
  (Join-Path $nvof 'NvOFInterface\nvOpticalFlowCommon.h'),
  (Join-Path $nvof 'Common\NvOFBase\NvOFBase.h'),
  (Join-Path $nvenc 'include\ffnvcodec\nvEncodeAPI.h')
)
if(-not $DisableDeinterlace){
  $req+=Join-Path $ffmpeg 'include\libavfilter\avfilter.h'
}
foreach($p in $req){if(-not(Test-Path -LiteralPath $p -PathType Leaf)){$missing.Add($p)}}
foreach($exe in @('cmake.exe','ninja.exe','cl.exe')){
  if(-not(Get-Command $exe -ErrorAction SilentlyContinue)){$missing.Add("$exe missing from PATH")}
}
if(-not $missing.Count){
  $cmakeText=[IO.File]::ReadAllText((Join-Path $source 'CMakeLists.txt'))
  if($cmakeText -notmatch '(?m)^project\(veyra VERSION 2[.]1[.]9 LANGUAGES C CXX RC\)'){
    throw 'Expected official Veyra 2.1.9 source tree. Do not compile a different engine version silently.'
  }
}
if($missing.Count){
  Write-Warning ("Veyra native GPU cannot compile: "+$missing.Count+" missing prerequisites.")
  foreach($item in $missing){Write-Output " - $item"}
  if($CheckOnly){return}
  throw 'Stopped before configuring or changing source files.'
}
if($CheckOnly){
  Write-Output 'Required files/toolchain present. Compilation/GPU functionality NOT tested.'
  return
}
$cmakeSource=Join-Path $repo 'integrations\veyra\native'
$cmakeArgs=@(
  '-S',$cmakeSource,'-B',$build,'-G','Ninja',
  '-DCMAKE_BUILD_TYPE=Release',
  ("-DVEYRA_SOURCE_ROOT="+$source),
  ("-DVEYRA_FFMPEG_ROOT="+$ffmpeg),
  ("-DVEYRA_DLSS_SDK_ROOT="+$dlss),
  ("-DVEYRA_NVOF_SDK_ROOT="+$nvof),
  ("-DVEYRA_NVENC_HEADERS_ROOT="+$nvenc),
  '-DVEYRA_ENABLE_DLSS_SR=ON',
  '-DVEYRA_ENABLE_DLSS_FG=ON',
  '-DVEYRA_ENABLE_EXPERIMENTAL_DLSSNR=ON',
  '-DVEYRA_ENABLE_NVOF=ON',
  ('-DVEYRA_ENABLE_DEINTERLACE='+$(if($DisableDeinterlace){'OFF'}else{'ON'}))
)
& cmake.exe @cmakeArgs
if($LASTEXITCODE -ne 0){throw 'Veyra native engine CMake configure failed.'}
if($ConfigureOnly){Write-Output 'Configured. No GPU DLL compiled yet.';return}
& cmake.exe --build $build --target nova_veyra_native --parallel 2
if($LASTEXITCODE -ne 0){throw 'Original Veyra GPU engine failed compilation/linking.'}
$dll=Join-Path $build 'nova_veyra_native.dll'
if(-not(Test-Path -LiteralPath $dll -PathType Leaf)){
  throw "Build reported success but missing native DLL: $dll"
}
Write-Output "Compiled Veyra GPU module: $dll"
Write-Output 'Do NOT copy DLL alone: the original shaders, FFmpeg, NVIDIA runtime files and licenses are needed.'
Write-Output 'Requires real RTX 3060 decode, FG/SR/NR, memory usage and stream test before default activation.'
