#requires -Version 5.1
<#
Build XuperTV Nova locally on Windows. Installs only the public MSYS2 UCRT64
toolchain via WinGet (if needed), then uses the project's checked-in shell
script to install packages, compile C++ and deploy a portable Qt executable.
No releases, remote execution scripts, account secrets or git branches.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Run
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $root 'ui\novaqt\Main.qml'
if (-not (Test-Path $source) -or -not (Test-Path (Join-Path $root 'CMakeLists.txt'))) {
    throw 'Ejecuta este archivo dentro de la carpeta del proyecto Xuper-Tv con sus fuentes completas.'
}

function Find-Msys2Bash {
    $candidates = @(
        "$env:SystemDrive\msys64\usr\bin\bash.exe",
        "$env:SystemDrive\tools\msys64\usr\bin\bash.exe",
        "$env:LOCALAPPDATA\Programs\MSYS2\usr\bin\bash.exe",
        "$env:USERPROFILE\scoop\apps\msys2\current\usr\bin\bash.exe"
    )
    foreach ($file in $candidates) {
        if (Test-Path $file) { return $file }
    }
    return $null
}

Write-Host '[NOVA] Preparando el compilador de Windows...' -ForegroundColor Cyan
$bash = Find-Msys2Bash
if (-not $bash) {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw 'Falta MSYS2 y WinGet. Instala MSYS2 desde https://www.msys2.org/ y vuelve a ejecutar este archivo.'
    }
    Write-Host '[NOVA] Instalando MSYS2 desde el catalogo oficial de WinGet...' -ForegroundColor Yellow
    & winget install --id MSYS2.MSYS2 --exact --source winget --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) {
        throw ('No se pudo instalar MSYS2 (codigo '+$LASTEXITCODE+'). Revisa el aviso de WinGet.')
    }
    $bash = Find-Msys2Bash
    if (-not $bash) {
        throw 'MSYS2 se instalo en una ruta diferente. Abre MSYS2 UCRT64 y coloca la instalacion en C:\msys64 o ajusta Find-Msys2Bash.'
    }
}

# Use the UCRT64 subsystem. Never mix a different MSYS2 runtime with the
# Windows executables or load missing DLLs from the system PATH.
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$env:MSYS2_PATH_TYPE = 'inherit'
$env:NOVA_REPO_PATH = $root
$env:NOVA_CLEAN = if ($Clean) { '1' } else { '0' }
Write-Host ('[NOVA] Fuentes: ' + $root) -ForegroundColor Cyan
Write-Host '[NOVA] Instalando dependencias y compilando con CMake/Ninja...' -ForegroundColor Cyan
# Convert a normal drive-letter Windows path to MSYS2 /c/... directly.
# Calling cygpath.exe from Windows PowerShell was failing on some machines.
# We do NOT evaluate a Bash command string, so spaces in the folder are safe.
$scriptPath = Join-Path $root 'tools\compilar-nova-ucrt64.sh'
if (-not (Test-Path -LiteralPath $scriptPath)) {
    throw 'Falta tools\compilar-nova-ucrt64.sh. Descarga la rama completa.'
}
$fullScriptPath = [System.IO.Path]::GetFullPath($scriptPath)
if ($fullScriptPath -notmatch '^([A-Za-z]):[\\/](.+)$') {
    throw 'El proyecto debe estar en una unidad local de Windows (por ejemplo C:).'
}
$driveLetter = $Matches[1].ToLowerInvariant()
$relativePath = $Matches[2].Replace('\', '/')
$msysScript = '/' + $driveLetter + '/' + $relativePath
Write-Host ('[NOVA] Script local: ' + $msysScript) -ForegroundColor DarkCyan
# Bash login loads /ucrt64/bin before compiling; no string is evaluated.
& $bash --login $msysScript
$exit = $LASTEXITCODE
if ($exit -ne 0) {
    throw ("La compilacion termino con error $exit. El texto de la consola indica el paso que fallo.")
}
$folder = Join-Path $root 'dist\Xuper-TV-NovaQt-Local'
$exe = Join-Path $folder 'XuperTVNovaQt.exe'
if (-not (Test-Path $exe)) { throw 'La compilacion termino pero no se encontro el ejecutable portable.' }
Write-Host ''
Write-Host '[NOVA] LISTO! La app esta en:' -ForegroundColor Green
Write-Host ('  ' + $exe)
Write-Host 'Los archivos service.json, library_state.json y pairing.dat existentes NO se borraron.'
if ($Run) {
    Write-Host '[NOVA] Abriendo interfaz Qt...' -ForegroundColor Cyan
    Start-Process -FilePath $exe -WorkingDirectory $folder
}
