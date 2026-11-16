param([Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference = 'Stop'
# The pinned RIFE DLL is built with MSVC and OpenMP. Use the x64
# redistributable files supplied by the Visual Studio Build Tools.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$root = Join-Path $vs 'VC\Redist\MSVC'
$redist = Get-ChildItem $root -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
foreach ($dll in 'msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll','vcomp140.dll') {
    $file = Get-ChildItem (Join-Path $redist.FullName 'x64') -Recurse -File -Filter $dll | Select-Object -First 1
    if (!$file) { throw "Missing redistributable dependency $dll" }
    Copy-Item -LiteralPath $file.FullName -Destination $Destination -Force
}
