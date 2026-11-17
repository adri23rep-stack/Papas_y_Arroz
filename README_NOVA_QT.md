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

## Correcciones de conexión y reproducción local

Si Nova muestra «Catálogo desconectado», verás **el error de conexión
directamente bajo el título**, junto al botón «Reintentar». La app nunca
presenta el contenido de prueba como si fuera un canal real. Al fallar la
conexión, aparece una fila independiente de **Prueba local del reproductor
(no es TV en vivo)**, que utiliza el archivo real `test.mp4` que ahora se
incluye en el paquete Windows.

Las primeras compilaciones copiaban `service.template.json` con la ruta
de certificado de Nintendo Switch (`sdmc:/...`). La versión corregida
empaqueta la ruta relativa `cacert.pem`. También interpreta en memoria la
ruta antigua de `service.json` generados previamente, **sin modificar
la identidad de instalación ni tus datos privados**. Ya no hace falta
borrar tus archivos de configuración para probar el arreglo.

La conexión real depende de que el servidor acepte la activación, permisos
y acceso regional. El reproductor no fabrica canales ni evade restricciones
del proveedor. Si la pantalla presenta un error de red o permiso, ese texto
es el diagnóstico que debemos revisar antes de asegurar que el catálogo
está conectado.

## Perfil de registro Android de prueba (solo Nova Windows)

El proveedor puede rechazar el registro en `/api/portalCore/v3/snToken` con
`portal200001`. Para **diagnosticar** si la ficha de dispositivo influye,
Nova admite un perfil experimental que simula metadatos publicos de
Android 7.1.1 / OnePlus A5000. **No copia identificadores ni sesiones del APK
Focuz, no altera permisos ni garantiza acceso al servicio.**

El perfil esta **desactivado por defecto**. En PowerShell, desde la carpeta
del **nuevo** `XuperTVNovaQt.exe` compilado tras este cambio:

```powershell
$env:NOVA_DEVICE_PROFILE = "android-test"
.\XuperTVNovaQt.exe
```

Si el servicio rechaza la solicitud, el diagnostico mostrara
`Registro del dispositivo [Android de prueba]: ...` para confirmar que el
modo se activo. Cierra Nova y vuelve al perfil original con:

```powershell
Remove-Item Env:\NOVA_DEVICE_PROFILE -ErrorAction SilentlyContinue
.\XuperTVNovaQt.exe
```

El modo solo cambia en memoria los metadatos de plataforma enviados durante
la conexion. Conserva `service.json`, `device_state.json`, favoritos,
biblioteca y ajustes del reproductor. No edites ni publiques esos archivos
privados. No necesitas virtualizacion, root ni el APK para probarlo.

**Limites:** usa valores sinteticos generales, no el perfil real del emulador.
Si ambos perfiles devuelven `portal200001`, habremos descartado esta prueba
de metadatos, no identificado toda la diferencia del cliente Android.
La reproduccion con RIFE/RTX AI no se modifica.

### Segundo perfil de prueba: Android TV (Amlogic)

El APK Focuz contiene `FocuzIdentityHelper.ensureProfile`, que prepara
metadatos sinteticos de la familia Amlogic/S905X3/Android 11 y los mantiene
consistentes durante la sesion. La ruta `Lbc/f.g0` utiliza este perfil
cuando construye `SnTokenBean` (registro del dispositivo). Esta observacion
no demuestra que copiar el perfil conceda autorizacion del servicio.

Para probar solo esos metadatos publicos en Nova Windows, sin copiar
identidades, sesiones, tokens ni el mecanismo de rotacion de pruebas de Focuz:

```powershell
$env:NOVA_DEVICE_PROFILE = "android-tv-test"
.\XuperTVNovaQt.exe
```

El error, si lo hay, mostrara `[Android TV de prueba]`. Para restaurar
el modo normal, cierra Nova y ejecuta:

```powershell
Remove-Item Env:\NOVA_DEVICE_PROFILE -ErrorAction SilentlyContinue
.\XuperTVNovaQt.exe
```

No se editan `service.json` ni `device_state.json` del modo normal. Si el servidor acepta la prueba, Nova guarda el estado experimental por separado en `device_state.android-tv-test.json` (o `device_state.android-test.json` para el modo anterior); no copia credenciales ni modifica los datos del reproductor.
Un rechazo `portal200001` en este perfil **no** prueba que el problema
sea el hardware: tambien pueden influir version, autorizacion, requisitos del
proveedor y la secuencia completa de registro. Evita reenviar identificadores
y no uses ni imites la rotacion de sesiones de prueba.

## Televisión pública española (IPTV-org)

Nova Qt incluye ahora, por defecto, la lista pública actualizable:

```text
https://iptv-org.github.io/iptv/languages/spa.m3u
```

La descarga se realiza al iniciar o pulsar «Reintentar» y se guarda una copia
en `iptv_es_cache.m3u`. Si falla la descarga, se usa la última copia válida.
La lista **no requiere** `service.json`, un emulador o una sesión en el portal
de XuperTV. Cuando el portal devuelve `portal200001`, se conservan los canales
públicos como catálogo principal de **TV en vivo**, con sus nombres y logos
si la lista los proporciona. **Ver todo** enseña todos los canales; el inicio
muestra hasta 80 para mantener la interfaz ligera. **Buscar** funciona
localmente por nombre cuando no hay sesión del portal.

MPV abre cada fuente M3U/HLS directamente como canal en vivo. Es posible
que algunos canales estén caídos, bloqueados por región o requieran permisos;
IPTV-org no garantiza la disponibilidad de cada transmisión. No se copian ni
renuevan los enlaces temporales de Focuz. No se alteran el control del iPhone,
los perfiles de calidad, RIFE ni la configuración original del proveedor.
