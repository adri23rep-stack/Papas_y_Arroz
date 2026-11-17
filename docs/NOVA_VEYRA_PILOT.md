# Nova + Veyra: primer experimento de reproducción

**Estado:** piloto experimental, NO es un port completo ni se ha compilado/probado en una GPU Windows.
**Rama:** `feature/nova-veyra-engine` (separada de `main`).

## Qué hace esta primera implementación

Nova Qt mantiene su catálogo, servicio y protocolo con iPhone. Con el opt-in habilitado,
al seleccionar un medio compatible intenta abrir **la aplicación instalada de Veyra**
con la fuente elegida como argumento. El motor GPU, ajustes y filtros usados son los
del Veyra instalado por el usuario. Si no es elegible o no arranca, Nova vuelve a MPV.

No se han copiado fuentes de Veyra dentro del repositorio. Esta primera iteración
usa una **integración de procesos separados**, antes de decidir si migramos el UI de
Nova a la base Veyra. No afirma tener la misma integración de controles del reproductor.

## Requisitos

1. Construir la rama para Windows con `COMPILAR_NOVA_WINDOWS.cmd`.
2. Instalar Veyra 2.1.9 (se revisó el código fuente adjunto de esa versión);
   localizar su **ejecutable Qt/QML** real (al compilar el código fuente se llama\n   `veyra_qml_ui.exe`; el instalador podría cambiarle el nombre). La edición clásica puede no aceptar
   argumentos de entrada.
3. Usar una ruta absoluta al .exe de Veyra. No se descarga ni instala Veyra desde Nova.

## Prueba en PowerShell (primero con un archivo local)

Ajustar la ruta al ejecutable de Veyra:

```powershell
$env:NOVA_VEYRA_PILOT = '1'
$env:NOVA_VEYRA_EXE = 'C:\Ruta\A\veyra_qml_ui.exe'
# Abrir Nova compilado desde esta rama, desde su carpeta de ejecución:
.\XuperTVNovaQt.exe
```

Seleccionar en Nova un archivo local que exista (si está disponible en el catálogo).
Veyra debería abrirse con la reproducción. Cerrar Veyra debería actualizar el estado
de Nova. **Los controles del iPhone todavía no controlan la instancia de Veyra.**

Para probar una URL de emisión pública **sin credenciales, consulta ni cabeceras**:

```powershell
$env:NOVA_VEYRA_ALLOW_NETWORK = '1'
.\XuperTVNovaQt.exe
```

El piloto no enviará a Veyra fuentes con cabeceras HTTP de autenticación,
subtítulos separados, URL con usuario/contraseña, consulta o fragmento; en esos
casos seguirá usando MPV. Esta restricción evita filtrar datos confidenciales a
la línea de comandos de otro proceso. Incluso las URLs permitidas podrían
contener identificadores en su ruta; probar únicamente con fuentes públicas.

Desactivar todo (las variables solo afectan la sesión actual de PowerShell):

```powershell
Remove-Item Env:\NOVA_VEYRA_PILOT -ErrorAction SilentlyContinue
Remove-Item Env:\NOVA_VEYRA_EXE -ErrorAction SilentlyContinue
Remove-Item Env:\NOVA_VEYRA_ALLOW_NETWORK -ErrorAction SilentlyContinue
```

## Pendientes antes de considerar Veyra como motor predeterminado

- Seleccionar la base arquitectónica final: portar la UI/control de Nova al
  proyecto Veyra o implementar IPC local entre ambos procesos.
- Conectar **pausa, volumen, seek, cambio de canal, pistas, subtítulos,
  pantalla completa y cambio de monitor** sin usar teclas simuladas.
- Conservar el minuto reproducido y la sesión tras cambiar de motor.\n  El piloto conserva el valor anterior en Nova y no guarda el progreso que se\n  alcanza dentro de Veyra.
- Resolver autorización y fuentes protegidas **sin exponer tokens** en argumentos,
  registros ni configuraciones.
