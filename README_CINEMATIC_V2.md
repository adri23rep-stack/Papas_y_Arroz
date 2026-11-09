# Xuper TV — Cinematic V2 (Windows)

Rediseño real de la interfaz TV usando el motor existente **SDL2 + libmpv**.
No se cambiaron la autenticación del servicio, el decodificador, la tienda de
contenidos ni el receptor del iPhone. Esta rama parte de
`feature/apple-controller-server` y está separada de `main`.

## Nuevo Inicio

- Portada cinematográfica grande del elemento seleccionado.
- Información auténtica del catálogo: título, tipo y sinopsis **si el
  servicio la proporciona**. No se inventan ratings, duración ni EPG.
- **Ver ahora**, **Pantalla completa** y **Detalles** son botones funcionales.
  El botón de detalles abre la sinopsis ampliada. Pantalla completa también
  funciona con **F11** y **Alt+Enter**.
- **Último canal visto**: se conserva al terminar de ver TV y aparece como
  tarjeta para reanudar. La pestaña **TV en vivo** vuelve a abrir ese canal
  automáticamente cuando existe un historial válido.
- Fila de canales en vivo con miniaturas obtenidas del servidor.
- Filas separadas de **Películas destacadas** y **Series populares**, con
  sus portadas originales tomadas del catálogo.
- Cada portada se puede seleccionar con mouse o trackpad del iPhone.
- En las secciones de películas se usan tarjetas con imágenes tipo póster,
  no los cuadros pequeños del primer diseño.

## Cómo obtiene las portadas

Tras conectar al servicio, el programa consulta de forma limitada una
categoría de TV y las primeras categorías compatibles de películas y series
para seleccionar portadas reales. Puede tardar algo más en el primer inicio
que la versión anterior y depende de la disponibilidad del servidor. Las
portadas se almacenan en la caché de imágenes habitual.

## Reproductor

- Conserva subtítulos, selección de fuentes, volumen, historial y la barra
  deslizable de canales con vista previa en vivo.
- Durante la reproducción hay botón visible de **Pantalla completa** en el
  overlay superior, compatible con mouse y trackpad remoto.
- El control del iPhone sigue funcionando sin modificar la IPA.

## Pruebas

El workflow de Windows compila el programa y ejecuta las pruebas de catálogo,
reproducción, canales, control LAN y la nueva geometría de la interfaz, además
de la recuperación del último canal visto. Las pruebas de GitHub no sustituyen
la prueba real con una PC/HDMI, especialmente para cargas de imágenes y videos.

**Sin release ni merge a main hasta validación.**
