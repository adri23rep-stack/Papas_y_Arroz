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
- **Hecho:** contrato C ABI versión 4; compilación de tipos contra
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
La DLL tiene que exportar `nova_veyra_abi() == 4` y todas las funciones
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

## Empaquetado local del motor REAL (después de compilar la DLL)

El instalador `tools/Stage-NovaVeyraNative.ps1` prepara una **carpeta
nueva**, nunca reescribe la instalación existente. Usa los DXIL compilados
por el código fuente de Veyra y verifica los archivos DLL importados con
`dumpbin /DEPENDENTS`. Copia únicamente archivos instalables de Nova,
no `service.json`, identidades, ajustes, cache, sesiones ni tokens.
Los archivos propietarios de NVIDIA se copian **solo** de la instalación
portable que ya posee el usuario, para su propia prueba; **no** se suben
al repositorio ni a Github Actions.

```powershell
.\tools\Stage-NovaVeyraNative.ps1 -NovaDirectory "C:\RUTA_REAL\Nova" -NativeBuildDirectory "C:\RUTA_REAL\out\nova-native-gpu" -VeyraSource "C:\RUTA_REAL\Veyra-NRVideo-main" -PortableVeyraDirectory "C:\RUTA_REAL\Veyra-2.1.9-NVIDIA-win64-portable" -FfmpegRoot "C:\RUTA_REAL\vcpkg\installed\x64-windows" -Output "C:\NovaVeyraNativoPrueba" -CheckOnly
```

Sin `-CheckOnly`, produce `Start-NovaVeyraNative.ps1`. Ese lanzador
marca `NOVA_VEYRA_NATIVE_STRICT=1`: ante un error de DLL o de D3D12,
muestra el error **sin cambiar silenciosamente a MPV**. Con la DLL
cargada, el botón **Probar Veyra · test.mp4** del panel de calidad
ejecuta el archivo local incluido con Nova. Se pueden comprobar:
reproducción, conteo real de cuadros, generación de FPS, audio y cierre.

El original Veyra resuelve shaders y binarios NGX respecto a la carpeta
del **EXE huésped**. Por eso la preparación pone los DXIL en
`Nova/shaders` y los activos NVIDIA autorizados en
`runtime_local/nvidia` o `runtime/experimental`, no junto al
source de Veyra. Si la DLL depende de versiones incompatibles de FFmpeg
o Qt, el preparador detiene el proceso por colisión de hash.

La comprobación Windows `veyra-native-source-contract.yml` ya compila
`NovaVeyraNative.obj` con MSVC a partir de las cabeceras exactas del
motor real. **No** enlaza `veyra_engine` ni puede sustituir las
pruebas físicas de GPU.

## NVIDIA Optical Flow SDK recibido; compilación local pendiente

El usuario entregó el **`Optical_Flow_SDK_5.0.7.zip` original**.
Se comprobaron directamente `NvOFInterface/nvOpticalFlowCommon.h`,
`NvOFInterface/nvOpticalFlowD3D12.h` y
`Common/NvOFBase/NvOF.h` / `NvOFD3D12.h`. El antiguo preflight
buscaba erróneamente `NvOFBase.h`, que NO existe en el SDK real;
se corrigió.

`tools/Prepare-NovaVeyraOpticalFlow.ps1` valida y extrae el ZIP
únicamente en el Windows local del usuario. La prueba en GitHub Actions
usa un ZIP **sintético**, nunca el contenido licenciado del usuario.
La obtención del SDK **ya está resuelta**; NO debe volver a pedirse.

Para construir y, si están presentes los portables previos, crear
automáticamente una Alpha 1 de prueba en una carpeta nueva:

```powershell
.\tools\Complete-NovaVeyraNative.ps1 -OpticalFlowZip "$env:USERPROFILE\Downloads\Optical_Flow_SDK_5.0.7.zip" -DisableDeinterlace
```

El código preparado descarga versiones públicas fijadas de Veyra,
DLSS/NVENC y FFmpeg mediante vcpkg, inicializa MSVC y trata de
compilar la DLL. Usa los nombres reales del SDK 5.0.7.
La primera Alpha prueba video local progresivo y puede omitir
desentrelazado para facilitar la compilación inicial.

**Lo pendiente no es conseguir otro ZIP de Optical Flow**. Falta
ejecutar el compilador completo en Windows con MSVC, verificar
dependencias, generar `nova_veyra_native.dll`, y efectuar un smoke
con GPU Nvidia sobre archivo MP4 y medición de cuadros.
Un `Nova` compilado por MinGW y los tests del extractor no certifican
que el motor Veyra GPU esté funcionando.

## Primer smoke GPU Alpha 1 (solo en tu PC)

El módulo nativo compila también `nova_veyra_native_smoke.exe`: NO es el
antiguo fixture de mentira. Crea un HWND real y usa el
`EngineController` original con un archivo local `test.mp4`.
La prueba básica requiere que Veyra reporte motor activo y al menos
10 cuadros de video reales; la opcional exige además que el backend
NVIDIA-VFG reporte cuadros generados (no basta con pulsar 2x).

Desde la carpeta de la Alpha empaquetada:

```powershell
.\Test-NovaVeyraNative.ps1
.\Test-NovaVeyraNative.ps1 -Vfg2x
```

La RTX 3060 no debe tratarse como una GPU con **DLSS Frame Generation**
compatible. Por eso se selecciona NVIDIA-VFG (ID 4) para esta Alpha,
no DLSS FG. La compatibilidad de VFG depende de los modelos/runtime
instalados; se confirma únicamente con `fg_active` y
`generated_frames > 0`.

### Shader paths

`VeyraShaders.cmake` escribe a
`CMAKE_CURRENT_BINARY_DIR/shaders`, dentro de
`out/nova-native-gpu/veyra-original/shaders` cuando el código se
compila como subproyecto. `EngineController` usa
`CMAKE_BINARY_DIR/shaders`; la integración copia después de compilar
los DXIL originales a `out/nova-native-gpu/shaders` y comprueba
`ScaleBlit.dxil`, `PresentBlit_vs.dxil` y `PresentBlit_ps.dxil`.

Una prueba de GitHub Actions solo valida el adaptador MSVC y el
empaquetado con componentes sintéticos; no sustituye el render D3D12
real en la tarjeta del usuario. No incluir SDK/modelos NVIDIA en
artifacts públicos.
