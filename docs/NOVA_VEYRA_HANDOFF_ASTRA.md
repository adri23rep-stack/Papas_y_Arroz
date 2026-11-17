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

## Pruebas y alcance

- `tests/test_veyra_bridge_installer.py`: 7 pruebas offline de instalación y rollback.
- `tests/test_veyra_stdio.py`: adaptador REAL con motor simulado; protocolos de abrir,
  pausa, ajustes, subtítulos, resume, TV, JSON fragmentado y cierre del padre.
- `tests/test_veyra_player.py`: Nova REAL contra hijo simulado; controles, cierre,
  fallo reportado, salida con error y ejecutable sin handshake; verifica entrada a MPV.
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
3. Integrar fin natural de reproducción y siguiente episodio usando el estado real
   de transporte Veyra. Actualmente el cierre de ventana termina sesión; no se
   identifica automáticamente EOF ni se marca como episodio completado.
4. Fuentes con cabeceras especiales o subtítulos externos aún usan MPV. No se
   debe eliminar esa protección hasta traducirlos correctamente.
5. Integración visual del video dentro de Nova sigue pendiente. Hay dos ventanas.

Nunca subir datos de instalación, PIN, tokens ni enlaces de reproducción privados.
Los artefactos Windows incluyen Nova y el adaptador/launcher; NO incluyen un Veyra
modificado listo para usar. No anunciarlos como integración GPU final.
