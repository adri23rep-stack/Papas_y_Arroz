# XuperTV Nova Qt — etapa 1 (solo Inicio)

Esta es una **vista previa nativa de Windows con Qt 6 Quick / QML**, no una
reescritura del reproductor. El diseño anterior en SDL se mantiene para
comprobar, comparar y reproducir contenido mientras terminamos las vistas.

## Cómo abrir Nova Qt

1. Descarga **XuperTV-NOVA-Qt-Preview.zip** y
   **XuperTV-NOVA-Playback-Addon.zip** de `build/nova-ui-preview`.
2. Extrae **ambos ZIP en la misma carpeta**, de modo que se combinen los
   archivos de `Xuper-TV-NovaQt-Preview` (no dentro de dos carpetas separadas).
3. Abre `XuperTVNovaQt.exe`. El complemento incluye las DLL necesarias para
   reproducir video y enlazar el control del iPhone.
4. Arriba a la derecha encontrarás **QR / PIN**. Abre Apple Controller,
   escanea el QR o escribe IP y PIN, con ambos equipos en la misma red.
5. Elige un canal o episodio para reproducir. El motor libmpv abre su propia
   ventana de video mientras conservamos la interfaz Qt del catálogo.

**XuperTV-NOVA-Windows-Preview.zip** es la versión clásica, con su propio
reproductor. Se extrae **aparte** como respaldo, no encima de Nova Qt.

El token permanente de emparejamiento sigue protegido en el perfil local de
Windows. Nunca se incluye en las descargas.


## Etapa de integración

- Conserva la interfaz Qt Quick de Inicio, sus animaciones y su degradado.
- Los títulos reales, categorías y episodios pueden explorarse y buscarse
  desde Qt. Los datos y URLs de streaming se resuelven mediante las funciones
  existentes de servicio, sin exponer tokens en QML.
- Reproducción real mediante libmpv en una ventana independiente de video.
  El catálogo Qt permanece disponible durante el video.
- El servidor del iPhone ya existente se ejecuta dentro de Nova Qt: mismo
  protocolo, mismo PIN/QR, control por LAN, búsqueda y acciones de reproducción
  compatibles con la navegación disponible en Qt.
- El botón pequeño **QR / PIN** enseña el código QR y datos para conectar.
- No migramos todavía controles avanzados del reproductor clásico (por ejemplo
  su menú integrado de pistas y filtros). El viejo programa sigue disponible
  como respaldo mientras terminamos la transición.
- No se fusiona con `main` ni se publica release.


La distribución de Qt requiere revisar y respetar su licencia y las
obligaciones de redistribución antes de una publicación estable.
