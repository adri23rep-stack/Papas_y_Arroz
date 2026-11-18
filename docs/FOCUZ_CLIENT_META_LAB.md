# Laboratorio de compatibilidad Focuz — cabeceras de cliente

Rama aislada: `fix/focuz-client-meta-lab`. No se ha fusionado con `main`, no modifica Veyra y no incluye APKs, identificadores personales, tokens, servidores o cadenas privadas.

## Evidencia estática del APK aportado

- Paquete: `com.lite.fczx`, manifiesto 4.34.4 / 43404.
- En `Lzb/a.intercept`, `apkVer` contiene el valor literal `49920`.
- `Lzb/a.maybeInjectClientMeta` genera un UUID temporal, concatena un sufijo fijo del APK, calcula un MD5 hexadecimal y envía `X-Request-Trace` y `X-Client-Meta`.
- Los filtros estáticos incluyen las rutas de registro `snToken`, activación `active` y catálogo `getColumnContents`.
- No está demostrado que el servidor necesite esas cabeceras ni que resuelvan `portal200001`. Ese código es una respuesta del servidor; su causa exacta sigue pendiente.

## Cambios en este laboratorio

1. `tools/configure_from_apk.py` encuentra el sufijo fijo solo si puede reconocer la secuencia conocida `const-string` + `StringBuilder.append(String)`. Lo guarda **solo** en el `service.json` generado localmente bajo la propiedad `client_meta_suffix`; no imprime su valor.
2. `source/service.cpp` añade las dos cabeceras por solicitud a rutas `/api/portalCore/`, solo si `client_meta_suffix` está presente y tiene un formato permitido. Sin esa propiedad conserva el comportamiento anterior.
3. `tests/test_service.py` comprueba en localhost/TLS que el UUID es v4, el MD5 coincide con datos **sintéticos**, y las solicitudes sin configuración adicional no envían las cabeceras.

## Prueba en Windows

Ejecuta estos comandos desde **una copia independiente** de la rama, con tu APK local:

```powershell
py -3 -m pip install -r .\tools\requirements.txt
py -3 .\tools\configure_from_apk.py C:\ruta\Focuz.apk --platform windows --output C:\ruta\privada\service.json
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Compilar-Nova.ps1
```

Copia `service.json` privado a la carpeta del **ejecutable compilado de laboratorio**, inicia Nova allí y observa el código de respuesta. La extracción no hace conexiones; Nova sí enviará solicitudes de registro cuando se abra. **No compartas `service.json`**. No copies estados, cookies o identidades de BlueStacks ni de tu instalación principal.

## Límites y comprobaciones pendientes

- Este entorno no dispone del toolchain MSYS2/Qt de Windows y no se ha ejecutado la compilación completa aquí. Las pruebas TLS del repositorio deben pasarse al compilar.
- El servidor puede rechazar el registro por otras razones: permisos, identidad local, perfiles, reglas regionales o cambios de protocolo. No intentes eludir una denegación de acceso.
- Los campos privados nunca deben subirse a GitHub, pegarse en un chat o imprimirse en registros de diagnóstico.

## Blindaje de estado y fallback (2026-11-18)

**Objetivo:** evitar que una interrupción de red o un rechazo transitorio deje la aplicación en peor estado que antes. Esto no concede permisos ni garantiza la continuidad del catálogo privado.

- `service_bootstrap` ya no sobrescribe `device_state.json` al recibir solo `snToken`. La identidad/contador se conservan si `active` es rechazado o no entrega una sesión válida.
- Tras la autorización completa, el nuevo estado se escribe en un archivo temporal, se valida el resultado de escritura y se reemplaza de forma atómica. Esto protege contra archivos cortados por cierre inesperado.
- La caché **pública** `iptv_es_cache.m3u` se guarda con `QSaveFile`: una descarga o escritura fallida no destruye la copia previa.
- `tests/test_service.py` agrega regresión: rechazo, versión denegada y activación incompleta no alteran el último estado autorizado.
- `tools/Backup-NovaLocalState.ps1` crea un respaldo privado con hashes SHA-256 en `%LOCALAPPDATA%\Nova-Focuz\Backups`. No sube archivos a ningún servidor ni imprime identidad o tokens.

**Respaldo antes de actualizar el ejecutable:** en PowerShell, desde la rama ya descargada:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Backup-NovaLocalState.ps1
```

Si la instalación funcional está en otra carpeta:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Backup-NovaLocalState.ps1 -SourceDirectory "C:\ruta\a\Nova"
```

**No hagas pruebas de conexión agresivas contra un servidor que esté rechazando la autorización.** Si un proveedor deniega `snToken` o `active`, Nova debe indicarlo y conservar los datos locales sin intentar eludir la decisión. La TV pública puede seguir disponible por separado, pero nunca debe presentarse como el catálogo autorizado. El respaldo es para recuperación local, no para renovar o transferir licencias.
