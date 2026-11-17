<#
.SYNOPSIS
  Stage OPTIONAL public NVIDIA SDK headers and static DLSS libraries.
.DESCRIPTION
  Only clones official pinned GitHub sources after an explicit invocation.
  Does not install software, alter Nova, download a driver or touch main.
  The Optical Flow SDK requires its own license acceptance and is NOT fetched.
  FFmpeg/Qt/MSVC remain separate prerequisites.
.EXAMPLE
  .\tools\Fetch-NovaVeyraPublicSDKs.ps1 -Destination C:\NovaBuildDeps -CheckOnly
  .\tools\Fetch-NovaVeyraPublicSDKs.ps1 -Destination C:\NovaBuildDeps
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Destination,
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($Destination)
$items = @(
  @{
    Label = 'DLSS_SDK_310.7.0'
    Repository = 'https://github.com/NVIDIA/DLSS.git'
    Tag = 'v310.7.0'
    Required = @('include\nvsdk_ngx.h',
                 'lib\Windows_x86_64\x64\nvsdk_ngx_s.lib')
  },
  @{
    Label = 'nv-codec-headers-13.0'
    Repository = 'https://github.com/FFmpeg/nv-codec-headers.git'
    Tag = 'n13.0.19.0'
    Required = @('include\ffnvcodec\nvEncodeAPI.h')
  }
)

if ($CheckOnly) {
  foreach ($item in $items) {
    $path = Join-Path $root $item.Label
    Write-Output "$($item.Repository) ($($item.Tag)) => $path"
    foreach ($part in $item.Required) {
      if (Test-Path -LiteralPath (Join-Path $path $part) -PathType Leaf) {
        Write-Output "  OK: $part"
      } else {
        Write-Output "  MISSING: $part"
      }
    }
  }
  Write-Output 'NVIDIA Optical Flow SDK 5.0.7: download and accept terms separately from NVIDIA.'
  return
}
if (-not (Get-Command 'git.exe' -ErrorAction SilentlyContinue)) {
  throw 'Git for Windows is required to clone the two official public SDK repositories.'
}
New-Item -Path $root -ItemType Directory -Force | Out-Null
foreach ($item in $items) {
  $path = Join-Path $root $item.Label
  if (Test-Path -LiteralPath $path) {
    throw "Destination already exists, will not overwrite or merge SDKs: $path"
  }
}
foreach ($item in $items) {
  $path = Join-Path $root $item.Label
  Write-Host "Cloning verified source repository $($item.Repository) at tag $($item.Tag)"
  & git.exe -c advice.detachedHead=false clone --depth 1 --branch $item.Tag $item.Repository $path
  if ($LASTEXITCODE -ne 0) {
    throw "Git clone failed. Check or remove this NEW folder manually: $path"
  }
  foreach ($part in $item.Required) {
    $file = Join-Path $path $part
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
      throw "Official SDK checkout is incomplete; no substitute downloaded: $file"
    }
  }
}
Write-Host 'Official public DLSS and NVENC SDK files staged.'
Write-Host "DlssSdkRoot: $(Join-Path $root 'DLSS_SDK_310.7.0')"
Write-Host "NvencHeadersRoot: $(Join-Path $root 'nv-codec-headers-13.0')"
Write-Host 'Next: obtain Optical Flow SDK 5.0.7 (official NVIDIA license), Qt and FFmpeg.'
Write-Host 'Use tools/Build-NovaVeyraEngine.ps1 -CheckOnly before compiling.'
