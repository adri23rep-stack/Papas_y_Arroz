# Auditoría Nova + Veyra y catálogo — 18 de noviembre de 2026

Rama exclusiva: `feature/nova-veyra-engine`. Base revisada: `ca3532db54b17284b94514aa888204c8d892a002`. No se modifica ni se fusiona `main`. No se borran ni migran configuraciones o sesiones del usuario.

## Qué está confirmado en el código

- La Alpha aislada no copia automáticamente `service.json` ni `device_state.json` (`Stage-NovaVeyraNative.ps1`). `load_installation` crea una identidad nueva solo cuando faltan configuración y estado. Una instalación que ya funciona puede tener otra identidad, configuración, versión declarada, certificados o perfil de prueba. Se conserva la protección que impide regenerar identidad si falta configuración pero existe estado.
- El perfil normal aún declara Horizon/Nintendo Switch. Los perfiles Android opcionales se eligen por `NOVA_DEVICE_PROFILE` y usan archivos de estado propios; no se cambió ninguno. Esto puede diferir de una instalación Android funcional, pero no demuestra por sí solo que el servidor la rechace por ese motivo.
- El arranque llama `snToken`, después `active` y después obtiene categorías. El estado persistido contiene serial/contador; el token de sesión se obtiene de nuevo y queda en memoria. Tener configuración no equivale a tener autorización actual.
- «Solicitando fuente» corresponde a `startPlayLive` o `startPlayVOD`, después configuración SLB y resolución Ranger si hay licencia. MPV y Veyra reciben el resultado después. El éxito del decodificador independiente no comprueba esas peticiones.
- Ranger solo entrega a Nova las fuentes HTTP/HTTPS compatibles; rechaza rutas locales del CDN, esquemas nativos y transportes que requieren un proxy aún no implementado. La compatibilidad de otra aplicación con esos transportes no está comprobada aquí.
- Las peticiones tenían 25 s por host y hasta dos hosts por etapa. El temporizador visual de 45 s descartaba la respuesta, pero no detenía el trabajo: varias etapas podían seguir ocupando el grupo de tareas después de vencer el plazo.
- Las copias de `ServiceSession` leían el mapa de canales y las categorías mientras otro hilo podía modificarlos. `trace` también incrementaba un contador compartido sin sincronización.
- Inicio no invalidaba la petición de reproducción; Atrás no permitía cancelarla. Una respuesta tardía podía abrir el reproductor después de volver a Inicio.
- Los avances descartaban errores VOD; además, una carga anterior podía actualizar las tarjetas durante una nueva actualización antes de sustituir la sesión.
- Los lanzadores Alpha activan el modo nativo estricto. Este bloqueaba la salida hacia MPV para películas con cabeceras, subtítulos o direcciones firmadas que el propio piloto Veyra rechazaba por incompatibilidad.

## Correcciones

1. Copias y cachés de sesión protegidas con un bloqueo corto, sin sostenerlo durante HTTP. La consulta de categorías de Nova también utiliza una copia aislada.
2. Un presupuesto total de 45 s para resolver una fuente, contado desde su creación en la interfaz, incluyendo espera en cola, hosts alternativos y etapas Ranger. Cancelación compartida con cURL; Atrás, Inicio y cierre invalidan la respuesta. El resultado viejo no inicia reproducción.
3. Avances opcionales limitados a 90 s, cancelados al actualizar/cerrar; resultados anteriores descartados desde que empieza la nueva actualización. Las categorías completas siguen disponibles independientemente de los avances.
4. Errores VOD conservados en estado visible y en `serviceStatus`, incluso si otra parte del catálogo carga.
5. Registro de etapa, identificador numérico de petición, intento, presupuesto, duración, código HTTP/cURL y tamaño. Solo se expone presencia de configuración/serial, nunca sus valores. Rechazos arbitrarios del servidor se sustituyen por un código genérico; no se registran direcciones, claves, tokens, cabeceras o cuerpos. Escritura del registro sincronizada.
6. El modo estricto Veyra solo bloquea MPV cuando la fuente es apta para el piloto nativo. Las fuentes que requieren cabeceras/subtítulos o que no cumplen sus condiciones de red conservan MPV. Se mantienen las restricciones de acceso y de envío de fuentes a Veyra.

No se cambian versión declarada, claves, identidad ni contenido de solicitudes de registro para intentar adivinar qué acepta el proveedor. `portal200001` sigue siendo un rechazo de motivo no confirmado.

## Pruebas y alcance

