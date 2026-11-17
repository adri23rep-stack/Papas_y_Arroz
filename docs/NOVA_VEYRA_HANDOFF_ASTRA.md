# Nova + Veyra — estado de continuación, 17-nov-2026

Repositorio: adri23rep-stack/Papas_y_Arroz. Rama única: `feature/nova-veyra-engine`.
No modificar main sin petición del usuario. No crear ramas adicionales para arreglos.

## Implementado

- Nova conserva catálogo, historial y control iPhone. Veyra sigue siendo un proceso aparte.
- Puente privado JSON stdin/stdout; URLs fuera de argumentos y stderr del hijo descartado.
- PC: panel Veyra con FG/SR/NR/HDR, multiplicadores ofrecidos por el motor y FPS reportados.
- iPhone: `/settings/get` expone esos controles solamente con Veyra conectado;
  `/settings/set` valida sesión, valor y multiplicadores reales. Se mantiene el protocolo
  genérico existente; falta probar el cliente iOS físico. No se modificó el repo iOS.
- No se convierten perfiles MPV a efectos Veyra. Los ajustes GPU los conserva Veyra;
  Nova no sobrescribe el preset al arrancar ni presenta una solicitud como confirmada.
- Recuperación a MPV si falla el proceso, el estado reporta fallo, no inicia el video en
  30 segundos, falta handshake en 10 segundos o se pierde el estado durante 10 segundos.
  Conserva la última posición válida; TV no recibe seek.
- Cierre ordenado con `quit` y límite de 1.5 segundos antes de matar solo el hijo.
  El adaptador sale si desaparecen las tuberías del padre.
- Rutas locales absolutas, entorno Qt aislado, lectura por líneas hasta 64 KiB,
  sin perder lotes de mensajes que superen el límite anterior de 8 KiB.
- Instalador idempotente con rollback ante escritura fallida y rechazo de restauración
  si main.cpp tiene cambios posteriores ajenos al instalador.

- EOF exacto del transporte Veyra marca la película como completada y cierra el hijo.
  Se inhibe el autoavance de su playlist para que no abra un archivo ajeno a Nova.

## Pruebas y alcance

- `tests/test_veyra_bridge_installer.py`: 9 pruebas offline de instalación y rollback.
- `tests/test_veyra_stdio.py`: adaptador REAL con motor simulado; protocolos de abrir,
  pausa, ajustes, subtítulos, resume, TV, JSON fragmentado y cierre del padre.
- `tests/test_veyra_player.py`: Nova REAL contra hijo simulado; controles, cierre,
  fallo reportado, salida con error, EOF y ejecutable sin handshake; verifica entrada a MPV.
- `remote_server_test`: opciones MPV conservadas, controles Veyra condicionales,
  rechazo de multiplicador no ofrecido y estado no actualizado prematuramente.
- `bootstrap-windows.yml`: compila Nova, corre pruebas anteriores y smoke portable.
- `veyra-bridge-msvc.yml`: compila sintácticamente el adaptador contra cabeceras REALES
  de Veyra 2.1.9 con MSVC/Qt 6.8.3. NO enlaza ni genera el reproductor Veyra completo.

Fuente upstream fijada a `Likely7/Veyra-NRVideo` commit
`bf596e11112ab350e66d91dfb9c7ea742c254fb4`. Se compararon byte a byte CMakeLists,
main QML y QmlPlayerBridge.h con el ZIP del usuario: idénticos.

Consultar Actions del último commit para resultados; no inferir éxito por el código.
El primer CI de esta continuación compiló C++ pero encontró un test antiguo que
suponía que todos los ajustes siempre se muestran. Se actualizó para las opciones
condicionales. La comprobación MSVC necesitó `/Zc:__cplusplus` para Qt.

## Pendiente real (prioridad)

