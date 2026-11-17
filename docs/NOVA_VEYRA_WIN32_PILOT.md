# Prueba real de Nova + Veyra 2.1.9 oficial (Win32, sin compilar)

**Estado: prueba experimental de video LOCAL. Aún no ejecutada con RTX 3060.**

La versión oficial 2.1.9 es `Veyra.exe` (Win32). El supuesto atajo de editar
`Main.qml` no funciona con esa distribución. Esta variante sí habla con
el ejecutable Win32 original, sin cambiar sus bytes y sin SDK NVIDIA.

## Qué hace exactamente

El código de Veyra del ZIP que proporcionó el usuario confirma:

- `apps/veyra/ui/AppShell.cpp`: la orden `Recent=113` lee el último archivo
  de la sección `Player`, clave `最近打开`, de `runtime_local/veyra.ini`.
- Las órdenes `Play=102`, `Stop=103`, `Fullscreen=120` usan `WM_COMMAND`.
- `src/base/RuntimePaths.cpp`: los datos locales se guardan al lado del EXE
  dentro de `runtime_local`. Por eso un portable COPIADO se puede aislar.
- `apps/veyra/main.cpp`: el producto publicado es Win32, no QML.

`tools/nova_veyra_win32_proxy.py` abre **únicamente archivos locales que
existen**; prepara la entrada `Recent` en la COPIA y manda el comando de
Windows al proceso que lanzó. Puede pausar y cambiar pantalla completa.
Detecta cuando la ventana cambia de título para saber si la orden de abrir
se recibió; **no asegura que el video se decodificó ni inventa FPS**.

## Cómo probar en tu Windows

Necesitas:

1. Una versión de Nova del branch `feature/nova-veyra-engine` (el ZIP
   `Nova-Veyra-Windows-Pilot` más reciente en GitHub Actions).
2. El ZIP oficial NVIDIA **Veyra 2.1.9** ya extraído en una carpeta, que
   contenga `Veyra.exe`, sus DLLs, `runtime/`, `runtime_local/`, `shaders/`.
3. Python 3.10+ en Windows (el mismo de tus proyectos sirve).

Dentro de la carpeta del repositorio Nova, ejecuta en PowerShell
con tus rutas reales:

```powershell
.\tools\Stage-NovaVeyraWin32.ps1 -CheckOnly `
  -OfficialPortable 'C:\Veyra-2.1.9-NVIDIA-win64-portable' `
  -NovaExe 'C:\NovaPilot\XuperTVNovaQt.exe' `
  -Output 'C:\NovaVeyraWin32Prueba'
```

Cuando diga que reconoce el EXE oficial, ejecuta igual **sin**
`-CheckOnly` (la carpeta de salida debe ser nueva):

```powershell
.\tools\Stage-NovaVeyraWin32.ps1 `
  -OfficialPortable 'C:\Veyra-2.1.9-NVIDIA-win64-portable' `
  -NovaExe 'C:\NovaPilot\XuperTVNovaQt.exe' `
  -Output 'C:\NovaVeyraWin32Prueba'

& 'C:\NovaVeyraWin32Prueba\Start-NovaVeyraWin32.ps1'
```

Abre **primero un archivo de prueba local** desde el catálogo de Nova
(entrada que apunte a un archivo real del PC). Si se abre la ventana
Win32 de Veyra, prueba pausa y pantalla completa.

La preparación verifica SHA-256 de `Veyra.exe` contra el valor publicado
para el producto oficial 2.1.9:
`c8a53dc778331c696f5a488400885ef842adb849e1b22a9fbea07520a9ecb8f0`.

No modifica tu Veyra original, tus configuraciones, PIN ni tu catálogo.
No necesita administrador. Si el enlace falla, Nova debería regresar a MPV.

## Restricciones importantes

- **NO** reproduce automáticamente canales de TV/streaming con este puente.
- **NO** controla aún audio, subtítulos, FPS, SR, NR, HDR, volumen ni seek
  en el Win32. Eso se maneja manualmente dentro de Veyra, mientras que Nova
  evita presentar controles GPU falsos.
- Si una serie tiene un minuto guardado (Continuar viendo), usa MPV. El
  puente Win32 no conoce la posición y no la sobrescribe.
- Veyra es otra ventana, no un renderizador embebido en Nova.
- El test de Python/Windows CI comprueba el protocolo y rutas, no que el
  motor GPU funcione en una PC real. El primer smoke requiere la RTX 3060.
- No mezclar runtimes entre versiones ni subir los ~700 MB oficiales a GitHub
  sin revisar sus licencias y términos.
- Si no se consigue la ventana/control o Veyra se cierra, Nova conserva MPV.

El objetivo de este smoke es probar **que Nova puede abrir un archivo con el
motor Win32 Veyra ya compilado**. Si funciona, la siguiente fase será el
puente completo con estado y controles GPU (preferentemente el QML C++ real).
