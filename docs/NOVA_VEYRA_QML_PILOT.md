# Nova + Veyra Qt/QML 2.1.9 — piloto del ejecutable real del usuario

**Estado: PREPARACIÓN LOCAL CONFIRMADA, reproducción no confirmada.**

Corrección basada en los resultados de PowerShell proporcionados por el usuario
(noviembre de 2026): dentro de su portable NVIDIA 2.1.9 sí existen
`veyra_qml_ui.exe`, `qml/Veyra/Main.qml` y `qml/Veyra/qmldir`.
Su ejecución de `tools/Stage-NovaVeyraQml.ps1` terminó correctamente y
creó `C:\NovaVeyraQmlPrueba`.

La comprobación anterior basada en `scripts/package-portable.ps1` concluía
que el ZIP oficial solo contenía `Veyra.exe`, pero **esa conclusión no se
aplica al paquete real que el usuario extrajo**. No suponer por defecto que
un portable carece de QML. El Win32 AppShell y el QML son dos ejecutables
diferentes: solo el segundo carga el módulo que podemos modificar.

## Cómo funciona este experimento

1. El preparador hace una COPIA de Veyra en otra carpeta y modifica solamente
   el `Main.qml` y `qmldir` de la copia.
2. `NovaControl.qml` envía y recibe comandos por HTTP en
   `127.0.0.1` usando una clave aleatoria de sesión.
3. `tools/nova_veyra_qml_proxy.py` convierte los mensajes locales
   entre Nova (stdin/stdout JSON) y el componente QML invisible.
4. El ejecutable QML original en esa COPIA sigue utilizando las DLL,
   shaders y archivos GPU de la versión portátil que pasó el chequeo.
5. El reproductor MPV sigue funcionando si la fuente está protegida con
   cabeceras o si falla el puente.

**Limitaciones:** la mera apertura de Nova no significa que Veyra funcionó.
El catálogo puede tener enlaces con parámetros de sesión o autorización y
Nova los manda deliberadamente a MPV. Todavía faltan las pruebas físicas
de video/sonido/FG en la RTX 3060.

## Prueba sin depender del catálogo

La rama ahora muestra el botón `Probar Veyra · test.mp4` en
**Calidad de video**, tanto en el piloto QML como en el Win32.
Colocar un video propio llamado `test.mp4` al lado de
`XuperTVNovaQt.exe`. Iniciar con el lanzador
`C:\NovaVeyraQmlPrueba\Start-NovaVeyraQml.ps1`.
Pulsar el botón y comprobar si aparece el proceso `veyra_qml_ui.exe`.

El nuevo botón necesita un ZIP de Nova recompilado DESPUÉS del commit
`10dd0bda868dec9f6a430b96c346fec2ecfeb4d1`: no aparece en el
ZIP anterior. Se recomienda usar una carpeta nueva para el ZIP y
actualizar la ruta del lanzador al nuevo Nova o crear una NUEVA copia
de piloto QML con `Stage-NovaVeyraQml.ps1`, nueva carpeta de salida.

El nombre de comando de PowerShell `PS C:\...>` y las líneas `>>`
son indicadores de la consola, **no deben pegarse como comandos**.

No modificar `main` ni el Veyra original. El motor Qt GPU todavía
no tiene prueba de reproducción integrada en el hardware del usuario.
