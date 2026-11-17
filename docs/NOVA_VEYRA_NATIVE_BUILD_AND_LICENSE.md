# Nova + motor Veyra original — compilación nativa (rama experimental)

**Fuente exacta:** el ZIP proporcionado `Veyra-NRVideo-main.zip` es un
archivo Git de `Likely7/Veyra-NRVideo` en el commit
`bf596e11112ab350e66d91dfb9c7ea742c254fb4`, compatible con la
versión `2.1.9`. No se está usando una implementación supuesta: el
adaptador `NovaVeyraNative.cpp` fue compilado por MSVC **solo en modo de
comprobación de API** (`/Zs`) contra esos archivos originales, en GitHub
Actions `veyra-native-source-contract.yml`.

## Estado real

- **Hecho:** licencia GPLv3 del proyecto experimental en `LICENSE`,
  licencia MIT upstream conservada en `LICENSE.MIT-UPSTREAM`.
- **Hecho:** contrato C ABI versión 3; compilación de tipos contra
  `EngineController.h`; controles de pausa/seek/velocidad/volumen,
  cambio de audio, NR/SR/FG/HDR y estado devuelto por el motor original.
- **Hecho:** Nova compila SIN la DLL y conserva MPV; al proporcionar
  `NOVA_VEYRA_NATIVE_DLL`, intentará usar el motor original para archivos
  locales y URLs públicas sencillas sin cabeceras ni firmas.
- **Pendiente:** enlazar y producir `nova_veyra_native.dll` con MSVC
  y librerías de NVIDIA/FFmpeg. Los CI tests no han hecho esto.
- **Pendiente:** desplegar shaders, runtime DLL de terceros, modelos y
  manifiestos; probar reproducción REAL con RTX 3060; resolver fallos de
  inicialización y medir FPS/uso de VRAM.
- **Pendiente:** catálogo HTTP firmado/cabeceras, pistas de subtítulos en
  el nuevo ABI, integración iOS completa y verificación de privacidad.
- **No activado por defecto:** el proceso nativo de Veyra todavía puede
  causar un fallo de Nova si su controlador D3D12 falla; no se debe
  publicar como final antes de la prueba GPU.

## Dependencias originales — no están dentro del ZIP de código fuente

1. Windows x64 y Visual Studio 2022, consola «x64 Native Tools».
2. CMake y Ninja.
3. NVIDIA DLSS SDK 310.7 con `include/nvsdk_ngx.h` y
   `lib/Windows_x86_64/x64/nvsdk_ngx_s.lib`.
4. NVIDIA Optical Flow SDK 5.0.7 completo (aceptar licencia oficial),
   incluyendo `NvOFInterface/` y `Common/NvOFBase/`.
5. Encabezados NVENC n13.0.19.0 `include/ffnvcodec/nvEncodeAPI.h`.
6. Instalación x64-windows de desarrollo de FFmpeg compatible con Veyra
   (archivos `include/`, `lib/avcodec.lib`, `lib/avformat.lib` y
   dependencias, no solo los DLL del portable). Debe conservarse el
   soporte de decodificación y parches requeridos por Veyra.
7. Para desentrelazado: FFmpeg con módulo `libavfilter`; se puede
   compilar expresamente `-DisableDeinterlace` para la primera prueba.

El paquete portátil original de Veyra reproduce con estas DLL ya
compiladas, pero **no es un SDK para enlazar el código fuente**.

Herramientas de ayuda: `tools/Fetch-NovaVeyraPublicSDKs.ps1` prepara
DLSS/NVENC de repositorios públicos; la descarga de Optical Flow exige
aceptar la licencia NVIDIA y debe hacerse por separado.

## Comprobación en la PC (sin instalar, modificar o ejecutar Nova)

Después de tener las rutas REALES en tu computadora:

```powershell
.\tools\Build-NovaVeyraNative.ps1 `
  -VeyraSource "C:\RUTA_REAL\Veyra-NRVideo-main" `
  -FfmpegRoot "C:\RUTA_REAL\vcpkg\installed\x64-windows" `
  -DlssSdkRoot "C:\RUTA_REAL\DLSS_SDK_310.7.0" `
  -OpticalFlowSdkRoot "C:\RUTA_REAL\Optical_Flow_SDK_5.0.7" `
  -NvencHeadersRoot "C:\RUTA_REAL\nv-codec-headers-13.0" `
  -CheckOnly
```

Todas las rutas `C:\RUTA_REAL` son **ejemplos**, no rutas preconfiguradas.
El script muestra los archivos faltantes y no ejecuta CMake en
`-CheckOnly`. Tras resolverlos, repetir sin `-CheckOnly` compila el
target `nova_veyra_native`. `-ConfigureOnly` permite separar
configuración y compilación.

En una futura build realmente validada, usar una DLL compilada y con
runtimes correctamente desplegados:

```powershell
$env:NOVA_VEYRA_NATIVE_DLL = "C:\RUTA_REAL\nova_veyra_native.dll"
& "C:\RUTA_REAL\Nova\XuperTVNovaQt.exe"
```

Eso **no funciona** con `veyra_qml_ui.exe`: no es la nueva DLL nativa.
La DLL tiene que exportar `nova_veyra_abi() == 3` y todas las funciones
de `NovaVeyraApi.h`.

## Licencia

El programa combinado experimental se distribuye bajo **GPLv3**.
El trabajo original Jona Ruacho conserva atribución MIT. Veyra es GPLv3
y contiene dependencias con otras licencias. Los SDK NVIDIA, modelos
y ejecutables de terceras partes no adquieren GPL por estar empaquetados
junto al programa. Revisar sus licencias y avisos antes de publicar el
programa compilado. **No subir al repositorio fuentes o archivos binarios
bajo acuerdos que lo prohíban.**

El módulo de prueba no supone ni garantiza que el equipo supere límites
de DLSS, HDR o FPS. La interfaz debe mostrar el estado real
`applied` y las capacidades que notifique Veyra, no inventarlas.
