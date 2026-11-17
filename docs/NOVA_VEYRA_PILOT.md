# Nova + Veyra 2.1.9 — integración experimental (solo Windows)

**Rama:** feature/nova-veyra-engine. **Base:** main de Papas_y_Arroz.  
**Estado:** fuentes modificadas y pruebas CI configuradas; **sin validar todavía en RTX 3060 real**.
**Importante:** esta etapa conecta DOS procesos, no incorpora el renderizador D3D12 de
Veyra dentro de Qt/MPV de Nova. MPV sigue siendo el reproductor predeterminado.

## Qué hace realmente

Nova conserva catálogo, favoritos, lista de episodios, QR y servidor de control iPhone.
Con el piloto activado, intenta lanzar una **copia compilada y parcheada** de
Veyra 2.1.9. Después de establecer comunicación por tubería local, Nova envía
la fuente y recibe estado, progreso y pistas. No se usa OBS ni un puerto de red.

Funciones ya enlazadas por código:
- Reproducir/pausar, detener, saltar +/- segundos, volumen, velocidad.
- Elegir audio y subtítulos **que el motor Veyra detecte y pueda decodificar**.
- Regresar a un segundo guardado y actualizar Continuar viendo si Veyra reportó progreso.
- Pantalla completa (F11) y cambio de monitor (F10) del reproductor Veyra.
- Estado informativo: FPS de entrada/entrega, generación de FPS, SR, NR y HDR.
- El adaptador Veyra acepta comandos explícitos para FG/SR/NR/HDR y multiplicador
  de frames; **todavía NO hay selector conectado a esos comandos en la UI/iPhone**.
  Veyra utiliza su propio preset guardado.

Protecciones:
- Piloto apagado por defecto; las pruebas no modifican main.
- MPV se usa si el EXE no arranca, el puente no responde en 10 segundos,
  el medio no es compatible o faltan permisos de envío.
- El enlace de reproducción se manda por la entrada estándar **después** de que
  Veyra confirma el puente; no va en argumentos de arranque.
- Fuentes con cabeceras HTTP de autorización o subtítulos entregados por el
  servicio de Nova quedan en MPV: todavía no hay traducción fiel.
- El avance desconocido no pone Continuar viendo en 0.

## Preparar dos copias de compilación (no alterar el Veyra que ya funciona)

1. Extrae el ZIP de fuentes del usuario Veyra-NRVideo-main.zip en una carpeta
   aparte (la fuente revisada es Veyra 2.1.9, Qt QML).
2. Abre PowerShell en la raíz de Nova, rama feature/nova-veyra-engine.
3. Verifica las anclas del parche; si la versión no coincide, DETENTE:

