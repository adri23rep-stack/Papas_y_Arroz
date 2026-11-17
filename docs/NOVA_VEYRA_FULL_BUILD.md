# Veyra 2.1.9: insumos reales para compilar el motor completo en Windows

Comprobado contra el ZIP adjunto del usuario: Veyra-NRVideo-main.zip,
CMakeLists.txt, CMakePresets.json, README.md y docs/DEVELOPMENT.md.

**Estado:** falta la compilación COMPLETA y prueba RTX 3060. Las pruebas
`veyra-bridge-msvc.yml` y las de motor simulado validan las interfaces,
pero NO enlazan el ejecutable real de Veyra con los componentes GPU.

## Confirmaciones en el propio código fuente de Veyra

- Windows 11 x64, DirectX 12, compilador MSVC 2022 con C++20.
- CMake >= 3.24, Ninja y Qt 6.8+ (en los documentos: Qt 6.8.3 MSVC2022 x64).
- `VEYRA_BUILD_QML_UI` viene **OFF** por defecto. Para obtener
  `veyra_qml_ui.exe` debe configurarse `-DVEYRA_BUILD_QML_UI=ON`.
- `VEYRA_FFMPEG_ROOT` apunta al prefijo instalado de FFmpeg x64-windows;
  se identifica por `include/libavformat/avformat.h`.
- `VEYRA_NVOF_SDK_ROOT` (Optical Flow SDK) debe contener
  `NvOFInterface/nvOpticalFlowCommon.h` para habilitar sus componentes.
- `VEYRA_DLSS_SDK_ROOT` contiene `include/nvsdk_ngx.h`; requerido si se activa
  `VEYRA_ENABLE_EXPERIMENTAL_DLSSNR`.
- Si se construyen los sinks NVENC, `VEYRA_NVENC_HEADERS_ROOT` necesita
  `include/ffnvcodec/nvEncodeAPI.h` (versión indicada n13.0.19.0).
- Para usar desentrelazado con FFmpeg, el prefijo necesita
  `include/libavfilter/avfilter.h`, o hay que desactivar
  `VEYRA_ENABLE_DEINTERLACE` expresamente.

La carpeta `third_party_local` con estos componentes no aparece en el ZIP
adjunto. Algunos componentes son de terceros, con licencias y redistribución
propias. No subir SDK cerrados, DLL privadas ni modelos ajenos a GitHub.

## Secuencia de compilación que NO modifica tu Veyra instalado

1. Extraer el ZIP en un directorio **aparte** y conservar el programa Veyra
   que ya funciona. Abrir PowerShell con herramientas de Visual Studio x64.
2. Desde la raíz del repositorio Nova, rama
   `feature/nova-veyra-engine`, verificar las anclas del parche:

   `python .\tools\Apply-VeyraNovaBridge.py --check "C:\VeyraFuente"`

3. Aplicar parche en esa copia de las fuentes:

   `python .\tools\Apply-VeyraNovaBridge.py "C:\VeyraFuente"`

4. Preparar SDKs/FFmpeg/Qt legales por sus procedimientos originales.
   Para una prueba inicial del proyecto QML, configurar el preset de Veyra
   con opciones adaptadas a tu instalación, por ejemplo:

```powershell
cd C:\VeyraFuente
cmake --preset x64-release -DVEYRA_BUILD_QML_UI=ON `
  -DCMAKE_PREFIX_PATH="C:\Ruta\Qt\6.8.3\msvc2022_64" `
  -DVEYRA_FFMPEG_ROOT="C:\Ruta\FFmpeg-x64-windows" `
  -DVEYRA_NVOF_SDK_ROOT="C:\Ruta\Optical_Flow_SDK"
cmake --build --preset x64-release --target veyra_qml_ui --parallel 2
```

El comando es una **plantilla, no una build ejecutada**. Puede requerir rutas
adicionales dependiendo de las funciones GPU habilitadas. Revisar
`CMakeLists.txt` y los mensajes de configuración; no deshabilitar
silenciosamente características para lograr un EXE que aparenta tenerlas.

5. Probar el EXE modificado con el script del repositorio Nova:
   `tools/Run-NovaVeyraSmoke.ps1`.
6. Medir FPS reales y estabilidad en la RTX 3060 antes de distribuir.
   Conservar avisos y licencias de Veyra y sus dependencias.

**Primera prioridad cuando haya entorno Windows con dependencias:**
comprobar configuración CMake, registrar exactamente qué archivo/SDK falta,
compilar `veyra_qml_ui` y ejecutar smoke local con un video sin credenciales.
No confundir esta prueba con la de GitHub que solo comprueba la interfaz C++.

## Herramienta añadida en esta continuación

La rama ahora tiene `tools/Build-NovaVeyraEngine.ps1`. A diferencia de ejecutar
CMake a ciegas, verifica Qt, FFmpeg, NVIDIA DLSS SDK, Optical Flow SDK, NVENC
y las herramientas de compilación **antes de tocar las fuentes**.

Desde una terminal x64 de Visual Studio 2022, con rutas REALES a componentes
oficiales instalados, ejecutar:

```powershell
$common = @{
  VeyraSource = 'C:\Proyectos\Veyra-NRVideo-main'
  QtRoot = 'C:\Qt\6.8.3\msvc2022_64'
  FfmpegRoot = 'C:\veyra-deps\installed\x64-windows'
  DlssSdkRoot = 'C:\SDK\DLSS_SDK_310.7.0'
  OpticalFlowSdkRoot = 'C:\SDK\Optical_Flow_SDK_5.0.7'
  NvencHeadersRoot = 'C:\SDK\nv-codec-headers-13.0'
}
.\tools\Build-NovaVeyraEngine.ps1 @common -CheckOnly
# Solo cuando todo indique OK:
.\tools\Build-NovaVeyraEngine.ps1 @common
```

El chequeo muestra los archivos ausentes sin descargarlos ni modificar el
Veyra instalado. La segunda orden instala el parche en la COPIA de las
fuentes y compila el destino `veyra_qml_ui` en `out/nova-veyra-bridge`.
Se puede usar `-ConfigureOnly` o `-DisableDeinterlace` explícitamente.
El EXE compilado aún necesita sus recursos QML y dependencias para funcionar
fuera del árbol de compilación: **no llamarlo portable sin esas pruebas**.

GitHub Actions prueba la sintaxis del script y su respuesta ante SDK ausentes,
pero **no prueba una compilación GPU completa**.
