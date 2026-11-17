# Nova + Veyra oficial por QML — piloto sin recompilar NVIDIA

Estado: **experimental**. Código nuevo, todavía sin prueba física en RTX 3060.
Esta alternativa es totalmente separada del motor Veyra reconstruido con SDK.
La ruta normal MPV de Nova permanece predeterminada y main no se modifica.

## Hallazgo confirmado en el Veyra 2.1.9 real

Su EXE Qt/QML carga `qml/Veyra/Main.qml` desde la carpeta de la app y
expone `veyra` (QmlPlayerBridge) como objeto QML. No hace falta modificar
su EXE para **intentar** controlar el reproductor desde una copia de su QML.
Esto se verificó leyendo el Main.qml, qmldir y main.cpp del ZIP del usuario.
Falta validarlo corriendo el Veyra oficial real en Windows; QML podría imponer
restricciones adicionales o requerir adaptaciones de red/CORS.

## Qué añadimos

- `integrations/veyra/qml/NovaControl.qml` — controlador invisible que
  utiliza XMLHttpRequest de QML para conectar por HTTP local con Nova y
  traduce abrir video, pausa, volumen, avance, audio, subtítulos y FG/SR/NR/HDR.
  Envia posición, pistas y FPS que Veyra sí exponga.
- `tools/nova_veyra_qml_proxy.py` — proxy Python 3.10+ SIN librerías extras,
  que traduce el protocolo JSON ya probado de Nova (stdin/stdout) a
  `127.0.0.1` con token aleatorio por ejecución. No abre un puerto de red
  pública ni almacena la URL del video en argumentos del EXE.
  En Windows usa un Job Object para cerrar el Veyra hijo al terminar el proxy.
- `tools/Stage-NovaVeyraQml.ps1` — crea una COPIA local nueva de la distribución
  Veyra oficial, registra el QML en qmldir, y prepara un lanzador para Nova.
  Solo cambia el Main.qml de esa nueva copia; verifica SHA256 del EXE original
  frente al 2.1.9 publicado. No sobrescribe tu Veyra instalado.
- `tests/test_veyra_qml_proxy.py` — test local de permisos, token aleatorio,
  comandos, estado, y de que no salen contraseñas/URLs a la salida estándar.

**Importante:** este modo usa un directorio de configuración separado
`NovaPilotData`; los filtros de Veyra pueden arrancar con ajustes por defecto.
Se podrán cambiar desde el Veyra de prueba; no se tocan tus preferencias reales.

## Primer test Windows, sin SDK de NVIDIA

Necesitas el ZIP **NVIDIA portable oficial Veyra 2.1.9 extraído**,
una versión de Nova compilada desde la rama actual y Python 3.10+.

En PowerShell dentro de la carpeta de Nova:

```powershell
.\tools\Stage-NovaVeyraQml.ps1 `
  -OfficialPortable 'C:\Veyra-2.1.9-NVIDIA-win64-portable' `
  -NovaExe 'C:\NovaPilot\XuperTVNovaQt.exe' `
  -Output 'C:\NovaVeyraQmlTest' -CheckOnly

# Solo después del chequeo:
.\tools\Stage-NovaVeyraQml.ps1 `
  -OfficialPortable 'C:\Veyra-2.1.9-NVIDIA-win64-portable' `
  -NovaExe 'C:\NovaPilot\XuperTVNovaQt.exe' `
  -Output 'C:\NovaVeyraQmlTest'

& 'C:\NovaVeyraQmlTest\Start-NovaVeyraQml.ps1'
```

Las rutas son ejemplos; se sustituyen por carpetas existentes. El script
no inicia el programa ni descarga Veyra sin tu intervención.

**Probar primero** un medio local o TV HLS público sin autenticación.
Las fuentes con headers especiales/subtítulos externos continúan en MPV.
URLs con parámetros de firma siguen fuera de este piloto salvo que el usuario
habilite expresamente `NOVA_VEYRA_ALLOW_SIGNED_NETWORK=1`; Veyra puede
registrar el enlace internamente. No pegar URLs privadas en reportes.

## Limitaciones conocidas

1. El controlador QML y el proxy aún no están probados ejecutando Veyra
   oficial en Windows. Pasar tests Python no significa que Veyra abra QML.
2. El botón de cambio de monitor y la detección exacta de EOF aún no están
   implementados en el adaptador QML (a diferencia del puente C++ modificado).
3. Si falta Python, la distribución oficial o el EXE exacto, Nova vuelve a MPV.
4. Video Veyra sigue siendo una ventana separada, no un hijo de Qt Nova.
5. La reproducción desde catálogos que requieren cabeceras de autorización
   no se fuerza ni elude: queda MPV hasta que exista traducción fiel.
6. Probar FPS reales, sincronía A/V, pantalla completa y estabilidad en la RTX
   3060. No fusionar a main sin esa verificación.

### Código mantenido como alternativa

El puente C++ `integrations/veyra/NovaStdioBridge.hpp` sigue siendo
el camino nativo completo una vez que se compilen SDKs y dependencias.
El piloto QML no lo elimina ni modifica el Veyra oficial.

Un informe útil al probar: consola sin URL/token, si el reproductor Veyra
aparece, reproduce imagen/audio, responde a pausa/volumen y si al cerrarlo
Nova regresa sin errores. No enviar capturas que incluyan credenciales.