- Compilación local C++17 con `-Wall -Wextra -Werror`.
- Pruebas de catálogo, codecs, estado de reproducción, errores, instalación, guardados y política de selección nativa/MPV.
- Servidor HTTP/TLS local con credenciales inventadas: registro, activación nueva/existente, catálogo, series, películas, paginación, búsqueda, rechazo de permisos y protocolo Ranger.
- Regresiones: plazo global, cancelación durante HTTP, cancelación antes de empezar, espera en cola, plazo compartido entre etapas/hosts, navegación con plazo, errores que contienen datos privados y 8 hilos con 96 consultas/resoluciones simultáneas. Se comprueba que el archivo de configuración permanece intacto y que el registro no expone los valores sintéticos sensibles.
- Se corrige una expectativa antigua de prueba que aún exigía interpretar `portal200001` como versión fuera de servicio. Los servidores de prueba incluyen longitud de respuesta para no depender del cierre TLS de la plataforma.
- 17 pruebas Python del instalador y proxies Veyra: rollback, aislamiento de tokens, validación de estado y restricciones del puente Win32.
- Actions: nueva suite Linux offline, compilación y CTest Windows, pruebas reales del proceso Nova contra un motor simulado (incluido modo nativo estricto con cabeceras), y comprobación MSVC de adaptadores contra el código Veyra fijado. Resultados finales abajo.

Estas pruebas no son una conexión real al proveedor, una prueba de interfaz humana ni una validación GPU. El ensayo concurrente no sustituye a un detector de carreras.

## Falta validar en la PC

- Abrir la Alpha actualizada y comprobar registro → categorías → fuente para un canal, una película y un episodio. Si falla, el diagnóstico debe indicar la etapa y el código sin datos privados.
- Comparar localmente con la instalación funcional: presencia de los dos archivos, perfil utilizado, versión configurada y certificado. No compartir su contenido privado ni restablecer identidad.
- Probar Atrás/Inicio durante una fuente lenta; esperar más de 45 s y confirmar que no aparece un video tardío ni queda la interfaz bloqueada. Reintentar y actualizar mientras se cargan avances.
- Confirmar que películas con autorización/subtítulos usan MPV aunque el lanzador tenga modo estricto, y que el video local y TV pública apta siguen usando Veyra cuando corresponde.
- Repetir NVIDIA-VFG en RTX 3060, pantalla completa, audio, cierre y recuperación a MPV. Actions no dispone de esa GPU ni de los SDK privados del usuario.

## Incidencia detectada en Actions

La primera compilación Windows (`44397135310`) compiló los objetos de Nova, pero falló al enlazar `remote_browse_test`: su función simulada conservaba la firma anterior a la cancelación. Se actualizó esa función y se repitió el workflow; este fallo no se presenta como una compilación exitosa.

## Límites pendientes

No hay evidencia nueva del servicio real de esta instalación; no puede atribuirse su fallo exclusivamente a identidad, versión, red o CDN. El arranque de registro/categorías y la búsqueda completa aún conservan sus límites por petición, no un presupuesto global de toda la operación. La cancelación HTTP depende del progreso de cURL; un resolvedor DNS síncrono de una plataforma puede tardar en responder. La publicación tardía queda invalidada en la interfaz aunque el transporte tarde en salir. Las navegaciones/búsquedas distintas de la fuente conservan su comportamiento anterior al pulsar Inicio.


## Resultados finales verificados

Código compilado: `554b567b39c513487e9a7c78cd84d60f4e0d6a6a` (correcciones de producción en `1783d7c`, ajuste de prueba en `554b567`).

| Ejecución | Resultado y alcance |
| --- | --- |
| [Windows 44398203702](https://github.com/adri23rep-stack/Papas_y_Arroz/actions/runs/44398203702) | Correcta: compilación, 32/32 CTest, seis escenarios IPC (controles, fallo, caída, motor sin parche, fin y modo estricto con cabeceras), comprobaciones de audio/video y arranque de paquetes. |
| [Servicio offline 44398203725](https://github.com/adri23rep-stack/Papas_y_Arroz/actions/runs/44398203725) | Correcta: protocolo TLS simulado, cancelación, concurrencia, privacidad, suites C++ y 17 pruebas Python de instalador/proxies. |
| [Adaptador nativo 44397135128](https://github.com/adri23rep-stack/Papas_y_Arroz/actions/runs/44397135128) | Correcta: MSVC contra el código Veyra fijado; no prueba GPU ni enlace con SDK privados. |
| [Adaptador Qt 44397135178](https://github.com/adri23rep-stack/Papas_y_Arroz/actions/runs/44397135178) | Correcta: compatibilidad con las cabeceras reales de Veyra. |

Las dos comprobaciones de adaptadores corresponden a `1783d7c`; el cambio posterior solo corrigió una función simulada de prueba y documentación. No cambió los adaptadores ni el reproductor.

[Descargar paquete Windows de prueba](https://github.com/adri23rep-stack/Papas_y_Arroz/actions/runs/44398203702/artifacts/14469978799): `Nova-Veyra-Windows-Pilot`, 114107947 bytes. Es el paquete de Nova producido por CI, no una reconstrucción de la DLL NVIDIA-VFG con los SDK privados de la PC. SHA-256 del artefacto: `80d629f5986b575820a068c8780b97bc5507aa0213f4482a901eef23726eb7f0`.

Se verificó que `main` conserva `6643ed4e9d2120d004949319371fc502c660d318`. No se realizó ninguna conexión al servicio real con datos del usuario.
