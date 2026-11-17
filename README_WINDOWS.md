# Xuper TV Desktop — Windows Preview 0.1

Adaptación experimental para Windows de **Xuper TV by JonaRuacho 0.14.1**. El código original pertenece a su autor y conserva la licencia MIT en `LICENSE.MIT-UPSTREAM`. **Esta rama experimental combinada con Veyra adopta GNU GPLv3** (`LICENSE`). El motor Veyra original procede de `Likely7/Veyra-NRVideo` GPLv3, y sus SDK NVIDIA y bibliotecas de terceros conservan sus licencias propias. Esta rama es de pruebas, **sin release**: todavía no se distribuye la DLL GPU de Veyra ni se afirma que la RTX 3060 ya la esté usando. Consulta [auditoría nativa](docs/NOVA_VEYRA_ENGINE_SOURCE_AUDIT.md).

## Primer inicio automático: sin APK ni cuenta

El paquete incluye `service.template.json`, una plantilla de conexión
general que ya distribuía el proyecto de Nintendo Switch (JonaRuacho).
No contiene datos personales, cuentas, tokens de sesión ni identificadores
de dispositivos. No necesitas importar ni copiar una APK.

1. Extrae el ZIP completo en una carpeta **nueva**.
2. Abre `XuperTVDesktop.exe`: el programa genera `service.json` con
   una identidad **nueva y local** para esa carpeta, y guarda el estado en
   `device_state.json`.
3. El programa intenta registrarse, activar la sesión y consultar
   el catálogo automáticamente. **La respuesta la decide el servidor**:
   puede aceptar, rechazar o limitar la conexión por región, versión o permisos.
4. Si se rechaza, seguirá disponible el video y catálogo de demostración.
   El error de conexión se muestra en la pantalla.

### Si instalaste la versión anterior con asistente APK

Prueba primero en una **carpeta nueva** para no heredar un `service.json`
anterior. No copies esos archivos de estado a la carpeta nueva. Conserva
una copia de la carpeta vieja, sin publicarla en GitHub, por si luego
necesitas restaurar sus datos.

La plantilla de Switch se contrastó con el APK 4.34.4 proporcionado
para pruebas: el identificador interno del servicio, la versión 43404 y
los parámetros básicos de conexión coinciden. El paquete Android declara
otro nombre de paquete (`com.xuper.netxxus`), que es distinto del ID de
servicio interno (`com.android.msandroid`). No significa que el servidor
acepte la conexión: el registro y la disponibilidad de contenidos se
comprueban únicamente cuando responde el servicio, sin eludir
restricciones regionales, controles de acceso o licencias.

## Ejecutar en la PC conectada a la TV por HDMI

1. Abre la pestaña **Actions** y selecciona la compilación de **Xuper TV Desktop | Windows preview**.
2. Cuando la compilación salga **verde**, descarga el archivo **Xuper-TV-Desktop-Windows-preview** desde **Artifacts**.
3. Extrae **todo** el ZIP. No separes el EXE de sus DLL ni de los recursos.
4. Abre `XuperTVDesktop.exe` en Windows. Conecta tu pantalla/televisor por HDMI.
5. Pulsa **F10** para moverlo al siguiente monitor o **F11** para cambiar a pantalla completa.
6. El primer inicio ya no pide APK. Si el servidor no permite la conexión, puedes usar el catálogo de demostración y el video de prueba.

## Teclas

- Flechas: navegar
- Enter: seleccionar
- Esc: regresar
- F10: mover ventana a la siguiente pantalla
- F11 o Alt+Enter: pantalla completa
- Barra espaciadora: pausar/reanudar
- L: recargar `catalog.m3u`
- F: favorito
- X/Tab: opciones/contexto del reproductor

## Compilar por tu cuenta (MSYS2 UCRT64)

El workflow de GitHub Actions instala las dependencias con MSYS2. Para compilar en un entorno que las tenga instaladas:

```bash
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows --parallel 2
ctest --test-dir build/windows --output-on-failure
```

La rama fue construida desde la versión original fijada al commit `438c378706c1355d2f306d2d62508bdc53559cd3` y un parche de compatibilidad con Windows. El origen sigue siendo https://github.com/jonaruacho/Xuper-TV-by-JonaRuacho.

## Estado y límites

La compilación y funcionamiento en Windows deberán verificarse en GitHub Actions y en una PC. El objetivo es controlar el programa desde un iPhone posteriormente; **el control por Wi-Fi todavía no existe**.

Se incluye una plantilla de conexión pública del proyecto Switch, pero no se garantiza acceso al servicio, ni se incluyen cuentas o suscripciones. Para contenido real, utiliza servicios y fuentes con permiso de reproducción. No compartas el archivo `service.json` si contiene información privada.
