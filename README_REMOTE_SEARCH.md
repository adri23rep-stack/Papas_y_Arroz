# Xuper TV — Búsqueda integrada con Apple Controller

Esta rama agrega búsqueda de TV, películas y series dentro de **Windows**
y de la aplicación nativa **Apple Controller para iPhone**.

## Windows
- En el menú Cinematic V2, pulsa **Buscar** en la barra superior, o usa `X`
  en el teclado. Los resultados siguen mostrando imágenes y datos del catálogo.
- Continúa funcionando el buscador original del servicio, sus fuentes y el
  reproductor libmpv. Los filtros y portadas no incluyen datos inventados.
- El receptor local de Apple Controller sigue en TCP `47831`, con emparejamiento
  mediante PIN y token. No hay acceso desde Internet.

## Desde iPhone
1. Instala **el XuperTVDesktop.exe generado desde esta rama**; el viejo no
   contiene el buscador remoto.
2. Instala la IPA de `Apple-Controller/feature/catalog-search`, con firma válida.
3. Conéctate a la PC usando el PIN de la pantalla.
4. Toca el icono de lupa. Escribe un nombre; los resultados aparecen
   **dentro del iPhone**, con filtros Todos / TV / Películas / Series.
5. Toca una tarjeta para abrir el canal o la ficha de la película/serie
   en Windows. El reproductor puede cambiar de contenido desde el control.

## Detalles técnicos
- `POST /catalog/search` inicia la consulta y devuelve un `generation`.
- `POST /catalog/results` recibe ese número y devuelve el progreso y
  hasta 60 entradas con título, tipo, sinopsis e imagen pública HTTPS.
- `POST /catalog/open` envía el índice de la entrada elegida y su
  generación al menú/reproductor.
- Las tres rutas exigen el mismo token de emparejamiento local. Ninguna
  devuelve URL de reproducción, cabeceras de licencia ni datos de sesión.
- La búsqueda remota copia la sesión internamente para no modificar los
  canales de la reproducción; limita a seis categorías de TV por consulta
  inicial, por lo que algunas búsquedas de canales pueden ser parciales.
- Las portadas se cargan desde las direcciones **HTTPS** proporcionadas
  por el catálogo. Si un servidor solo ofrece HTTP o no responde, se
  muestra un icono de TV o cine en el iPhone, nunca se inventa una portada.
- El protocolo utiliza HTTP solo dentro de una red privada; no se debe
  exponer el puerto a Internet ni usar redes Wi-Fi públicas.

## Pruebas
GitHub Actions construye EXE y IPA de forma independiente. Las pruebas
automatizadas comprueban controles, autorización y la compilación; la
búsqueda real y las imágenes necesitan una prueba posterior en PC e iPhone.

**No fusionar a main ni publicar release sin validación.**
