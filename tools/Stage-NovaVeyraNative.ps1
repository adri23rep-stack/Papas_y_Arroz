<#
.SYNOPSIS
  Stage original Veyra EngineController DLL with Nova in a NEW isolated folder.
.DESCRIPTION
  Source must be a real MSVC-built nova_veyra_native.dll; never Veyra.exe or
  veyra_qml_ui.exe. Exact DLL dependencies, shaders and GPL/MIT notices are
  verified, and existing Nova user state/cookies are not copied.
  This script never downloads NVIDIA assets or changes the original app.
#>
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$NovaDirectory,
 [Parameter(Mandatory=$true)][string]$NativeBuildDirectory,
 [Parameter(Mandatory=$true)][string]$VeyraSource,
 [Parameter(Mandatory=$true)][string]$PortableVeyraDirectory,
 [Parameter(Mandatory=$true)][string]$FfmpegRoot,
 [Parameter(Mandatory=$true)][string]$Output,
 [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$nova=[IO.Path]::GetFullPath($NovaDirectory).TrimEnd('\')
$build=[IO.Path]::GetFullPath($NativeBuildDirectory).TrimEnd('\')
$veyra=[IO.Path]::GetFullPath($PortableVeyraDirectory).TrimEnd('\')
$source=[IO.Path]::GetFullPath($VeyraSource).TrimEnd('\')
$ffmpeg=[IO.Path]::GetFullPath($FfmpegRoot).TrimEnd('\')
$out=[IO.Path]::GetFullPath($Output).TrimEnd('\')
$dll=Join-Path $build 'nova_veyra_native.dll'
$smoke=Join-Path $build 'nova_veyra_native_smoke.exe'
$shaders=Join-Path $build 'shaders'
$expected=@(
 (Join-Path $nova 'XuperTVNovaQt.exe'), $dll, $smoke,
 (Join-Path $shaders 'ScaleBlit.dxil'),
 (Join-Path $source 'LICENSE'),
 (Join-Path $source 'THIRD_PARTY_NOTICES.md'),
 (Join-Path $repo 'LICENSE'),
 (Join-Path $repo 'LICENSE.MIT-UPSTREAM'),
 (Join-Path $veyra 'veyra_qml_ui.exe')
)
foreach($file in $expected){
 if(-not(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing required local file: $file"}
}
if(-not(Get-Command 'dumpbin.exe' -ErrorAction SilentlyContinue)){
 throw 'Run in VS 2022 x64 Native Tools terminal: dumpbin.exe is required to audit DLL dependencies.'
}
if(Test-Path -LiteralPath $out){
 throw "Output must be a NEW nonexistent folder (nothing touched): $out"
}
foreach($parent in @($nova,$veyra,$source,$build,$ffmpeg)){
 if($out.Equals($parent,[StringComparison]::OrdinalIgnoreCase) -or
    $out.StartsWith($parent+'\',[StringComparison]::OrdinalIgnoreCase) -or
    $parent.StartsWith($out+'\',[StringComparison]::OrdinalIgnoreCase)){
    throw 'Output may not be inside, contain or overwrite any input folder.'
 }
}
# Do not stage a random DLL with a plausible filename. The source-linked
# Veyra native module must export the whole stable ABI expected by Nova.
$exportText=(& dumpbin.exe /EXPORTS $dll 2>&1 | Out-String)
if($LASTEXITCODE -ne 0){throw "Unable to inspect native Veyra DLL exports: $dll"}
$requiredExports=@(
 'nova_veyra_abi','nova_veyra_create','nova_veyra_destroy',
 'nova_veyra_open','nova_veyra_stop','nova_veyra_pause',
 'nova_veyra_seek','nova_veyra_volume','nova_veyra_rate',
 'nova_veyra_settings','nova_veyra_snapshot','nova_veyra_flow_snapshot',
 'nova_veyra_audio_count','nova_veyra_audio_at','nova_veyra_select_audio'
)
foreach($name in $requiredExports){
 $rx='(?m)^\s*\d+\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+'+[regex]::Escape($name)+'(?:\s|$)'
 if($exportText -notmatch $rx){
   throw "Native Veyra DLL missing required C ABI symbol: $name"
 }
}
# Follow the entire PE import tree: FFmpeg DLLs themselves depend on more
# FFmpeg/codec libraries. A direct-only check can produce a broken ZIP.
$system='^(api-ms-win-|ext-ms-win-|kernel32\.dll$|user32\.dll$|gdi32\.dll$|advapi32\.dll$|shell32\.dll$|ole32\.dll$|oleaut32\.dll$|combase\.dll$|bcrypt\.dll$|crypt32\.dll$|version\.dll$|ntdll\.dll$|d3d11\.dll$|d3d12\.dll$|dxgi\.dll$|dxguid\.dll$|winmm\.dll$|ws2_32\.dll$|secur32\.dll$|cfgmgr32\.dll$|mfplat\.dll$|mf\.dll$|mfuuid\.dll$|mmdevapi\.dll$|avrt\.dll$|windowscodecs\.dll$|shlwapi\.dll$|setupapi\.dll$|rpcrt4\.dll$|comdlg32\.dll$)'
$dependencyPaths=@{}
$missing=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$visited=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$pending=[System.Collections.Generic.Queue[string]]::new()
$pending.Enqueue($dll)
$rootScan=$true
while($pending.Count -gt 0){
 $current=$pending.Dequeue()
 if(-not $visited.Add($current)){continue}
 $depOutput=(& dumpbin.exe /DEPENDENTS $current 2>&1 | Out-String)
 if($LASTEXITCODE -ne 0){throw "dumpbin could not inspect a DLL: $current"}
 $directImports=0
 foreach($line in ($depOutput -split "\r?\n")){
  if($line -notmatch '^\s*([a-zA-Z0-9_.+-]+\.dll)\s*$'){continue}
  ++$directImports
  $name=$Matches[1]
  if($name -match $system){continue}
  if(Test-Path -LiteralPath (Join-Path "$env:SystemRoot\System32" $name) -PathType Leaf){continue}
  if($dependencyPaths.ContainsKey($name)){continue}
  $found=$null
  foreach($folder in @($nova,(Join-Path $ffmpeg 'bin'),$veyra,(Join-Path $veyra 'bin'))){
   $candidate=Join-Path $folder $name
   if(Test-Path -LiteralPath $candidate -PathType Leaf){$found=$candidate;break}
  }
  if(-not $found){
   [void]$missing.Add($name)
  }else{
   $dependencyPaths[$name]=$found
   $pending.Enqueue($found)
  }
 }
 if($rootScan -and $directImports -eq 0){
    throw 'No PE imports found: nova_veyra_native.dll is not a valid compiled adapter.'
 }
 $rootScan=$false
}
if($missing.Count){
 foreach($name in $missing){Write-Warning "Native DLL dependency missing: $name"}
 throw 'Cannot stage Veyra GPU: required transitive PE libraries missing.'
}
$requiredShaders=(Get-ChildItem -LiteralPath $shaders -Filter '*.dxil' -File -ErrorAction Stop)
if($requiredShaders.Count -lt 15){throw "Incomplete original Veyra shader build: $shaders"}
Write-Output "Native DLL found: $dll"
Write-Output "Original GPU shaders: $($requiredShaders.Count)"
Write-Output "Non-system DLL imports resolved: $($dependencyPaths.Count)"
Write-Output 'Local NVIDIA NGX runtime is additionally loaded dynamically by Veyra.'
if($CheckOnly){Write-Output 'CheckOnly: no files copied.';return}
New-Item -ItemType Directory -Path $out -ErrorAction Stop | Out-Null
# Whitelist only install files and runtime plugin directories. NEVER copy
# local service.json, pairing data, caches, network tokens, history or settings.
$allowedFiles=@(
 'XuperTVNovaQt.exe','XuperTVDesktop.exe','XuperNativeProbe.exe',
 'XuperNativePlayer.exe','qt.conf','test.mp4','font.ttf','cacert.pem',
 'service.template.json','LICENSE','LICENSE.MIT-UPSTREAM',
 'LICENSE.nlohmann-json','COPYRIGHT.mpv','DEPENDENCIAS.md'
)
foreach($name in $allowedFiles){
 $file=Join-Path $nova $name
 if(Test-Path -LiteralPath $file -PathType Leaf){Copy-Item -LiteralPath $file -Destination $out}
}
Get-ChildItem -LiteralPath $nova -Filter '*.dll' -File | ForEach-Object {
 Copy-Item -LiteralPath $_.FullName -Destination $out
}
$allowedFolders=@('qml','platforms','generic','imageformats','iconengines',
 'networkinformation','styles','tls','translations','sqldrivers','shaders',
 'nova-gpu','licenses')
foreach($name in $allowedFolders){
 $folder=Join-Path $nova $name
 if(Test-Path -LiteralPath $folder -PathType Container){
   Copy-Item -LiteralPath $folder -Destination (Join-Path $out $name) -Recurse
 }
}
Copy-Item -LiteralPath $dll -Destination (Join-Path $out 'nova_veyra_native.dll')
Copy-Item -LiteralPath $smoke -Destination (Join-Path $out 'nova_veyra_native_smoke.exe')
Copy-Item -LiteralPath (Join-Path $repo 'tools\Test-NovaVeyraNative.ps1') -Destination (Join-Path $out 'Test-NovaVeyraNative.ps1')
# Users may explicitly import an existing catalog configuration locally.
# Do NOT copy service.json or device_state.json automatically in staging.
Copy-Item -LiteralPath (Join-Path $repo 'tools\Use-NovaVeyraCatalog.ps1') -Destination (Join-Path $out 'Use-NovaVeyraCatalog.ps1')
$targetShaders=Join-Path $out 'shaders'
if(-not(Test-Path -LiteralPath $targetShaders)){New-Item -ItemType Directory -Path $targetShaders | Out-Null}
foreach($shader in $requiredShaders){
 $dest=Join-Path $targetShaders $shader.Name
 if(Test-Path -LiteralPath $dest -PathType Leaf){
   if((Get-FileHash $dest -Algorithm SHA256).Hash -ne (Get-FileHash $shader.FullName -Algorithm SHA256).Hash){
     throw "Shader collision: $dest"
   }
 } else {Copy-Item -LiteralPath $shader.FullName -Destination $dest}
}
foreach($entry in $dependencyPaths.GetEnumerator()){
 $dest=Join-Path $out $entry.Key
 if(Test-Path -LiteralPath $dest -PathType Leaf){
   if((Get-FileHash $dest -Algorithm SHA256).Hash -ne (Get-FileHash $entry.Value -Algorithm SHA256).Hash){
     throw "Runtime DLL collision; refusing to mix Qt/FFmpeg versions: $($entry.Key)"
   }
 } else {Copy-Item -LiteralPath $entry.Value -Destination $dest}
}
# NVIDIA models and providers are NOT automatically fetched, patched, modified
# or redistributed. Copy unmodified binaries only when user already has them.
$ngxs=@(Get-ChildItem -LiteralPath $veyra -Filter 'nvngx_*.dll' -File -ErrorAction SilentlyContinue)
foreach($ng in $ngxs){
 $dest=Join-Path $out $ng.Name
 if(Test-Path -LiteralPath $dest -PathType Leaf){continue}
 Copy-Item -LiteralPath $ng.FullName -Destination $dest
}
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $out 'LICENSE') -Force
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE.MIT-UPSTREAM') -Destination $out -Force
Copy-Item -LiteralPath (Join-Path $source 'THIRD_PARTY_NOTICES.md') -Destination (Join-Path $out 'THIRD_PARTY_NOTICES.VEYRA.md')
# Veyra's NGX/FG/NR code resolves its provider models relative to the HOST
# EXE, NOT the MSVC DLL. Preserve only runtime BINARY assets and relative paths
# from the user's own licensed portable, never personal ini/json/cache data.
$extensions=@('.dll','.bin','.dat','.model','.onnx','.pak','.weights')
$runtimeCount=0
# VFG also searches runtime/nvidia-vfg (sibling of experimental).
# Carry only the user's existing licensed binary runtime, preserving paths.
foreach($relative in @('runtime\experimental','runtime\nvidia-vfg','runtime_local\nvidia')){
 $runtimeFolder=Join-Path $veyra $relative
 if(-not(Test-Path -LiteralPath $runtimeFolder -PathType Container)){continue}
 foreach($item in (Get-ChildItem -LiteralPath $runtimeFolder -File -Recurse)){
  if($item.Extension.ToLowerInvariant() -notin $extensions){continue}
  $tail=$item.FullName.Substring($veyra.Length).TrimStart('\')
  $dest=Join-Path $out $tail
  $parent=Split-Path -Parent $dest
  New-Item -ItemType Directory -Path $parent -Force | Out-Null
  Copy-Item -LiteralPath $item.FullName -Destination $dest
  ++$runtimeCount
 }
}
# VFG SDK runtime is separate from NGX/DLSS. Distinguish an actually
# complete provider directory from arbitrary NVIDIA DLLs in the package.
$runtimeRoot=if(Test-Path -LiteralPath (Join-Path $out 'runtime\experimental') -PathType Container){
 Join-Path $out 'runtime\experimental'
}else{
 Join-Path $out 'runtime_local\nvidia'
}
$vfgCandidates=@(
 (Join-Path $runtimeRoot 'vfg'),
 (Join-Path (Split-Path -Parent $runtimeRoot) 'nvidia-vfg'),
 $runtimeRoot
)
$vfgRequired=@('NVVideoEffects.dll','NVCVImage.dll','nvVFXVideoFrameGeneration.dll')
$vfgReady=$false
foreach($candidate in $vfgCandidates){
 $found=0
 foreach($name in $vfgRequired){
   if(Test-Path -LiteralPath (Join-Path $candidate $name) -PathType Leaf){++$found}
 }
 if($found -eq $vfgRequired.Count){
   Write-Output "NVIDIA VFX provider runtime complete: $candidate"
   $vfgReady=$true
   break
 }
}
if(-not $vfgReady){
 Write-Warning 'NVIDIA-VFG runtime not found. Official Video Effects SDK 1.3 provider binaries (NVVideoEffects.dll, NVCVImage.dll, nvVFXVideoFrameGeneration.dll) must exist together in the Veyra runtime folder. DLSS and Optical Flow SDK alone do not provide these.'
}
if($runtimeCount -eq 0){
 Write-Warning 'No user-provided NVIDIA runtime models staged; decoding may work but DLSS/NR/FG providers will be unavailable.'
}else{
 Write-Output "Original NVIDIA runtime binaries staged locally: $runtimeCount"
}
$launcher=@'
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$env:NOVA_VEYRA_NATIVE_DLL=Join-Path $root 'nova_veyra_native.dll'
$env:NOVA_VEYRA_NATIVE_STRICT='1'
Set-Location $root
& (Join-Path $root 'XuperTVNovaQt.exe')
'@
Set-Content -LiteralPath (Join-Path $out 'Start-NovaVeyraNative.ps1') -Value $launcher -Encoding UTF8
# Optional live beta. Default Alpha launcher remains unchanged. The beta
# only routes UNSIGNED public IPTV-org HTTP(S) sources through original Veyra;
# XuperTV streams with authentication headers remain on MPV.
$liveLauncher=@'
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$env:NOVA_VEYRA_NATIVE_DLL=Join-Path $root 'nova_veyra_native.dll'
$env:NOVA_VEYRA_NATIVE_STRICT='1'
$env:NOVA_VEYRA_NATIVE_ALLOW_NETWORK='1'
$env:NOVA_VEYRA_NATIVE_LIVE_BETA='1'
Set-Location $root
& (Join-Path $root 'XuperTVNovaQt.exe')
'@
Set-Content -LiteralPath (Join-Path $out 'Start-NovaVeyraLiveBeta.ps1') -Value $liveLauncher -Encoding UTF8
Write-Output 'Optional public live stream VFG beta launcher: Start-NovaVeyraLiveBeta.ps1'
Write-Output 'Only clean public HTTP(S) streams are attempted with Veyra. Unavailable sources use MPV.'
Write-Output "Staged NEW experimental GPLv3 Nova + Veyra at $out"
Write-Output 'Next run Start-NovaVeyraNative.ps1 with local test.mp4 and NVIDIA hardware; confirm real frame counts.'
Write-Output 'Native engine and vendor asset versions MUST still be hardware-tested; staging is not GPU validation.'