1. Compilar y empaquetar el motor Veyra COMPLETO modificado. El ZIP del usuario no
   incluye `third_party_local`: faltan SDK/runtime NVIDIA, Optical Flow, NVENC,
   FFmpeg MSVC y demás dependencias indicadas en su CMake/scripts. El preset CMake
   solo NO basta: necesita `VEYRA_BUILD_QML_UI=ON` y las rutas SDK/FFmpeg/Qt.
   La comprobación de cabeceras y el motor simulado NO sustituyen esa build.
2. Verificar reproducción y mejoras en Windows/RTX 3060: sincronía, TV, HDR,
   fullscreen, monitores y rendimiento. No se hicieron mediciones GPU aquí.
3. **Implementado en esta continuación:** siguiente episodio automático en Nova Qt
   solo al EOF real si `playback_history_.auto_next` está activo y la página
   del catálogo contiene el episodio siguiente contiguo de la misma serie.
   Se captura el candidato al iniciar el episodio para que cambiar de sección
   mientras reproduce no mezcle programas. No se inventa la secuencia,
   no avanza películas, canales, búsquedas sin orden ni páginas ambiguas.
   Prueba C++: `nova_auto_next_test` (CTest, assert activo incluso en Release).
4. Fuentes con cabeceras especiales o subtítulos externos aún usan MPV. No se
   debe eliminar esa protección hasta traducirlos correctamente.
5. Integración visual del video dentro de Nova sigue pendiente. Hay dos ventanas.

Nunca subir datos de instalación, PIN, tokens ni enlaces de reproducción privados.
Los artefactos Windows incluyen Nova y el adaptador/launcher; NO incluyen un Veyra
modificado listo para usar. No anunciarlos como integración GPU final.

## Continuación posterior al último commit de Astra

- Lógica de auto-siguiente en `include/nova_auto_next.hpp` y `source/nova_qt_home.cpp`.
- Pruebas `tests/nova_auto_next_test.cpp` registradas en `CMakeLists.txt`.
- Guía de compilación real del motor: `docs/NOVA_VEYRA_FULL_BUILD.md`.
- Releer Actions actual para confirmar empaquetado final antes de anunciar un ZIP.
- Aún falta el binario Veyra completo parcheado y pruebas RTX 3060; no hacer merge.

## Continuación después de la prueba de Nova Windows (noviembre 17)

**Bloqueo central confirmado:** Veyra completo con GPU NO puede salir del ZIP
de código solamente; faltan dependencias de compilación, no solo de ejecución.
Por eso el piloto Windows todavía usa MPV mientras el puente Veyra no está
compilado y habilitado. No anunciar que el EXE portable incluye Veyra.

NUEVO: `tools/Build-NovaVeyraEngine.ps1` comprueba las rutas y el toolchain,
parchea una COPIA del código y construye solamente `veyra_qml_ui` cuando
están presentes Qt MSVC, FFmpeg x64-windows, DLSS 310.7.0, Optical Flow SDK
5.0.7 y headers NVENC 13.0.19.0. El modo `-CheckOnly` no toca archivos.
NUEVO: `tools/Fetch-NovaVeyraPublicSDKs.ps1` permite clonar con versión
fija desde NVIDIA/DLSS (v310.7.0) y FFmpeg/nv-codec-headers (n13.0.19.0);
la descarga y aceptación del Optical Flow SDK sigue siendo independiente.
CI Windows valida la sintaxis de ambos scripts y el rechazo de SDKs faltantes.
Ver `docs/NOVA_VEYRA_FULL_BUILD.md` para rutas, comando y límites.

Al reanudar: primero buscar en la máquina Windows real los SDKs, el prefijo
FFmpeg y Qt MSVC; después ejecutar -CheckOnly. No perder tiempo reintentando
compilar un Veyra reducido sin esas dependencias. Si no están disponibles,
la siguiente petición útil al usuario es la ruta/copia autorizada de esas
dependencias, NO el APK ni las credenciales del catálogo.
