# Handoff para Astra — Nova + Veyra 2.1.9

Fecha: 17-nov-2026. Fuente: ZIP del usuario Veyra-NRVideo-main.zip.
Repositorio: adri23rep-stack/Papas_y_Arroz.
**Trabaja SOLO en feature/nova-veyra-engine. No crear ramas extra ni modificar main.**

## Meta y estado real

Objetivo: conservar catálogo, episodios, favoritos, historial e iPhone de Nova,
pero reproducir y mejorar los videos con Veyra en la RTX 3060 de 12 GB.
El usuario confirmó rendimiento excelente del **Veyra original**, NO del port.

Esta rama implementa una prueba de DOS procesos, Nova Qt y Veyra Qt QML,
con comunicación privada stdin/stdout (JSON por líneas). No es todavía un
renderizador D3D12 embebido ni una interfaz Nova dentro del proceso Veyra.
MPV sigue como alternativa predeterminada y respaldada.

## Código que ya existe

- source/nova_qt_player.cpp: selecciona Veyra experimental, inicia QProcess,
  espera nova-ready, manda open_source por stdin, recibe status y controla
  pausa/seek/volumen/velocidad, pistas audio/sub, F11 y monitor.
  Tiene fallback por handshake de 10 segundos y control de posición.
- include/nova_qt_player.hpp: estado IPC, posiciones, flags de fallback.
- source/nova_qt_home.cpp: guarda posición solo si es válida; F10 monitor.
- integrations/veyra/NovaStdioBridge.hpp: adaptador GPL-3.0-or-later para
  compilar en Veyra. No abre puertos ni OBS. Usa QmlPlayerBridge.
- tools/Apply-VeyraNovaBridge.py: --check, instalar parche idempotente,
  respaldo del main.cpp de Veyra, --restore, escritura atómica.
- tests/test_veyra_bridge_installer.py: tests offline del instalador.
- .github/workflows/bootstrap-windows.yml: compilación de Nova + unittest,
  habilitada para esta rama.
- docs/NOVA_VEYRA_PILOT.md: instrucciones completas de prueba Windows.

## Métodos reales de Veyra confirmados en el ZIP 2.1.9

Revisados en include/veyra/ui/QmlPlayerBridge.h y src/ui/bridge:
openPath, openNetworkAddress, togglePlayPause, stopPlayback, seekBy,
seekTo, setVolume, setPlaybackRate, audioTracks, selectedAudioTrack,
setSelectedAudioTrack, subtitleTracks, subtitlePrimary, setSubtitlePrimary,
running, paused, failed, hasSource, position, duration, fgEnabled,
fgMultiplier, fgMaxMultiplier, setFgEnabled, setFgMultiplier, srEnabled,
setSrEnabled, nrEnabled, setNrEnabled, videoHdr, setVideoHdr,
sourceFps, submitFps, submitFpsKnown.

audioTracks entrega mapas index/label donde index es streamIndex.
subtitleTracks entrega index/label/usable; -1 desactiva subtítulos.

Código de origen preciso:
apps/veyra-qml/main.cpp, include/veyra/ui/QmlPlayerBridge.h,
src/ui/bridge/Playback.cpp, Chain.cpp, Subtitles.cpp y Enhancement.cpp.
Veyra es C++20/Qt/D3D12/MSVC; Nova usa MinGW/Qt/libmpv.
No pasar objetos, punteros ni DLL Qt entre dos toolchains; el IPC los aísla.

## Cómo usar el prototipo

En PowerShell, estando en Nova:
1. python .\tools\Apply-VeyraNovaBridge.py --check "C:\ruta\Veyra-NRVideo-main"
2. python .\tools\Apply-VeyraNovaBridge.py "C:\ruta\Veyra-NRVideo-main"
3. Compilar veyra_qml_ui.exe desde esa COPIA con su preset CMake x64-release.
4. Compilar XuperTVNovaQt.exe desde nuestra rama.
5. Definir NOVA_VEYRA_PILOT=1 y NOVA_VEYRA_EXE=RUTA_ABSOLUTA_DEL_EXE.
6. Para HTTP(s) público habilitar NOVA_VEYRA_ALLOW_NETWORK=1.

NO reemplazar el Veyra que ya funciona; NO borrar sus ajustes.
No entregar como portable completo hasta empaquetar sus dependencias.

## Seguridad y límites

- JAMÁS subir service.json, device_state.json, PIN, tokens, sesiones,
  autenticaciones, URLs firmadas, logs privados ni archivos del usuario.
- El URL se envía por stdin tras handshake, NO por argumentos CLI.
- Si SourceResult contiene headers o subtítulos externos, Nova usa MPV.
- URLs con query requieren NOVA_VEYRA_ALLOW_SIGNED_NETWORK=1 y pueden
  aparecer en logs propios de Veyra. Revisar antes de usar proveedores.
- Si Veyra no arranca, no responde o la fuente no es elegible: fallback MPV.
- No borrar progreso desconocido ni hacer seek en TV en vivo.
- Los cinco perfiles de calidad MPV NO equivalen a efectos de Veyra:
  no hacer un mapeo silencioso. En piloto se usa preset guardado de Veyra.
- Veyra deriva de software GPL-3.0; revisar avisos/licencias antes de publicar.
- El soporte VFG para RTX30 es experimental; no asegurar FPS sin medir.

## Fallo concreto detectado y corregido

El primer Actions compiló source/nova_qt_player.cpp, pero falló
source/nova_qt_home.cpp: un comentario contenía el texto literal backslash-n
y absorbía la condición de guardado de posición; quedó una llave huérfana.
**Ya está corregido.** Verificar Actions de la última cabeza de rama antes
de cambiar más cosas.

## Ruta de trabajo priorizada (ahorra tokens)

P0. Leer únicamente archivos arriba. Ver último Actions del branch:
arreglar errores de compilación basados en logs, no reescribir a ciegas.
P1. Correr los tests offline del instalador; aplicar --check sobre el ZIP
original y compilar Veyra Qt QML en Windows/MSVC. Resolver errores reales.
P2. Probar handshake JSON, película local, HLS público, subtítulos, audio,
pausa, seek, volumen, fullscreen, monitor, cierre, crash y fallback.
P3. Controlar explícitamente FG/SR/NR/HDR/multiplicador desde PC/iPhone,
sin romper perfiles existentes ni asumir capacidades que no se detectan.
P4. Fuentes con headers, subtítulos externos, reconexiones TV y empaquetado.
P5. Evaluar ventana de video embebida / migración UI (NO está implementada).

## Matriz de aceptación

- Piloto OFF -> MPV Nova normal.
- EXE ausente -> MPV.
- EXE normal sin parche -> timeout y MPV.
- Veyra parcheado + archivo local -> imagen y sonido, control iPhone.
- Audio/sub iPhone -> lista real de Veyra, sin opciones inventadas.
- TV público -> buffer y reconexión medidos, sin trabones.
- Continuar viendo -> segundos válidos sin reiniciar a cero.
- F10/F11 -> Veyra cambia monitor/fullscreen, Nova permanece abierta.
- Cambiar episodio/canal -> no queda doble reproducción ni sesión anterior.
- Veyra falla -> Nova no se cae.
- GPU RTX 3060 -> medir FPS de origen/salida y rendimiento real.

## Entrega al usuario

Informar: commits/archivos exactos, resultado de CI, build probado o no,
pruebas reales RTX 3060 pendientes y enlaces a artifacts disponibles.
No afirmar éxito de GPU por pasar un smoke headless. No merge a main
sin petición expresa del usuario.
