# Nova Qt: perfiles de calidad y reinicio del reproductor

Esta implementación está en una rama aislada y no cambia el reproductor clásico.

## Modos disponibles

| Número | Perfil | Funcionamiento |
|---|---|---|
| 0 | Compatible (CPU) | Sin aceleración de decodificación; escala bicúbica |
| 1 | Equilibrado | Aceleración por hardware automática cuando MPV la admite; bicúbica |
| 2 | Nitidez alta | Aceleración disponible; Lanczos |
| 3 | Detalle extra | Lanczos y filtro FFmpeg CAS para fuente SD/HD |
| 4 | GPU FSRCNNX | Salida mpv GPU OpenGL y shader real; puede requerir escala de imagen |
| 5 | **RTX AI Super Resolution** | NVIDIA RTX Video Super Resolution real mediante `d3d11vpp` y Direct3D 11. El escalado se adapta a la resolución física del monitor (hasta x2). |

El shader `shaders/FSRCNNX_x2_8-0-4-1.glsl` debe viajar junto al ejecutable.
En GPU FSRCNNX, la etiqueta *configurado (verificar GPU)* **no garantiza**
que haya mejora visible, especialmente cuando el video ya está a resolución de
salida. FSRCNNX no activa RTX VSR ni HDR: el nuevo perfil NVIDIA es independiente.

## Control de calidad

En PC: engrane en la barra superior o tecla F9 con la ventana de Nova enfocada.
En iPhone: `/settings/get` expone `video_preset` (valores 0-5) y
`/settings/set` lo guarda. Los dos controles usan
`remote_playback_settings.txt`.

Con video activo, la solicitud se agrupa durante 450 ms; luego el trabajador
guarda estado del medio, volumen, velocidad, retrasos, pistas de audio/subtítulos,
visibilidad de subtítulos, pausa y proporción de imagen. Se cierra y reconstruye
**solo libmpv**, sin salir del catálogo ni perder el emparejamiento.
Para series/películas, MPV intenta recuperar la posición mediante `start`
y `hr-seek`. La precisión exacta depende del formato, keyframes y servidor.
Para canales en vivo, **no** se intenta seek: se reconecta la fuente actual.

Si el nuevo perfil falla al inicializar video o su filtro, se intenta recuperar
el último modo anterior funcional sin perder el estado guardado.
Si RTX VSR provoca desfase sostenido de audio/video, Nova restaura el perfil
anterior antes de desactivar RIFE. No se garantiza ausencia total de tirones. No se puede
garantizar reconexión de una URL que haya caducado o que el proveedor rechace.

## RTX AI Super Resolution: condiciones reales

- En **NVIDIA App → Sistema → Video**, activar **Super Resolution** para que el controlador NVIDIA aplique la mejora. Se necesita GPU RTX y controlador actualizado.
- Nova configura `gpu-context=d3d11` + `gpu-api=d3d11` y añade `d3d11vpp=scale=...:scaling-mode=nvidia` al final del filtro de video; **no** usa el DLSS de juegos.
- Video de menor resolución que la pantalla: aplica VSR hasta x2, sin exceder el tamaño físico del monitor. Si el video ya iguala la resolución del monitor, no aumenta el tamaño innecesariamente y mantiene Lanczos.
- Con RIFE GPU x2, los frames se generan con VapourSynth/Vulkan **antes** del escalado D3D11 NVIDIA. Ambos efectos consumen GPU; verificar fluidez en RTX 3060.
- El diagnóstico informa **VSR solicitado** o **confirmado por MPV** si aparece el aviso del driver. Solicitarlo no asegura mejora perceptible, ni garantiza que esté activado en NVIDIA App.
- Si el filtro D3D11 falla, se intenta restaurar el modo anterior preservando reproducción y posición. Los problemas de controlador o red pueden requerir intervención.

## MotionBoost (fotogramas y movimiento)

- `motion_mode=0`: desactivado, reproduce FPS de origen.
- `motion_mode=1`: MPV GPU temporal (mezcla frames, no inventa movimiento).
- `motion_mode=2`: **RIFE ncnn Vulkan (GPU) x2**, con
  VapourSynth + complemento `nova-gpu`. 24 FPS → objetivo de 48,
  30 FPS → objetivo de 60. Este procesamiento se ejecuta en la GPU,
  pero la CPU aún participa en decodificación, transferencias y sincronía.
- Se eliminó por completo `minterpolate` CPU de Nova para evitar el
  congelamiento de video y audio detectado en pruebas reales.
- La opción RIFE no debe arrancar si faltan plugin, modelo o runtime.
  Nova mantiene el video normal y muestra el motivo.
- Puede reducir fluidez si el GPU no logra procesar a tiempo.
  Se necesita prueba con la RTX 3060 real; no se garantizan 60 FPS.

Instalación: la compilación de Windows separa el complemento GPU de la
interfaz Qt y el addon de reproducción, para evitar paquetes gigantes.
El complemento solo se instala cuando el usuario lo extrae de manera
explícita. Modelo por defecto: RIFE v4.6 (Vulkan).

Código del plugin: https://github.com/styler00dollar/VapourSynth-RIFE-ncnn-Vulkan
Licencia y atribución de cada dependencia en su repositorio original.

## Validación

- Ejecutar en CI Windows `cmake --build`, `ctest` y smoke Qt.
- Probar un video local de varios minutos: a los 01:24:36, cambiar
  Equilibrado → Lanczos → CAS → FSRCNNX → RTX AI Super Resolution; comprobar tiempo aproximado, volumen,
  idioma, subtítulos y pausa.
- Repetir cambios rápidos y confirmar un único reinicio tras la última selección.
- En la RTX 3060, activar NVIDIA VSR, probar 720p sobre pantalla 1080p, comprobar indicador NVIDIA y diagnóstico de filtro.
- Probar RTX VSR + RIFE 2x durante varios minutos y verificar que, ante un retraso sostenido, se restaure la calidad anterior sin perder reproducción.
- Probar canal en vivo (sin seek), salida por CPU y un shader deliberadamente
  ausente para observar fallback sin cerrar Nova.
- Comprobar diagnóstico: origen, salida, decodificador real (`hwdec-current`)
  y filtro solicitado/aplicado; probar en la RTX 3060 real para medir rendimiento.

**No fusionar a `main` sin compilar y validar en Windows.**
