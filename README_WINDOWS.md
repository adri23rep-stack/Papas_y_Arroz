# Xuper TV Desktop — Windows Preview 0.1

Adaptación experimental para Windows de **Xuper TV by JonaRuacho 0.14.1**. El código original pertenece a su autor y conserva la licencia MIT. Esta rama es de pruebas, **sin release**.

## Ejecutar en la PC conectada a la TV por HDMI

1. Abre la pestaña **Actions** y selecciona la compilación de **Xuper TV Desktop | Windows preview**.
2. Cuando la compilación salga **verde**, descarga el archivo **Xuper-TV-Desktop-Windows-preview** desde **Artifacts**.
3. Extrae **todo** el ZIP. No separes el EXE de sus DLL ni de los recursos.
4. Abre `XuperTVDesktop.exe` en Windows. Conecta tu pantalla/televisor por HDMI.
5. Pulsa **F10** para moverlo al siguiente monitor o **F11** para cambiar a pantalla completa.
6. Sin servicio configurado, puedes navegar por el catálogo de demostración y abrir el video de prueba.

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

No se incluye acceso a servicios de TV, credenciales ni suscripciones. Para contenido real, utiliza servicios y fuentes con permiso de reproducción. No compartas el archivo `service.json` si contiene información privada.
