<#
.SYNOPSIS
  RESEARCH ONLY: Win32 Veyra 2.1.9 cannot supply the Qt QML runtime.
.DESCRIPTION
  Creates a NEW local folder, preserving original Veyra and Nova installs.
  Does not publish anything. Built shaders replace vendor shader files.
  Requires an already-built patched QML Veyra executable.
.EXAMPLE
  .\tools\Stage-NovaVeyraLocal.ps1 -NovaExe C:\NovaPilot\XuperTVNovaQt.exe -VeyraBuild C:\VeyraSource\out\nova-veyra-bridge -OfficialVeyraPortable C:\Veyra219-NVIDIA -Output C:\NovaVeyraTest -CheckOnly
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$NovaExe,
  [Parameter(Mandatory=$true)][string]$VeyraBuild,
  [Parameter(Mandatory=$true)][string]$OfficialVeyraPortable,
  [Parameter(Mandatory=$true)][string]$Output,
  [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$nova = [IO.Path]::GetFullPath($NovaExe)
$build = [IO.Path]::GetFullPath($VeyraBuild)
$donor = [IO.Path]::GetFullPath($OfficialVeyraPortable)
$out = [IO.Path]::GetFullPath($Output)
$patched = Join-Path $build 'veyra_qml_ui.exe'
$cache = Join-Path $build 'CMakeCache.txt'
# Official portable ships only Veyra.exe, and does not contain Qt QML files.
# Cannot safely use it as a complete runtime donor for a separately-built QML UI.
$stockExe = Join-Path $donor 'Veyra.exe'
if (Test-Path -LiteralPath $stockExe -PathType Leaf) {
  $reason = 'The official Veyra 2.1.9 portable is Win32 (Veyra.exe) with no QML runtime. This staging recipe cannot produce a working Qt Veyra build from it. Build/deploy the Qt UI and runtime separately. No files changed.'
  if ($CheckOnly) { Write-Output $reason; return }
  throw $reason
}
$original = Join-Path $donor 'veyra_qml_ui.exe'
$shaderDir = Join-Path $build 'shaders'
# Published in Veyra's corresponding source, docs/BUILD_2.1.9.md.
$expectedOfficialExeSha = 'c8a53dc778331c696f5a488400885ef842adb849e1b22a9fbea07520a9ecb8f0'
$required = @(
  $nova, $patched, $cache, $original,
  (Join-Path $donor 'qt.conf'),
  (Join-Path $donor 'qml\Veyra\Main.qml'),
  (Join-Path $donor 'platforms\qwindows.dll')
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if (-not (Test-Path -LiteralPath $shaderDir -PathType Container)) { $missing += $shaderDir }
if ($missing.Count) {
  Write-Warning 'Missing required file(s):'
  foreach ($file in $missing) { Write-Output " - $file" }
  if ($CheckOnly) { return }
  throw 'Runtime staging blocked. Prepare the official portable and patched build.'
}
$actualSha = (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha -ne $expectedOfficialExeSha) {
  throw "Official Veyra 2.1.9 executable mismatch: $actualSha. Refusing unknown runtimes."
}
$cacheText = [IO.File]::ReadAllText($cache)
foreach ($flag in @('VEYRA_BUILD_QML_UI','VEYRA_ENABLE_EXPERIMENTAL_DLSSNR',
                    'VEYRA_ENABLE_DLSS_SR','VEYRA_ENABLE_DLSS_FG','VEYRA_ENABLE_NVOF')) {
  $pattern = '(?m)^' + [regex]::Escape($flag) + ':BOOL=ON\r?$'
  if ($cacheText -notmatch $pattern) {
    throw "GPU build flag $flag is not ON. Refusing a reduced build."
  }
}
foreach ($root in @((Split-Path -Parent $nova),$build,$donor)) {
  if ($out.TrimEnd('\').Equals($root.TrimEnd('\'),[StringComparison]::OrdinalIgnoreCase) -or
      $out.StartsWith($root.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be outside Nova, Veyra build and official runtime folders.'
  }
}
if (Test-Path -LiteralPath $out) {
  throw "Output already exists: $out. Never overwrites a prior test."
}
if ($CheckOnly) {
  Write-Output 'Validated existing binaries, GPU flags and official 2.1.9 runtime donor.'
  Write-Output 'No GPU smoke test has been performed.'
  return
}
New-Item -ItemType Directory -Path $out | Out-Null
$engine = Join-Path $out 'Veyra'
New-Item -ItemType Directory -Path $engine | Out-Null
# Whitelist application files, not user settings, recordings or logs.
$folders = @('qml','platforms','QtQuick','QtQml','QtGraphicalEffects',
             'imageformats','iconengines','styles','tls','translations',
             'runtime','runtime_local','licenses','assets','resources',
             'networkinformation','media')
foreach ($name in $folders) {
  $from = Join-Path $donor $name
  if (Test-Path -LiteralPath $from -PathType Container) {
    Copy-Item -LiteralPath $from -Destination (Join-Path $engine $name) -Recurse -Force
  }
}
Get-ChildItem -LiteralPath $donor -File | Where-Object {
  $_.Extension -ieq '.dll' -or $_.Name -eq 'qt.conf'
} | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination $engine -Force
}
Copy-Item -LiteralPath $patched -Destination (Join-Path $engine 'veyra_qml_ui.exe')
# Shader binaries must come from the modified engine, NOT the old donor.
Copy-Item -LiteralPath $shaderDir -Destination (Join-Path $engine 'shaders') -Recurse
$launcher = @'
$env:NOVA_VEYRA_PILOT = '1'
$env:NOVA_VEYRA_EXE = Join-Path $PSScriptRoot 'Veyra\veyra_qml_ui.exe'
$env:NOVA_VEYRA_ALLOW_NETWORK = '1'
$env:NOVA_VEYRA_ALLOW_SIGNED_NETWORK = '0'
$nova = '__NOVA_EXE__'
Set-Location (Split-Path -Parent $nova)
& $nova
'@
$launcher = $launcher.Replace('__NOVA_EXE__', $nova.Replace("'", "''"))
Set-Content -LiteralPath (Join-Path $out 'Start-NovaVeyra.ps1') -Value $launcher -Encoding UTF8
Write-Host "New LOCAL Nova+Veyra test folder: $out"
Write-Host 'Run Start-NovaVeyra.ps1. Do NOT upload this bundle to GitHub.'
Write-Host 'Still unverified: Qt imports, GPU runtimes, frame generation and RTX 3060.'
