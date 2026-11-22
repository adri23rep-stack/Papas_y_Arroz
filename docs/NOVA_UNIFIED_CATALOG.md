# Catálogo unificado de Nova Qt

## Qué quedó integrado

Nova Qt combina en su pantalla existente el catálogo Xuper que tenga conectado
con los siguientes **feeds de reproducción pública**:

- IPTV-org: español y México.
- IPTV Nexus: español, México, películas libres y series libres.
- LegalFreeIPTVplaylist: TV y VOD.

Todos se abren con el mismo reproductor MPV. La interfaz mantiene tres secciones:
**TV, Series y Películas**. La búsqueda encuentra títulos de estas listas
incluso cuando Xuper está conectado, sin instalar una versión distinta de Nova.

### Agregar más fuentes sin actualizar Nova

Copia \`nova_sources.example.txt\` como \`nova_sources.txt\` junto al .exe.
Una línea por catálogo: \`identificador|Nombre|https://servidor/lista.m3u|tipo\`.

Tipos: \`live\`, \`movie\`, \`series\`, \`vod\` y \`mixed\`.
Las fuentes declaradas como \`vod\` o \`mixed\` requieren \`tvg-type="movie"\`
o \`tvg-type="series"\` para clasificar videos; si no se conoce su tipo se
tratan como canales. \`movie\` y \`series\` asignan un tipo al feed entero.

### Qué ocurre al fallar un servicio

- Cada feed se descarga por separado con límite de tamaño y tiempo de espera.
- Si falla, Nova conserva una copia local de la última descarga válida.
- Una respuesta HTML/403 no reemplaza la copia.
- Las entradas se deduplican por título dentro de cada sección.
- Si Xuper no responde, el catálogo público sigue visible.

**Limitación actual:** dos enlaces distintos con el mismo título no se prueban
automáticamente uno tras otro durante la reproducción; se conserva uno en el
catálogo. Para failover real por stream falta un selector de fuentes que reciba
la señal de error de MPV. Tampoco se ha validado cada canal desde México.

## Fuentes de metadatos y repositorios que no son nuevos feeds

- **Cinemeta/Stremio:** proporciona portadas, fichas y un protocolo de addons,
  pero un título en Cinemeta no implica que haya un stream reproducible.
  La integración de ese protocolo como proveedor de reproducción requiere
  resolver streams autorizados por título y todavía no está implementada.
- **TMDB:** ofrece metadatos; requiere una clave propia y no entrega video.
- **NEOWATCH:** utiliza principalmente IPTV-org e Internet Archive, de modo
  que añadirlo como fuente sin extraer su catálogo duplicaría resultados.
  Sus ideas de control de salud y failover son referencia para etapas siguientes.
- **MetaFlux:** proyecto de metadatos, no otro proveedor de vídeo listo para
  usar en Nova.

No se utilizan tokens, datos de cuenta ni APIs privadas de terceros.

## Validación

\`nova_catalog_sources_test\` comprueba que el parser distinga canales,
películas, episodios, enlaces inválidos y entradas que requieren cabeceras.
Las pruebas son sintéticas; para validar red y reproducción hay que ejecutar
la aplicación en Windows con su entorno Qt/MPV.

Los proyectos tienen licencias independientes. La accesibilidad real de sus
streams puede depender del país o de cambios en servidores externos.
