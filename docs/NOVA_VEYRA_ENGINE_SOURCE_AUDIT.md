# Auditoría del código Veyra 2.1.9: motor original en Nova

Fecha: 2026-11-17. Fuente inspeccionada directamente:
`Veyra-NRVideo-main.zip` entregado por el usuario, árbol
`Likely7/Veyra-NRVideo` v2.1.9. Comprobación de diseño; **no equivale
a prueba de reproducción GPU**.

## Componentes originales (confirmados por código)

| Función | Archivo original Veyra | Qué hace realmente |
|---|---|---|
| Entrada / decodificación | `src/source/MediaFileSource.cpp`, `src/media/FFmpegDemuxer.cpp`, `src/media/FFmpegVideoDecoder.cpp` | FFmpeg, D3D12VA cuando disponible, respaldo a software y reinicio temporal al fallar el hardware |
| Control del motor | `src/engine/EngineController.cpp`, `include/veyra/engine/EngineController.h` | Ciclo de vida en un hilo, audio, seek, revisiones de ajustes, sincronización y medición de los resultados |
| Gráfica de mejora real | `src/pipeline/EnhanceGraph.cpp` | GPU D3D12 y secuencia de filtros. No es un filtro de pantalla aplicado después del video |
| GPU y Present | `src/gfx/D3D12DeviceContext.cpp`, `src/engine/VideoPresenter.cpp`, `src/gfx/PresentSink.cpp` | D3D12, fences, swapchain DXGI en un HWND existente; no necesita ventana UI de Veyra |
| DLSS/SR RTX | `src/ngx/DlssSrBackend.cpp`, `src/ngx/VideoSrBackend.cpp` | Acceso a NVIDIA NGX y sus recursos reales |
| Generación de FPS | `src/ngx/DlssFgBackend.cpp`, `src/pipeline/VfgBackend.cpp`, `src/ngx/NvOfSession.cpp` | Interpolación por proveedores reales, flujo óptico y límites de la GPU |
| Ruido, NR temporal | `src/pipeline/NrInstance.cpp`, `src/pipeline/NrTemporalPass.cpp`, `src/ngx/DlssNrRuntimeAdapter.cpp` | NR + reutilización/admisión y versiones efectivas de los recursos |
| HDR | `src/ngx/TrueHdrBackend.cpp`, `include/veyra/engine/VideoHdrSettings.h` | HDR de video con parámetros y estado real de la salida |
| Sonido/subtítulos | `src/sink/WasapiAudioSink.cpp`, `src/engine/Subtitles.cpp` | Audio nativo y presentación de subtítulos del motor |
| Interfaz separada | `src/ui/QmlPlayerBridge.cpp`, `include/veyra/ui/QmlPlayerBridge.h` | Solo controla el motor; NO contiene los algoritmos GPU |

Hallazgo decisivo: `EngineController::open(HWND, const std::wstring&,
PlayerOptions)` recibe el **HWND de un reproductor ajeno** y `VideoPresenter
::open(D3D12DeviceContext&, HWND, EnhanceGraph&, ...)` crea su swapchain
sobre él. Por eso **Nova puede ofrecer su propia interfaz y reproducir
directamente con el motor original sin la ventana ni QML de Veyra**.

La implementación Veyra usa C++20/MSVC y SDK DirectX12/NVIDIA. La Nova
actual usa C++17/MinGW. Son ABI C++ incompatibles; enlazar las librerías
C++ de ambos compiladores directamente sería incorrecto.

## Cambio concreto entregado a la rama experimental

- `integrations/veyra/native/NovaVeyraApi.h`: contrato C entre MSVC y
  MinGW, sin compartir `std::string`, clases C++ ni memoria entre CRT.
- `integrations/veyra/native/NovaVeyraNative.cpp`: adaptador que
  **llama el `veyra::engine::EngineController` original**, no un sustituto.
  Abre archivo o URL permitida, pausa, avanza, velocidad, volumen, SR/NR/FG,
  RTX Video HDR, y devuelve el snapshot REAL de frames/FPS.
- `integrations/veyra/native/CMakeLists.txt`: compila el `veyra_engine`
  original como parte del mismo build nativo MSVC; genera
  `nova_veyra_native.dll`, sin compilar UI Win32 ni QML de Veyra.
- `include/nova_veyra_native_loader.hpp`: carga la DLL y valida
  cada símbolo y su versión de ABI. No se carga de forma predeterminada.
- `source/nova_qt_player.cpp`: opción `NOVA_VEYRA_NATIVE_DLL`:
  Nova crea su propia ventana Qt, pasa su HWND al motor original,
  consulta estado real y usa los controles propios de Nova/iPhone
  para efectos, reproducción y monitor. Si hay fallo al iniciar,
  vuelve al MPV anterior para no perder reproducción.
- `tests/nova_veyra_native_abi_test.cpp` y `CMakeLists.txt`: prueba
  de tamaños, enums y firma C del ABI en Windows.

**Esto es integración del motor original en proceso, NO un puente Python,
QML escondido ni otra aplicación Veyra.exe.**

## Qué sigue sin estar probado ni construido

1. No existe todavía un `nova_veyra_native.dll` compilado y probado con
   esta rama. El paquete de Nova Windows no la incluye.
2. `tools/Build-NovaVeyraNative.ps1` permite revisar dependencias y
   compilar el motor de Veyra **cuando** están presentes los SDK oficiales,
   la distribución de desarrollo FFmpeg compatible y MSVC 2022.
3. Falta desplegar DLL y shaders correctos, ejecutar D3D12 real en RTX 3060
   y confirmar imagen/audio, ausencia de fugas y FPS generados medidos.
4. Los comandos nativos iniciales están implementados, pero audio-pistas
   avanzadas/subtítulos embebidos, soporte de headers/signed CDN y ajuste
   avanzado de modelos NR todavía no están expuestos plenamente por el ABI.
   Para esos casos, Nova conserva MPV.
5. El plugin corre dentro del proceso de Nova. Un fallo grave del controlador
   podría afectar a Nova; no se habilitará de forma predeterminada hasta
   que exista prueba GPU y recuperación. Tests del ABI no equivalen a esto.

## Licencias importantes

Veyra tiene licencia **GNU GPL v3** (`LICENSE` del ZIP). Nova procede del
código MIT de Jona Ruacho. Reutilizar y distribuir el motor original
junto a Nova obliga a analizar cómo se aplican los términos GPLv3 al programa
combinado, ofrecer código fuente correspondiente y conservar avisos de
terceros. No declarar la integración cerrada o de solo licencia MIT.
No redistribuir SDKs/runtimes NVIDIA o modelos propietarios sin autorización.
El módulo nativo de este experimento está marcado GPL-3.0-only.

## Uso posterior de la nueva DLL

Solo tras compilarla realmente, sin instalar Veyra ni lanzar otra ventana:

```powershell
$env:NOVA_VEYRA_NATIVE_DLL = 'C:\NovaVeyraNative\nova_veyra_native.dll'
& 'C:\NovaPilot\XuperTVNovaQt.exe'
```

Estas rutas son **ejemplos**, no un paquete disponible. El motor no se
activa si falta la DLL, y el MPV previo sigue intacto. Primero probar
video local; después transmisión legítima sin headers y finalmente catálogo.
