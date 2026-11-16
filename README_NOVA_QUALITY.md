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

El shader `shaders/FSRCNNX_x2_8-0-4-1.glsl` debe viajar junto al ejecutable.
En GPU FSRCNNX, la etiqueta *configurado (verificar GPU)* **no garantiza**
que haya mejora visible, especialmente cuando el video ya está a resolución de
salida. No se anuncia RTX Video Super Resolution, HDR ni interpolación.

## Control de calidad

En PC: engrane en la barra superior o tecla F9 con la ventana de Nova enfocada.
En iPhone: `/settings/get` expone `video_preset` (valores 0-4) y
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
el último modo anterior funcional sin perder el estado guardado. No se puede
garantizar reconexión de una URL que haya caducado o que el proveedor rechace.

## MotionBoost (fotogramas y movimiento)

MotionBoost es independiente de los cinco perfiles de nitidez. PC e iPhone
comparten el nuevo ajuste `motion_mode` (0-2), que se guarda en
`remote_playback_settings.txt`. El motor usa el mismo reinicio seguro de MPV
y conserva la posición, audio, subtítulos, volumen y pausa.

| Modo | Técnica | Limitaciones |
|---|---|---|
| 0 Desactivado | FPS del archivo original | Ninguna carga adicional |
| 1 Suavizado MPV | Interpolación temporal y sincronización con pantalla | Mezcla fotogramas, **no** estima movimiento real ni garantiza 60 FPS |
| 2 Movimiento 60 FPS (experimental) | FFmpeg `minterpolate` con compensación de movimiento `mci` | Procesa principalmente en **CPU**, costoso a 1080p o para canales en vivo |

La opción 2 sí genera cuadros calculados entre los originales y fija
60 FPS como objetivo del filtro, pero **no garantiza 60 FPS fluidos en tiempo real**.
El resultado depende de la CPU, del video y de la frecuencia del monitor.
Puede introducir deformaciones, retrasos y desincronización si se satura.
La interfaz muestra `estimated-vf-fps` (FPS estimados tras el filtro), no un
contador de FPS realmente presentados por el monitor. Para medir fluidez
real hay que probar en el equipo y observar fotogramas perdidos y audio.

**RTX 3060:** la aceleración automática sigue disponible para decodificar
video, pero el filtro `minterpolate` NO usa los Tensor Cores ni
NVIDIA Optical Flow de la GPU. Integrar RIFE en GPU requerirá otro motor
VapourSynth/RIFE y un paquete verificado, no incluido en esta entrega.
No confundir MotionBoost con DLSS Frame Generation de videojuegos.

Los cambios entre modos se agrupan durante 450 ms y reinician **solo** MPV.
Un filtro no compatible intenta restaurar el perfil anterior. Los canales
en vivo vuelven a conectar a la señal actual; no intentan volver al mismo
segundo ni pueden garantizar la misma latencia.

Para comprobar que se generaron fotogramas, el workflow incluye una prueba
con fuente sintética 24 FPS y salida interpolada 60 FPS mediante FFmpeg,
además de una prueba con el puente `lavfi` de MPV. Son pruebas funcionales
del filtro, no benchmarks en una RTX 3060 real.

## Validación

- Ejecutar en CI Windows `cmake --build`, `ctest` y smoke Qt.
- Probar un video local de varios minutos: a los 01:24:36, cambiar
  Equilibrado → Lanczos → CAS → FSRCNNX; comprobar tiempo aproximado, volumen,
  idioma, subtítulos y pausa.
- Repetir cambios rápidos y confirmar un único reinicio tras la última selección.
- Probar canal en vivo (sin seek), salida por CPU y un shader deliberadamente
  ausente para observar fallback sin cerrar Nova.
- Comprobar diagnóstico: origen, salida, decodificador real (`hwdec-current`)
  y filtro solicitado/aplicado; probar en la RTX 3060 real para medir rendimiento.

**No fusionar a `main` sin compilar y validar en Windows.**
