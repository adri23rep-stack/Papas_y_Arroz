#requires -Version 5.1
<#
Local-only safety snapshot for an already working Nova installation.
This never modifies the source, sends files to GitHub, or copies IPTV URLs
into diagnostics. A backup cannot keep provider authorization valid.
#>
[CmdletBinding()]
param(
    [string]$SourceDirectory = '',
    [switch]$CheckOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

if (-not $SourceDirectory) {
    $SourceDirectory=Join-Path $env:USERPROFILE 'Nova-Focuz-Meta-Lab\codigo\dist\Xuper-TV-NovaQt-Local'
}
if (-not (Test-Path -LiteralPath $SourceDirectory -PathType Container)) {
    throw "No se encontro la carpeta de Nova: $SourceDirectory"
}
$source=(Resolve-Path -LiteralPath $SourceDirectory).ProviderPath
$exe=Join-Path $source 'XuperTVNovaQt.exe'
$configFile=Join-Path $source 'service.json'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw 'No es la carpeta de una instalacion Nova Qt.'
}
if (-not (Test-Path -LiteralPath $configFile -PathType Leaf)) {
    throw 'No existe service.json. No se hizo ningun cambio.'
}
try {
    $config=Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 |
        ConvertFrom-Json -ErrorAction Stop
    if ([string]::IsNullOrWhiteSpace($config.installation_serial) -or
        [string]::IsNullOrWhiteSpace($config.app_id)) {
        throw 'Faltan campos de identidad.'
    }
} catch {
    throw 'service.json no es valido; no se genero respaldo.'
}
$names=@(
    'service.json',
    'device_state.json',
    'device_state.android-tv-test.json',
    'device_state.android-test.json',
    'library_state.json',
    'playback_state.json',
    'iptv_es_cache.m3u'
)
$present=@($names | Where-Object {
    Test-Path -LiteralPath (Join-Path $source $_) -PathType Leaf
})
Write-Host ('Nova encontrada. Archivos a respaldar: '+($present -join ', '))
Write-Host 'No se copian APK, videos, ni datos a servidores.'
if ($CheckOnly) { Write-Host 'Solo comprobacion; nada cambiado.'; return }
$root=Join-Path $env:LOCALAPPDATA 'Nova-Focuz\Backups'
$name=('respaldo-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$destination=Join-Path $root $name
New-Item -ItemType Directory -Force -Path $destination | Out-Null
try {
    foreach ($file in $present) {
        Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $destination $file) -ErrorAction Stop
    }
    $manifest=@($present | ForEach-Object {
        $hash=Get-FileHash -LiteralPath (Join-Path $destination $_) -Algorithm SHA256
        [PSCustomObject]@{ File=$_; SHA256=$hash.Hash }
    })
    $manifest | ConvertTo-Json -Depth 3 |
        Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding UTF8
} catch {
    Remove-Item -LiteralPath $destination -Force -Recurse -ErrorAction SilentlyContinue
    throw
}
Write-Host "Respaldo creado localmente: $destination" -ForegroundColor Green
Write-Host 'Importante: incluye datos privados; no lo compartas ni lo subas a GitHub.'
