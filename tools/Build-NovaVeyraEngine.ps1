<#
.SYNOPSIS
  Check/build a SEPARATE Veyra 2.1.9 Qt player with Nova's IPC bridge.
.DESCRIPTION
  Requires official Qt MSVC, FFmpeg, NVIDIA DLSS, Optical Flow SDK and NVENC
  headers. Does not fetch proprietary assets, modify installed Veyra, or touch main.
  Start PowerShell from "x64 Native Tools Command Prompt for VS 2022" first.
  Run with -CheckOnly before allowing changes to the source COPY.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$VeyraSource,
  [Parameter(Mandatory=$true)][string]$QtRoot,
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
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$source = [IO.Path]::GetFullPath($VeyraSource)
$qt = [IO.Path]::GetFullPath($QtRoot)
$ff = [IO.Path]::GetFullPath($FfmpegRoot)
$ngx = [IO.Path]::GetFullPath($DlssSdkRoot)
$of = [IO.Path]::GetFullPath($OpticalFlowSdkRoot)
$nvenc = [IO.Path]::GetFullPath($NvencHeadersRoot)
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $source 'out\nova-veyra-bridge' }
$failed = [System.Collections.Generic.List[string]]::new()

function RequireFile([string]$root, [string]$part, [string]$description) {
  $path = Join-Path $root $part
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    $script:failed.Add("$description missing: $path")
  } else {
    Write-Host "OK: $description"
  }
}
function RequireExe([string]$name) {
  if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
    $script:failed.Add("$name not found on PATH")
  } else {
    Write-Host "OK: $name"
  }
}
RequireFile $source 'CMakeLists.txt' 'Veyra source tree'
RequireFile $source 'apps\veyra-qml\main.cpp' 'Veyra Qt main'
RequireFile $source 'src\ui\bridge\Playback.cpp' 'Veyra playback source'
RequireFile $qt 'lib\cmake\Qt6\Qt6Config.cmake' 'Qt MSVC libraries'
RequireFile $qt 'bin\windeployqt6.exe' 'Qt deployer'
RequireFile $ff 'include\libavformat\avformat.h' 'FFmpeg demux'
RequireFile $ff 'include\libavcodec\avcodec.h' 'FFmpeg decode'
RequireFile $ff 'share\ffmpeg\FFMPEGConfig.cmake' 'FFmpeg CMake package'
RequireFile $ngx 'include\nvsdk_ngx.h' 'NVIDIA DLSS SDK header'
RequireFile $ngx 'lib\Windows_x86_64\x64\nvsdk_ngx_s.lib' 'NVIDIA DLSS SDK static library'
RequireFile $of 'NvOFInterface\nvOpticalFlowCommon.h' 'NVIDIA Optical Flow header'
RequireFile $of 'Common\NvOFBase\NvOFBase.h' 'NVIDIA Optical Flow base header'
RequireFile $nvenc 'include\ffnvcodec\nvEncodeAPI.h' 'NVENC SDK header'
if (-not $DisableDeinterlace) {
  RequireFile $ff 'include\libavfilter\avfilter.h' 'FFmpeg deinterlace module'
} else {
  Write-Warning 'Deinterlacing explicitly disabled for this build.'
}
RequireExe 'cmake.exe'
RequireExe 'ninja.exe'
RequireExe 'cl.exe'
RequireExe 'python.exe'
if ($failed.Count -gt 0) {
  Write-Host ''
  Write-Warning ('Cannot build Veyra GPU yet (' + $failed.Count + ' missing dependencies):')
  foreach ($item in $failed) { Write-Host " - $item" }
  if ($CheckOnly) { return }
  throw 'Veyra build blocked. Supply official dependencies; no source was changed.'
}
if ($CheckOnly) {
  Write-Host 'All configured paths exist. This is a preflight, NOT proof of a successful build.'
  return
}

$patcher = Join-Path $repo 'tools\Apply-VeyraNovaBridge.py'
if (-not (Test-Path -LiteralPath $patcher -PathType Leaf)) {
  throw "Nova source patcher missing: $patcher"
}
& python.exe $patcher --check $source
if ($LASTEXITCODE -ne 0) { throw 'Veyra source patch check failed; no build started.' }
& python.exe $patcher $source
if ($LASTEXITCODE -ne 0) { throw 'Could not install Nova bridge into the source copy.' }

$args = @(
  '-S', $source, '-B', $build, '-G', 'Ninja',
  '-DCMAKE_BUILD_TYPE=Release',
  '-DVEYRA_BUILD_QML_UI=ON',
  '-DVEYRA_ENABLE_D3D12_DEBUG=OFF',
  '-DVEYRA_ENABLE_EXPERIMENTAL_DLSSNR=ON',
  '-DVEYRA_ENABLE_DLSS_SR=ON',
  '-DVEYRA_ENABLE_DLSS_FG=ON',
  '-DVEYRA_ENABLE_NVOF=ON',
  ('-DVEYRA_ENABLE_DEINTERLACE=' + $(if($DisableDeinterlace){'OFF'}else{'ON'})),
  "-DCMAKE_PREFIX_PATH=$qt",
  "-DVEYRA_FFMPEG_ROOT=$ff",
  "-DVEYRA_DLSS_SDK_ROOT=$ngx",
  "-DVEYRA_NVOF_SDK_ROOT=$of",
  "-DVEYRA_NVENC_HEADERS_ROOT=$nvenc"
)
Write-Host 'Configuring the Veyra Qt player with the Nova bridge in a separate build tree...'
& cmake.exe @args
if ($LASTEXITCODE -ne 0) { throw 'Veyra CMake configuration failed; inspect the errors above.' }
if ($ConfigureOnly) {
  Write-Host 'Configuration succeeded. Build not started (ConfigureOnly).'
  return
}
& cmake.exe --build $build --config Release --target veyra_qml_ui --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Veyra compiler/linker failed. Installed Veyra unchanged.' }
$exe = Join-Path $build 'veyra_qml_ui.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
  throw "CMake reported success, but missing: $exe"
}
Write-Host 'Veyra Qt binary compiled with Nova bridge:'
Write-Host $exe
Write-Host 'Not portable yet: deploy Qt QML imports, FFmpeg, NVIDIA runtimes separately.'
Write-Host 'Use tools/Run-NovaVeyraSmoke.ps1 for the first local test.'
