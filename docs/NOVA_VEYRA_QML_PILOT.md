# Nova + Veyra 2.1.9 — resultado de la comprobación del atajo QML

**Estado actual: NO COMPATIBLE con el Veyra 2.1.9 NVIDIA portable oficial.**
**No utilizar la ruta QML sin recompilar como una versión lista para el usuario.**
No hay un Veyra QML oficial en la descarga que podamos modificar sin recompilar.

## Verificación directa contra el código de Veyra que subió el usuario

- `scripts/package-portable.ps1` empaqueta el objetivo **`veyra.exe`** como
  **`Veyra.exe`**, además de DLL, shaders y runtimes del motor. No incluye
  `qml/Veyra/Main.qml`, `qmldir` ni `veyra_qml_ui.exe`.
- `apps/veyra/main.cpp` inicia la interfaz Win32 `AppShell`; **no usa
  QQmlApplicationEngine ni carga módulos QML**.
- `apps/veyra-qml/main.cpp` SÍ carga `Veyra/Main.qml`, pero corresponde
  a un ejecutable **diferente**.
- En `CMakeLists.txt`, `VEYRA_BUILD_QML_UI` está **OFF por defecto**; el
  objetivo `veyra_qml_ui.exe` solo existe si se compila con esa opción.
- Por lo tanto agregar `NovaControl.qml` a una copia del ZIP oficial
  **no permite controlar el `Veyra.exe` de la versión de lanzamiento**.

Este es un problema de diseño de la ruta de prueba, no de permisos/DRM.
Las pruebas automáticas de `tests/test_veyra_qml_proxy.py` solo verifican
el proxy Python y el formato de mensajes, NO que exista la interfaz QML
en el programa distribuido.

## Acciones preventivas ya aplicadas

- `tools/Stage-NovaVeyraQml.ps1` detecta la existencia del Veyra.exe
  oficial y se detiene ANTES de copiar o modificar cualquier archivo.
- `tools/Stage-NovaVeyraLocal.ps1` ya no trata el paquete Win32
  como si trajera el runtime completo de la interfaz Qt QML.
- Windows CI comprueba que ambos programas de preparación rechazan
  explícitamente una distribución Win32 original en modo `-CheckOnly`.
- Los archivos del experimento QML permanecen aislados en la rama para
  revisión, pero **no constituyen una alternativa validada sin recompilar**.

## Qué camino SÍ es técnicamente viable

La vía segura actual es compilar el **objetivo QML real** de Veyra 2.1.9
con el adaptador `integrations/veyra/NovaStdioBridge.hpp`, usando:

1. `tools/Apply-VeyraNovaBridge.py --check` en una copia de las fuentes.
2. `tools/Build-NovaVeyraEngine.ps1 -CheckOnly` para verificar SDK/Qt/FFmpeg.
3. Compilar `veyra_qml_ui.exe` cuando estén sus dependencias legales.
4. Probarlo con `tools/Run-NovaVeyraSmoke.ps1` desde la rama experimental.
5. Medir reproducción y filtros GPU en la RTX 3060. `main` permanece intacta.

Existe además un proyecto QML/proxy experimental reutilizable una vez se
disponga de una build QML real, pero NO representa una forma de integrar el
`Veyra.exe` que trae la versión NVIDIA portable.

**Conclusión:** no se ha probado Nova reproduciendo con el Veyra oficial;
este atajo no es apto para el paquete del usuario. No afirmarlo como hecho.
