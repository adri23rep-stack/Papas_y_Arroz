#requires -Version 5.1
<#
Import ONLY local configuration from an existing Nova installation into
this installed copy, with explicit user confirmation. Never uploads files.
Existing installation identity is never overwritten automatically.
#>
[CmdletBinding()]
param(
    [string]$SourceDirectory = '',
    [string]$DestinationDirectory = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(-not $DestinationDirectory){$DestinationDirectory=$PSScriptRoot}
if(-not $SourceDirectory){
    $candidate=Join-Path $env:USERPROFILE 'Nova-Focuz-Meta-Lab\codigo\dist\Xuper-TV-NovaQt-Local'
    if(Test-Path -LiteralPath (Join-Path $candidate 'service.json')){
        $SourceDirectory=$candidate
    }else{
        Add-Type -AssemblyName System.Windows.Forms
        $picker=New-Object System.Windows.Forms.OpenFileDialog
        $picker.Title='Selecciona el service.json de tu Nova que funciona'
        $picker.Filter='Configuracion de Nova (service.json)|service.json'
        if($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK){return}
        $SourceDirectory=Split-Path -Parent $picker.FileName
    }
}
$source=(Resolve-Path -LiteralPath $SourceDirectory).ProviderPath
$dest=(Resolve-Path -LiteralPath $DestinationDirectory).ProviderPath
if($source.TrimEnd('\') -ieq $dest.TrimEnd('\')){throw 'Origen y destino iguales.'}
$oldFile=Join-Path $source 'service.json'
$newFile=Join-Path $dest 'service.json'
if(!(Test-Path -LiteralPath $oldFile -PathType Leaf)){throw 'No se encontro service.json en el origen.'}
$incoming=Get-Content -LiteralPath $oldFile -Raw -Encoding UTF8 | ConvertFrom-Json
if([string]::IsNullOrWhiteSpace([string]$incoming.installation_serial) -or
   [string]::IsNullOrWhiteSpace([string]$incoming.app_id) -or
   @($incoming.portal_hosts).Count -eq 0) {
    throw 'La configuracion elegida no contiene una identidad valida.'
}
if(Test-Path -LiteralPath $newFile){
    $current=Get-Content -LiteralPath $newFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if($current.installation_serial -ne $incoming.installation_serial){
        throw 'Ya hay una identidad distinta en este Nova. No se sobrescribio.'
    }
    Write-Host 'Ya existe la misma identidad en el destino. No es necesario importar.'
    return
}
$names=@('service.json','device_state.json')
$found=@($names | Where-Object { Test-Path -LiteralPath (Join-Path $source $_) -PathType Leaf})
Write-Host ('Archivos privados que se copiaran: '+($found -join ', '))
$confirmation=Read-Host '¿Importar estos archivos LOCALES en este Nova? (SI/NO)'
if($confirmation -cne 'SI'){Write-Host 'Cancelado; sin cambios.';return}
foreach($name in $found){
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $dest $name) -ErrorAction Stop
}
Write-Host 'Configuracion local importada. Abre Nova desde el menu Inicio.' -ForegroundColor Green
Write-Host 'No compartas service.json ni device_state.json.'
