<#
.SYNOPSIS
  Extract the user's already-downloaded NVIDIA Optical Flow SDK 5.0.7 locally.
.DESCRIPTION
  Accepts the ORIGINAL ZIP with its Optical_Flow_SDK_5.0.7 top folder.
  Copies only the SDK headers/source needed for building Veyra; never uploads
  vendor files or overwrites a directory. Does not accept license terms
  on behalf of the user. Zip must already have been lawfully obtained.
.EXAMPLE
  .\tools\Prepare-NovaVeyraOpticalFlow.ps1 -ZipPath "$env:USERPROFILE\Downloads\Optical_Flow_SDK_5.0.7.zip" -Destination "C:\NovaVeyraDeps\Optical_Flow_SDK_5.0.7"
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$ZipPath,
  [Parameter(Mandatory=$true)][string]$Destination,
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$zip=[IO.Path]::GetFullPath($ZipPath)
$dst=[IO.Path]::GetFullPath($Destination).TrimEnd('\','/')
if(-not(Test-Path -LiteralPath $zip -PathType Leaf)){
  throw "Original Optical Flow SDK 5.0.7 ZIP not found: $zip"
}
$required=@(
 'NvOFInterface/nvOpticalFlowCommon.h',
 'NvOFInterface/nvOpticalFlowD3D12.h',
 'Common/NvOFBase/NvOF.h',
 'Common/NvOFBase/NvOFD3D12.h',
 'LicenseAgreement.pdf'
)
$prefix='Optical_Flow_SDK_5.0.7/'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
  $entries=@{}
  foreach($entry in $archive.Entries){
    $name=$entry.FullName.Replace('\','/')
    if(-not $name.StartsWith($prefix,[StringComparison]::Ordinal)){continue}
    $rel=$name.Substring($prefix.Length)
    if(-not $rel -or $rel.EndsWith('/')){continue}
    $pieces=$rel.Split('/')
    if($pieces -contains '..' -or $pieces -contains '.' -or $pieces -contains '' -or
       $rel.Contains(':') -or $rel.StartsWith('/') -or $rel.StartsWith('\')){
      throw "Unsafe ZIP entry: $rel"
    }
    $allowed=$rel -eq 'LicenseAgreement.pdf' -or
      $rel.StartsWith('NvOFInterface/',[StringComparison]::Ordinal) -or
      $rel.StartsWith('Common/NvOFBase/',[StringComparison]::Ordinal)
    if(-not $allowed){continue}
    if($entry.Length -gt 10485760){throw "Suspicious oversized SDK file: $rel"}
    if($entries.ContainsKey($rel)){throw "Duplicate SDK entry: $rel"}
    $entries[$rel]=$entry
  }
  foreach($file in $required){
    if(-not $entries.ContainsKey($file)){
      throw "Original ZIP missing required Optical Flow 5.0.7 file: $file"
    }
  }
  $sha=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
  Write-Output "Original NVIDIA Optical Flow 5.0.7 ZIP verified locally: $zip"
  Write-Output "SHA256: $sha"
  Write-Output "Required files: $($required.Count)/$($required.Count); eligible SDK files: $($entries.Count)"
  if($CheckOnly){return}
  if(Test-Path -LiteralPath $dst){
    throw "Destination already exists; refusing to overwrite any SDK or user files: $dst"
  }
  $parent=[IO.Path]::GetDirectoryName($dst)
  if(-not(Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Path $parent -Force | Out-Null}
  $stage=Join-Path $parent ([IO.Path]::GetFileName($dst)+'.extracting-'+[guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $stage | Out-Null
  try{
    foreach($name in ($entries.Keys | Sort-Object)){
      $relative=$name.Replace('/',[IO.Path]::DirectorySeparatorChar)
      $target=[IO.Path]::GetFullPath((Join-Path $stage $relative))
      if(-not $target.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
        throw "ZIP entry escapes staging folder: $name"
      }
      $folder=[IO.Path]::GetDirectoryName($target)
      New-Item -ItemType Directory -Path $folder -Force | Out-Null
      $inputStream=$entries[$name].Open()
      $outputStream=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
      try{$inputStream.CopyTo($outputStream)}finally{$outputStream.Dispose();$inputStream.Dispose()}
    }
    foreach($file in $required){
      if(-not(Test-Path -LiteralPath (Join-Path $stage $file.Replace('/',[IO.Path]::DirectorySeparatorChar)) -PathType Leaf)){
        throw "Extract failed verification: $file"
      }
    }
    Move-Item -LiteralPath $stage -Destination $dst
  }finally{
    if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force}
  }
  Write-Output "NVIDIA Optical Flow SDK is ready for Veyra compilation: $dst"
  Write-Output "You do not need to download this SDK again."
}finally{$archive.Dispose()}
