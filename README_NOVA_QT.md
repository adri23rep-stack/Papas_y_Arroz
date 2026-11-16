# XuperTV Nova Qt — etapa 1 (solo Inicio)

Esta es una **vista previa nativa de Windows con Qt 6 Quick / QML**, no una
reescritura del reproductor. El diseño anterior en SDL se mantiene para
comprobar, comparar y reproducir contenido mientras terminamos las vistas.

## Cómo abrirla

1. Descarga el ZIP de la rama `build/nova-ui-preview` y extráelo completo.
2. Ejecuta **XuperTVNovaQt.exe** para ver el nuevo Inicio con Qt Quick.
3. Ejecuta **XuperTVDesktop.exe** para seguir utilizando el reproductor y el
   control del iPhone. Ahí, F8 sigue alternando entre las dos vistas SDL.

Ambos ejecutables utilizan los mismos archivos de instalación existentes y
deben permanecer en la misma carpeta. No copies DLL individuales de versiones
antiguas.

## Qué tiene exactamente la etapa 1

- Primera pantalla sin barra lateral, con tarjetas de bordes reales
  antialiasing y máscara GPU, e íconos vectoriales de Qt Quick Shapes.
- Portada panorámica de 2026 aleatoria, con degradado a negro.
- Tres filas horizontales de tamaño uniforme: TV, Series y Películas.
- Carga real (en segundo plano) de metadatos y portadas mediante
  `service_bootstrap`, `service_channels` y `service_browse` de C++.
- Movimiento horizontal nativo con mouse/touchpad y reacomodo con el tamaño
  de la ventana.
- Al tocar títulos o portadas, **no abre el diseño viejo**: por ahora aparece
  solo un mensaje de vista previa.

## Qué NO tiene todavía

Esta primera versión Qt es **solo para evaluar la apariencia de Inicio**:
aún no incluye pantallas Qt de catálogos, navegación completa, reproducción,
configuración, favoritos ni el control remoto. Esas funciones siguen operando
en el programa SDL original; se irán conectando solamente cuando el usuario
dé instrucciones.

No se ha modificado `main`, no se ha creado una rama extra y no se ha
publicado un release. Los archivos QML están en `ui/novaqt` y el adaptador
de datos de C++ en `source/nova_qt_home.cpp`.

La distribución de Qt requiere revisar y respetar su licencia y las
obligaciones de redistribución antes de una publicación estable.
