<#
.SYNOPSIS
  Run an opt-in Nova + patched Veyra smoke session.
.DESCRIPTION
  Does not install anything, change service.json, or touch main.
  Uses only public, explicitly selected media and installed binaries.
.EXAMPLE
  .\tools\Run-NovaVeyraSmoke.ps1 -NovaExe C:\Nova\XuperTVNovaQt.exe -VeyraExe C:\Veyra\veyra_qml_ui.exe
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$NovaExe,
  [Parameter(Mandatory=$true)][string]$VeyraExe,
  [switch]$AllowPublicNetwork,
  [switch]$AllowQueryNetwork
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Executable([string]$Value,[string]$What) {
  if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) {
    throw "$What not found: $Value"
  }
  $resolved = (Resolve-Path -LiteralPath $Value).ProviderPath
  if ([IO.Path]::GetExtension($resolved) -ine '.exe') {
    throw "$What must be a compiled Windows .exe"
  }
  return $resolved
}

$nova = Resolve-Executable $NovaExe 'Nova'
$veyra = Resolve-Executable $VeyraExe 'Veyra'
if ($nova -eq $veyra) { throw "Nova and Veyra must be different executables." }
if ([IO.Path]::GetFileName($nova) -ine 'XuperTVNovaQt.exe') {
  Write-Warning "Nova executable name is unexpected; verify you selected the Qt preview."
}
if ([IO.Path]::GetFileName($veyra) -ine 'veyra_qml_ui.exe') {
  Write-Warning "Veyra executable name is unexpected; verify this is the patched Qt QML build."
}
if ($AllowQueryNetwork -and -not $AllowPublicNetwork) {
  throw 'AllowQueryNetwork requires AllowPublicNetwork.'
}

$names = @(
  'NOVA_VEYRA_PILOT',
  'NOVA_VEYRA_EXE',
  'NOVA_VEYRA_ALLOW_NETWORK',
  'NOVA_VEYRA_ALLOW_SIGNED_NETWORK'
)
$old = @{}
foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$previous = (Get-Location).Path
try {
  [Environment]::SetEnvironmentVariable('NOVA_VEYRA_PILOT','1','Process')
  [Environment]::SetEnvironmentVariable('NOVA_VEYRA_EXE',$veyra,'Process')
  [Environment]::SetEnvironmentVariable('NOVA_VEYRA_ALLOW_NETWORK',
    $(if ($AllowPublicNetwork) {'1'} else {'0'}), 'Process')
  [Environment]::SetEnvironmentVariable('NOVA_VEYRA_ALLOW_SIGNED_NETWORK',
    $(if ($AllowQueryNetwork) {'1'} else {'0'}), 'Process')
  Set-Location (Split-Path -Parent $nova)
  Write-Host 'Starting experimental Nova playback. No files or settings are deleted.'
  Write-Host 'If Veyra lacks the Nova bridge, Nova should fall back to MPV.'
  & $nova
} finally {
  Set-Location $previous
  foreach ($name in $names) {
    [Environment]::SetEnvironmentVariable($name, $old[$name], 'Process')
  }
}
