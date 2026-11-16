# XuperTV Nova Qt — diseño nuevo + compilación local

La rama `feature/nova-ui-from-scratch` contiene la nueva interfaz Qt Quick.
**main y el reproductor SDL anterior siguen intactos.** El servidor del
control del iPhone conserva el protocolo y el emparejamiento existente.

## Cómo compilar Nova en tu PC Windows

1. En GitHub, abre la rama `feature/nova-ui-from-scratch` y usa
   **Code > Download ZIP** (o clona esa misma rama). Extrae el proyecto entero.
2. Haz doble clic en **`COMPILAR_NOVA_WINDOWS.cmd`**, que está en la raíz.
3. Si falta MSYS2, el asistente llama a **WinGet** para instalarlo desde el
   catálogo oficial; después instala los paquetes de MSYS2 UCRT64 de Qt 6,
   CMake, Ninja, C++, libmpv, SDL, FFmpeg, curl y qrencode.
4. Compila `XuperTVNovaQt.exe`, reúne los componentes QML y DLL, prueba el
   arranque en modo sin pantalla y abre el programa si todas las pruebas pasan.

**Ejecutable compilado:**
`dist/Xuper-TV-NovaQt-Local/XuperTVNovaQt.exe`

El primer uso requiere conexión y descarga muchas dependencias. Si MSYS2
actualiza su propio entorno y pide reiniciar, cierra la consola y ejecuta el
mismo CMD otra vez. Si algo falla, la ventana permanece abierta con el mensaje
para que puedas fotografiarlo.

Alternativa en PowerShell, desde la raíz del proyecto:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Compilar-Nova.ps1 -Run
```

El script solamente instala paquetes públicos, compila el repositorio local
y prepara el ejecutable. **Nunca borra service.json, biblioteca, estado de
reproducción, emparejamiento ni las claves del iPhone**. Solo la opción
`-Clean` borra archivos intermedios de compilación, no datos personales.

## Pantalla y controles

- Banner del catálogo REAL: título, sinopsis proporcionada por el servicio,
  botones **Reproducir** y **Más info**, fundido oscuro a la izquierda y abajo.
- La imagen aprovecha su resolución original; en servidores de imágenes con
  rutas TMDb admite la variante `original`. Si la fuente publica solo una
  miniatura pequeña, Qt no puede inventar detalle real.
- Primero **Continuar viendo** a partir de los episodios y minutos realmente
  guardados en `playback_state.json` y `library_state.json`.
- Canales **16:9** con el logo centrado sobre negro uniforme; series y películas
  en carteles **2:3**, títulos debajo de la imagen.
- Un único acento violeta, foco de **1.08×** en **185 ms**, sin marcos brillantes.
- Secciones separadas y menú superior **Inicio, TV, Series, Películas, Buscar**.
- Exploración de categorías y episodios en Qt; video en una ventana separada
  mediante libmpv, mientras la interfaz sigue disponible.
- **QR / PIN** arriba a la derecha para vincular el control ya existente del
  iPhone, usando la misma red y el mismo servicio protegido de Windows.

### Programación de TV

La barra de progreso y el nombre «Ahora en vivo» están preparados para mostrarse
solo si el servicio proporciona una guía de programación con horario auténtico.
El modelo actual no incluye esos campos: **no se inventa ninguna emisión ni
porcentajes de avance**.

## Desarrollo

Fuentes Qt Quick: `ui/novaqt/`, adaptador Qt C++: `source/nova_qt_home.cpp`,
motor de video Qt: `source/nova_qt_player.cpp`.

Los scripts no crean ramas, no fusionan `main` y no publican releases.
Antes de distribuir públicamente una versión estable deben respetarse las
licencias de Qt y las demás bibliotecas incluidas.