\`\`\`powershell
python .\tools\Apply-VeyraNovaBridge.py --check "C:\Proyectos\Veyra-NRVideo-main"
\`\`\`

4. Instala el pequeño adaptador **solo en esa copia de fuentes**:

\`\`\`powershell
python .\tools\Apply-VeyraNovaBridge.py "C:\Proyectos\Veyra-NRVideo-main"
\`\`\`

5. Compila Veyra usando **Visual Studio/MSVC x64** y los requisitos propios de
   su repositorio. Objetivo Qt: veyra_qml_ui.exe. Su CMakePresets.json incluye
   la configuración x64-release (Ninja). Esto es un proyecto distinto de Nova
   y puede necesitar DLL, runtimes y modelos propios de Veyra.
6. Compila Nova desde esta rama con COMPILAR_NOVA_WINDOWS.cmd. No reemplaces
   ni borres service.json ni el emparejamiento existente.
7. En la carpeta del EXE de Nova, configura tu ruta REAL de Veyra parcheado:

\`\`\`powershell
$env:NOVA_VEYRA_PILOT = '1'
$env:NOVA_VEYRA_EXE = 'C:\Proyectos\Veyra-NRVideo-main\out\build\x64-release\veyra_qml_ui.exe'
.\XuperTVNovaQt.exe
\`\`\`

Si el EXE está en otra ruta, corrige la variable: NO uses esa ruta como
suposición del lugar final del compilador.

### Probar una fuente HTTP/HTTPS pública sin credenciales

\`\`\`powershell
$env:NOVA_VEYRA_ALLOW_NETWORK = '1'
.\XuperTVNovaQt.exe
\`\`\`

Por defecto no se envían URLs con parámetros de consulta. **Solo si sabes que
el enlace es tuyo/autorizado y aceptas que Veyra podría registrar la URL en
sus propios diagnósticos**, puedes habilitar además:

\`\`\`powershell
$env:NOVA_VEYRA_ALLOW_SIGNED_NETWORK = '1'
.\XuperTVNovaQt.exe
\`\`\`

Esto NO añade soporte para cabeceras HTTP especiales ni garantiza que un
proveedor permita la reproducción. Mantener esas fuentes en MPV es intencional.

### Desactivar por completo

\`\`\`powershell
Remove-Item Env:\NOVA_VEYRA_PILOT -ErrorAction SilentlyContinue
Remove-Item Env:\NOVA_VEYRA_EXE -ErrorAction SilentlyContinue
Remove-Item Env:\NOVA_VEYRA_ALLOW_NETWORK -ErrorAction SilentlyContinue
Remove-Item Env:\NOVA_VEYRA_ALLOW_SIGNED_NETWORK -ErrorAction SilentlyContinue
\`\`\`

Deshacer solo el parche de la copia de Veyra, sin tocar Nova ni el Veyra que
ya tenías instalado:

\`\`\`powershell
python .\tools\Apply-VeyraNovaBridge.py --restore "C:\Proyectos\Veyra-NRVideo-main"
\`\`\`

## Límites conocidos y tareas de Astra

- **Sin pruebas de compilación/reproducción Veyra modificada.** La CI Windows
  actualmente compila Nova y ejecuta pruebas offline del instalador del parche.
  No compila automáticamente el Veyra gigante ni ejecuta GPU NVIDIA.
- Los comandos de mejora de imagen están implementados en el puente, pero
  falta crear y conectar controles de Nova/iPhone para ellos.
- Si una fuente exige cabeceras HTTP, DRM o subtítulos separados de la API,
  sigue con MPV hasta que exista un soporte seguro y verificable.
- No están migrados cuadros de ajustes ni la interfaz de Nova dentro del
  proceso de Veyra. Sigue siendo una ventana de video independiente.
- Queda probar seek/HLS en vivo, reconexión, sincronización audio/video,
  pausa, cambio rápido de episodios, TV con streaming irregular y múltiples
  pantallas.
- Veyra (incluido el adaptador que se compila en él) se distribuye bajo
  GPL-3.0: respetar licencias y avisos al publicar un paquete derivado.
  El código Veyra NO se ha copiado ni enlazado como biblioteca en Nova.

## Validación de regresión

Ejecutar primero:
\`\`\`powershell
python -m unittest discover -s tests -p test_veyra_bridge_installer.py -v
\`\`\`

Y después en una PC con RTX 3060:
1. Sin variables, MPV de Nova abre el catálogo, funciona iPhone y pantalla completa.
2. Veyra parcheado abre una película local; imagen/audio y filtros iguales a Veyra.
3. Pausa, seek, volumen, pista audio y subtítulo desde iPhone.
4. F10/F11, dos monitores, reanudar una película desde minuto guardado.
5. Cerrar el Veyra hijo: Nova continúa abierto, sin memoria corrupta.
6. EXE Veyra sin parche, ausente o que falla: MPV recupera reproducción.
7. HLS público sin auth; probar cambios rápidos de canal y desincronización.
8. Probar token/cabeceras: MPV debe conservar esas fuentes sin filtrarlas.

**No hacer merge a main antes de superar compilación, smoke y pruebas reales.**
Consulta docs/NOVA_VEYRA_HANDOFF_ASTRA.md para el siguiente trabajo técnico.