- Validar canales HLS, MPEG-TS, audio multilingüe, subtítulos y cambio rápido de
  canal; medir GPU, latencia, FPS entregados, audio y reconexiones.
- Evaluar licencias antes de integrar código: Veyra es GPL-3.0; la invocación
  externa aquí no supone incorporar su código en la compilación de Nova.

## Cómo evaluar el piloto

1. Con la variable apagada, confirmar que MPV sigue funcionando como antes.
2. Activar piloto con una película local y ver si Veyra abre y muestra imagen/audio.
3. Cerrar Veyra y confirmar que Nova no se cierra.
4. Probar un canal público sin autenticación, solo si se habilitó red.
5. Probar un canal protegido y confirmar que Nova vuelve a MPV.
6. Repetir sin Veyra disponible: MPV debe seguir abriendo el medio.

**No hacer merge en main** hasta completar la prueba real en RTX 3060.

## Iteración 2: control real de Veyra mediante canal privado

**Implementación en la rama:** integraciones en:
- integrations/veyra/NovaStdioBridge.hpp
- tools/Apply-VeyraNovaBridge.py

El puente recibe órdenes JSON por la entrada estándar de Veyra y devuelve su
estado por la salida estándar. No abre puertos locales ni de red, no usa OBS y
solo se activa cuando Nova lanza Veyra con NOVA_VEYRA_STDIO=1.

**Advertencia:** una instalación estándar de Veyra NO incluye este puente.
Hay que compilar una copia independiente de Veyra con el parche para controlar
el reproductor desde Nova/iPhone.

### Preparación en Windows

1. Extraer el ZIP de código fuente Veyra en una carpeta distinta del programa
   Veyra que ya tienes funcionando.
2. Desde la raíz del repositorio Nova en esta rama, ejecutar en PowerShell:

    python .\tools\Apply-VeyraNovaBridge.py "C:\Ruta\Veyra-NRVideo-main"

3. Compilar el proyecto Veyra con sus instrucciones oficiales de CMake y
   Visual Studio. El objetivo Qt se llama veyra_qml_ui.exe.
4. Compilar Nova desde esta rama con COMPILAR_NOVA_WINDOWS.cmd.
5. Abrir Nova desde la carpeta donde se compiló con las variables siguientes:

    $env:NOVA_VEYRA_PILOT='1'
    $env:NOVA_VEYRA_EXE='C:\Ruta\VeyraBuild\veyra_qml_ui.exe'
    .\XuperTVNovaQt.exe

El script realiza una copia de respaldo de apps/veyra-qml/main.cpp llamada
main.cpp.nova-pre-bridge.bak. No modifica el Veyra ya instalado.

### Funciones ahora enlazadas en código (pendientes de prueba Windows)

- Reproducción/pausa, volumen, velocidad y salto de tiempo desde Nova/iPhone.
- Selección de pista de audio usando los índices publicados por Veyra.
- Posición y duración devueltas por Veyra.
- Continuar viendo: guarda la posición conocida al cerrar Veyra en videos
  a demanda; en TV en vivo no intenta guardar segundos.
- MPV sigue como modo predeterminado. Si no arranca el EXE externo, MPV es el
  camino de respaldo. Si arranca Veyra sin el parche, reproducirá pero no
  responderá a los nuevos controles de Nova.

### Todavía pendiente

- Compilar ambas aplicaciones y validar en una PC Windows/RTX 3060.
- Control remoto de subtítulos, cambio de monitor y pantalla completa de Veyra.
- Configuración de filtros VFG/SR/HDR desde el iPhone.
- Transferencia segura de fuentes que requieren autorización, sin exponer tokens
  ni contraseñas en la línea de comandos o registros.
- Comprobar cierre del motor, reconexiones de TV en vivo y sincronización A/V.

No fusionar a main hasta superar estas pruebas.
