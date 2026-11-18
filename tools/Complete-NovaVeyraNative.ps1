<#
Build the original Veyra 2.1.9 GPU DLL from the user's licensed Optical Flow ZIP.
Run locally on Windows with VS2022 or VS2026 C++ build tools and Git for Windows.
Never uploads NVIDIA binaries or private Nova settings.
#>
[CmdletBinding()]
param(
 [string]$OpticalFlowZip=(Join-Path $env:USERPROFILE 'Downloads\Optical_Flow_SDK_5.0.7.zip'),
 [string]$Workspace=(Join-Path $env:USERPROFILE 'NovaVeyraNativeBuild'),
 [string]$NovaDirectory='',
 [string]$VeyraPortableDirectory='',
 [switch]$CheckOnly,
 # Alpha 1 targets progressive local MP4 playback. The optional FFmpeg
 # avfilter/deinterlace module can be enabled later with -DisableDeinterlace:$false.
 [switch]$DisableDeinterlace=$true,
 [switch]$SkipFfmpegInstall
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$root=[IO.Path]::GetFullPath($Workspace)
$source=Join-Path $root 'Veyra-2.1.9-Source'
$deps=Join-Path $root 'deps'
$of=Join-Path $deps 'Optical_Flow_SDK_5.0.7'
$public=Join-Path $deps 'public-sdk'
$dlss=Join-Path $public 'DLSS_SDK_310.7.0'
$nvenc=Join-Path $public 'nv-codec-headers-13.0'
$vcpkg=Join-Path $deps 'vcpkg'
$ff=Join-Path $deps 'installed\x64-windows'
$build=Join-Path $root 'out\nova-native-gpu'
function Need([string]$p) {
 if(-not(Test-Path -LiteralPath $p -PathType Leaf)){throw ('Missing file: '+$p)}
}
function ClonePinned([string]$url,[string]$dir,[string]$sha){
 if(-not(Test-Path -LiteralPath $dir -PathType Container)){
   & git.exe clone $url $dir
   if($LASTEXITCODE -ne 0){throw ('Clone failed: '+$url)}
 }
 & git.exe -C $dir checkout --detach $sha
 if($LASTEXITCODE -ne 0){throw ('Cannot checkout pinned source '+$sha)}
 $actual=(& git.exe -C $dir rev-parse HEAD).Trim()
 if($actual -ne $sha){throw ('Unexpected git revision: '+$actual)}
}
function SetupMSVC {
 # Supports Visual Studio 2022 and 2026, using the newest installation
 # that actually includes the x64 MSVC tools (not just Visual Studio IDE).
 $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
 Need $vswhere
 $vsroot=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
 if(-not $vsroot){
   $installed=(& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
   if($installed){
     throw "Visual Studio found at $installed but C++ Desktop MSVC x64 tools are missing. Use Visual Studio Installer > Modify > Desktop development with C++."
   }
   throw 'Visual Studio 2022/2026 not detected. Install Community or Build Tools with C++ Desktop workload.'
 }
 $vcvars=Join-Path $vsroot 'VC\Auxiliary\Build\vcvars64.bat'
 Need $vcvars
 $lines=(& cmd.exe /d /s /c ('""'+$vcvars+'" >nul && set"'))
 if($LASTEXITCODE -ne 0){throw 'MSVC x64 environment initialization failed'}
 foreach($line in $lines){
   if($line -match '^([^=][^=]*)=(.*)$'){
     [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
   }
 }
 # The user's PowerShell currently exposes MSYS2 CMake, unsuitable for
 # this Windows/MSVC CMake build. Prefer Visual Studio's native Windows CMake.
 $cmakeBin=Join-Path $vsroot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
 $ninjaBin=Join-Path $vsroot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
 $cmakeExe=Join-Path $cmakeBin 'cmake.exe'
 if(Test-Path -LiteralPath $cmakeExe -PathType Leaf){$env:Path="$cmakeBin;$env:Path"}
 $ninjaExe=Join-Path $ninjaBin 'ninja.exe'
 if(Test-Path -LiteralPath $ninjaExe -PathType Leaf){$env:Path="$ninjaBin;$env:Path"}
 foreach($name in @('cl.exe','cmake.exe','ninja.exe')){
   if(-not(Get-Command $name -ErrorAction SilentlyContinue)){
     throw "Missing $name after initializing MSVC. Install Visual Studio C++ Desktop workload, CMake and Ninja."
   }
 }
 $compiler=Get-Command cl.exe -ErrorAction Stop
 $cmake=Get-Command cmake.exe -ErrorAction Stop
 $ninja=Get-Command ninja.exe -ErrorAction Stop
 if($cmake.Source -match '(?i)[\\/]msys2[\\/]|[\\/]usr[\\/]bin[\\/]cmake\.exe$'){
   throw "MSYS2 CMake is first on PATH: $($cmake.Source). Install Visual Studio's C++ CMake tools for Windows, or add native Windows CMake before MSYS2."
 }
 Write-Host "Detected MSVC: $($compiler.Source)"
 Write-Host "Detected CMake: $($cmake.Source)"
 Write-Host "Detected Ninja: $($ninja.Source)"
}
$extract=Join-Path $repo 'tools\Prepare-NovaVeyraOpticalFlow.ps1'
Need $extract
& $extract -ZipPath $OpticalFlowZip -Destination $of -CheckOnly
if($CheckOnly){
 SetupMSVC
 Write-Output 'SDK ZIP and Windows/MSVC toolchain valid; no downloads or disk changes made.'
 return
}
if(-not(Get-Command git.exe -ErrorAction SilentlyContinue)){throw 'Install Git for Windows first'}
New-Item -ItemType Directory -Path $root -Force | Out-Null
$logging=$false
try{
 Start-Transcript -Path (Join-Path $root 'build.log') -Append | Out-Null
 $logging=$true
 if(-not(Test-Path -LiteralPath $of -PathType Container)){
   & $extract -ZipPath $OpticalFlowZip -Destination $of
 }
 Need (Join-Path $of 'NvOFInterface\nvOpticalFlowD3D12.h')
 Need (Join-Path $of 'Common\NvOFBase\NvOF.h')
 SetupMSVC
 ClonePinned 'https://github.com/Likely7/Veyra-NRVideo.git' $source 'bf596e11112ab350e66d91dfb9c7ea742c254fb4'
 if(-not(Test-Path (Join-Path $dlss 'include\nvsdk_ngx.h')) -or
    -not(Test-Path (Join-Path $nvenc 'include\ffnvcodec\nvEncodeAPI.h'))){
   if((Test-Path $dlss) -or (Test-Path $nvenc)){
     throw 'Public SDK folder is incomplete; move that folder aside before retrying'
   }
   & (Join-Path $repo 'tools\Fetch-NovaVeyraPublicSDKs.ps1') -Destination $public
 }
 Need (Join-Path $dlss 'lib\Windows_x86_64\x64\nvsdk_ngx_s.lib')
 if(-not(Test-Path (Join-Path $ff 'lib\avcodec.lib'))){
   if($SkipFfmpegInstall){throw ('FFmpeg development SDK missing: '+$ff)}
   ClonePinned 'https://github.com/microsoft/vcpkg.git' $vcpkg '30ef65cad98f08e7197c9a1656fbd871bcb72f2d'
   $exe=Join-Path $vcpkg 'vcpkg.exe'
   if(-not(Test-Path -LiteralPath $exe)){
     & (Join-Path $vcpkg 'bootstrap-vcpkg.bat') -disableMetrics
     if($LASTEXITCODE -ne 0){throw 'vcpkg bootstrap failed'}
   }
   $features=if($DisableDeinterlace){
     'ffmpeg[avcodec,avformat,swresample,swscale]:x64-windows'
   }else{
     'ffmpeg[avcodec,avformat,avfilter,swresample,swscale]:x64-windows'
   }
   Push-Location $root
   try{
     & $exe install $features --x-install-root=(Join-Path $deps 'installed')
     if($LASTEXITCODE -ne 0){throw 'FFmpeg build failed; inspect build.log'}
   }finally{Pop-Location}
 }
 $buildArgs=@{
   VeyraSource=$source
   FfmpegRoot=$ff
   DlssSdkRoot=$dlss
   OpticalFlowSdkRoot=$of
   NvencHeadersRoot=$nvenc
   BuildDirectory=$build
 }
 if($DisableDeinterlace){$buildArgs.DisableDeinterlace=$true}
 & (Join-Path $repo 'tools\Build-NovaVeyraNative.ps1') @buildArgs -CheckOnly
 & (Join-Path $repo 'tools\Build-NovaVeyraNative.ps1') @buildArgs
 $dll=Join-Path $build 'nova_veyra_native.dll'
 Need $dll
 Write-Host ('Veyra original GPU DLL compiled: '+$dll)
 # Prefer the exact pilot folders used in the user's previous Windows tests.
 # The defaults do not search documents, credentials, caches or user profiles.
 $downloads=Join-Path $env:USERPROFILE 'Downloads'
 if(-not $VeyraPortableDirectory){
   $defaultPortable=Join-Path $downloads 'Veyra-2.1.9-NVIDIA-win64-portable'
   if(Test-Path -LiteralPath (Join-Path $defaultPortable 'veyra_qml_ui.exe') -PathType Leaf){
      $VeyraPortableDirectory=$defaultPortable
      Write-Host ('Veyra portable found: '+$defaultPortable)
   }
 }
 if(-not $NovaDirectory){
   $defaultPilot=Join-Path $downloads 'Nova-Veyra-Windows-Pilot'
   if(Test-Path -LiteralPath $defaultPilot -PathType Container){
     $novaCandidates=@(Get-ChildItem -LiteralPath $defaultPilot -Recurse -File -Filter 'XuperTVNovaQt.exe' -ErrorAction SilentlyContinue)
     if($novaCandidates.Count -eq 1){
        $NovaDirectory=$novaCandidates[0].DirectoryName
        Write-Host ('Nova executable found: '+$novaCandidates[0].FullName)
     }elseif($novaCandidates.Count -gt 1){
        Write-Warning 'More than one Nova EXE found; specify NovaDirectory to avoid staging the wrong build.'
     }
   }
 }
 if($NovaDirectory -and $VeyraPortableDirectory){
   $stage=Join-Path $root ('NovaVeyraNative-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
   $staging=@{
     NovaDirectory=$NovaDirectory; NativeBuildDirectory=$build;
     VeyraSource=$source; PortableVeyraDirectory=$VeyraPortableDirectory;
     FfmpegRoot=$ff; Output=$stage
   }
   & (Join-Path $repo 'tools\Stage-NovaVeyraNative.ps1') @staging -CheckOnly
   & (Join-Path $repo 'tools\Stage-NovaVeyraNative.ps1') @staging
   Write-Host ('Native Nova launcher: '+(Join-Path $stage 'Start-NovaVeyraNative.ps1'))
 }else{
   Write-Host 'GPU DLL compiled but Nova packaging not run: supply -NovaDirectory and -VeyraPortableDirectory if their locations differ from the Downloads pilot folders.'
 }
 Write-Host 'Real RTX 3060 playback and actual FPS still need a hardware test on this PC.'
}finally{if($logging){Stop-Transcript | Out-Null}}
