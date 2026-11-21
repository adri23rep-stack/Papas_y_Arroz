# Nova para macOS Tahoe (Intel) — vista previa experimental

Esta rama adapta la interfaz **Nova Qt**, no el cliente SDL antiguo. Conserva
la navegacion por TV, peliculas, series, busqueda, arte, historial, lista
publica IPTV-org y el cliente del servicio cuando este configurado legalmente.

## Compilar en Mac Intel

La accion de GitHub llamada **Nova macOS Intel (experimental)** genera
`Nova-macOS-Tahoe-Intel.zip` (si la compilacion y las pruebas pasan).
Tambien puedes construir desde la fuente:

```bash
brew install cmake ninja pkgconf qt sdl2 sdl2_ttf mpv curl libwebp mbedtls qrencode libpng jpeg-turbo
cmake -S . -B build/nova-macos -G Ninja -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix sdl2);$(brew --prefix mbedtls)"
cmake --build build/nova-macos --target XuperTVNovaQt
```

El resultado es `build/nova-macos/XuperTVNovaQt.app`. La accion empaqueta
las dependencias de Qt con macdeployqt. **No se ha comprobado aun que todas
las bibliotecas ajenas a Qt esten integradas**; podria ser necesario instalar
las dependencias de Homebrew indicadas arriba en la Mac donde se ejecute.

## Diferencias de esta primera vista previa

- El motor de video usa libmpv en una ventana nativa independiente de macOS;
  se evita la incrustacion `--wid` de Windows.
- El control remoto de iPhone con QR/PIN, el cambio de monitor remoto y
  las funciones Windows del servidor LAN aun no se han portado. No se
  muestra un QR o PIN falso.
- RIFE Vulkan distribuido como DLL de Windows, Direct3D, RTX Video Super
  Resolution y Optical Flow NVIDIA no funcionan en este port.
- Los modos de imagen compatibles con libmpv pueden estar disponibles;
  su soporte real depende del hardware grafico del Hackintosh.
- No se incluyen sesiones de usuario, secretos, cuentas, ni catalogos
  privados en el binario de GitHub.

**Estado:** preparacion de compilacion; el archivo .app NO se considera
probado en Hackintosh solo por compilar en GitHub Actions. Mantener esta
rama separada de `main` hasta confirmar arranque y reproduccion reales.
