#!/usr/bin/env bash
# Runs only inside an MSYS2 UCRT64 shell, launched by Compilar-Nova.ps1.
# The script installs trusted MSYS2 packages, builds from the working
# checkout and deploys a portable EXE with all runtime DLLs/QML imports.
set -euo pipefail
# The launcher's login shell may start in $HOME. Locate the repository
# relative to this script, not relative to the current working directory.
cd -- "$(dirname -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")")"
if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "[NOVA] Error: abre el proyecto con Compilar-Nova.ps1 (MSYS2 UCRT64)." >&2
    exit 1
fi
if [[ ! -f CMakeLists.txt || ! -f ui/novaqt/Main.qml ]]; then
    echo "[NOVA] No se encontraron las fuentes en la carpeta actual." >&2
    exit 1
fi

# Updating the MSYS2 runtime and immediately forking tools from the
# SAME Bash process causes intermittent "dofork" / errno 11 failures.
# PowerShell runs --update-only in one Bash process, waits for it to exit,
# then starts a fresh Bash process for installation and compilation.
if [[ "${1:-}" == "--update-only" ]]; then
    echo "[NOVA] Fase 1/2: actualizando MSYS2..."
    pacman -Syu --noconfirm
    echo "[NOVA] MSYS2 actualizado. Reiniciando Bash para compilar..."
    exit 0
fi
echo "[NOVA] Revisando dependencias locales sin modificar MSYS2..."
# SDL2 is a virtual dependency provided by the installed SDL2-compat
# or the legacy SDL2 package. Some versions of "pacman -Q SDL2" report
# success for a provider (not just the exact installed name), so checking
# it here can mistakenly force an incompatible SDL2 replacement.
# Do not request EITHER provider explicitly. ffmpeg and SDL2_ttf already
# depend on SDL2 and pacman will preserve the installed valid provider.
echo "[NOVA] SDL2: conservando el proveedor instalado; no se forzara reemplazo."
packages=(
  mingw-w64-ucrt-x86_64-gcc
  mingw-w64-ucrt-x86_64-cmake
  mingw-w64-ucrt-x86_64-ninja
  mingw-w64-ucrt-x86_64-pkgconf
  mingw-w64-ucrt-x86_64-SDL2_ttf
  mingw-w64-ucrt-x86_64-sdl3
  mingw-w64-ucrt-x86_64-mpv
  mingw-w64-ucrt-x86_64-ffmpeg
  mingw-w64-ucrt-x86_64-curl
  mingw-w64-ucrt-x86_64-mbedtls
  mingw-w64-ucrt-x86_64-libpng
  mingw-w64-ucrt-x86_64-libjpeg-turbo
  mingw-w64-ucrt-x86_64-libwebp
  mingw-w64-ucrt-x86_64-ntldd
  mingw-w64-ucrt-x86_64-qrencode
  mingw-w64-ucrt-x86_64-qt6-base
  mingw-w64-ucrt-x86_64-qt6-declarative
  mingw-w64-ucrt-x86_64-qt6-imageformats
)
# Query the ALREADY INSTALLED packages first. pacman -Q is read-only,
# so an unrelated MSYS2 install holding db.lck cannot block this build.
missing_packages=()
for package in "${packages[@]}"; do
    if ! pacman -Q "$package" >/dev/null 2>&1; then
        missing_packages+=("$package")
    fi
done
if (( ${#missing_packages[@]} == 0 )); then
    echo "[NOVA] Todas las dependencias ya estan instaladas. Omitiendo pacman."
else
    echo "[NOVA] Faltan ${#missing_packages[@]} paquetes de la herramienta."
    # Never auto-delete /var/lib/pacman/db.lck: another pacman may be busy.
    if [[ -e /var/lib/pacman/db.lck ]]; then
        echo "[NOVA] La base de paquetes esta bloqueada (db.lck)." >&2
        echo "[NOVA] Cierra instaladores MSYS2 activos y reintenta. No borres el bloqueo sin comprobar que no hay otro pacman trabajando." >&2
        exit 1
    fi
    pacman -S --needed --noconfirm "${missing_packages[@]}"
fi

if [[ "${NOVA_CLEAN:-0}" == 1 ]]; then
    echo "[NOVA] Limpiando SOLO los resultados antiguos de compilación..."
    rm -rf build/nova-local
fi

echo "[NOVA] Configurando proyecto C++..."
cmake -S . -B build/nova-local -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
echo "[NOVA] Compilando el ejecutable Qt..."
cmake --build build/nova-local --target XuperTVNovaQt --parallel 4

output="dist/Xuper-TV-NovaQt-Local"
mkdir -p "$output"
cp build/nova-local/XuperTVNovaQt.exe "$output/"

# Qt's Windows deployment tool collects Qt DLLs, modules and plugins.
echo "[NOVA] Empaquetando Qt Quick..."
/ucrt64/bin/windeployqt6.exe --release --no-translations \
    --qmldir ui/novaqt \
    --dir "$output" \
    "$output/XuperTVNovaQt.exe"

printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > "$output/qt.conf"
# Verify only modules actually imported by the current QML files.
# The legacy UI once imported QtQuick.Shapes, but Nova no longer does.
# windeployqt may create an empty Shapes directory without its qmldir:
# that is NOT a packaging error or a required dependency.
for name in QtQuick QtQuick/Window QtQuick/Effects; do
    if [[ ! -s "$output/qml/$name/qmldir" ]]; then
        echo "[NOVA] Falta el modulo Qt requerido: $name." >&2
        exit 1
    fi
done

echo "[NOVA] Copiando DLLs de libmpv, SDL y servidor del iPhone..."
# ntldd traverses Windows DLL dependencies, unlike a simple 'ldd'.
ntldd -R "$output/XuperTVNovaQt.exe" > build/nova-local/nova-dependencies.txt
while IFS= read -r dll; do
    [[ -z "$dll" ]] && continue
    found=$(find /ucrt64/bin -maxdepth 1 -iname "$dll" -print -quit)
    if [[ -n "$found" ]]; then
        cp -n "$found" "$output/" || true
    fi
done < <(
    sed -nE 's/^[[:space:]]*([^[:space:]]+\.dll)[[:space:]]+=>.*/\1/ip' \
        build/nova-local/nova-dependencies.txt | sort -fu
)

# The MSYS2 SDL2 compatibility DLL loads SDL3.dll dynamically, so ntldd
# often cannot detect SDL3. This was a real crash in the old Windows build.
for runtime in SDL2.dll SDL3.dll SDL2_ttf.dll libmpv-2.dll libcurl-4.dll libqrencode.dll; do
    if [[ ! -f "$output/$runtime" && -f "/ucrt64/bin/$runtime" ]]; then
        cp "/ucrt64/bin/$runtime" "$output/"
    fi
    if [[ ! -s "$output/$runtime" ]]; then
        echo "[NOVA] Dependencia requerida ausente: $runtime" >&2
        exit 1
    fi
done

# Offline headless smoke checks can use the offscreen plugin. This is not
# mandatory for normal use, but helps diagnose startup without a display.
if [[ -s /ucrt64/share/qt6/plugins/platforms/qoffscreen.dll ]]; then
    mkdir -p "$output/platforms"
    cp /ucrt64/share/qt6/plugins/platforms/qoffscreen.dll "$output/platforms/"
fi

# These are PUBLIC template / CA files. Never copy service.json, stored
# PINs, tokens or a local device identity over from another installation.
for name in catalog.m3u LICENSE DEPENDENCIAS.md README_NOVA_QT.md; do
    if [[ -f "$name" ]]; then cp "$name" "$output/"; fi
done
for name in cacert.pem service.template.json test.mp4; do
    if [[ -f "packaging/runtime/switch/xuperswitch/$name" ]]; then
        cp "packaging/runtime/switch/xuperswitch/$name" "$output/$name"
    fi
done
# The original template came from the Nintendo Switch build, where the CA
# path begins with sdmc:/. Fix ONLY the new public copy for Windows.
# Existing service.json and device_state.json remain untouched.
if [[ ! -s "$output/service.template.json" || ! -s "$output/cacert.pem" ]]; then
    echo "[NOVA] Faltan la plantilla publica o el certificado CA para activar el catalogo." >&2
    exit 1
fi
sed -i 's#sdmc:/switch/xuperswitch/cacert.pem#cacert.pem#g' "$output/service.template.json"
if grep -q 'sdmc:/switch/xuperswitch/' "$output/service.template.json"; then
    echo "[NOVA] La plantilla aun usa una ruta de Switch; no es valida para Windows." >&2
    exit 1
fi
if [[ ! -s "$output/test.mp4" ]]; then
    echo "[NOVA] Falta test.mp4: no se puede verificar la reproduccion local." >&2
    exit 1
fi
echo "[NOVA] Verificando que libmpv decodifique el video de prueba..."
if ! /ucrt64/bin/mpv.exe --no-config --no-audio --vo=null --frames=3     "$output/test.mp4"; then
    echo "[NOVA] mpv no pudo leer test.mp4; comprueba el paquete multimedia." >&2
    exit 1
fi
if [[ -d licenses ]]; then
    mkdir -p "$output/licenses"
    cp -a licenses/. "$output/licenses/"
fi
# Include bundled dependency licenses when available.
for package in qt6-base qt6-declarative mpv qrencode; do
    if [[ -d "/ucrt64/share/licenses/$package" ]]; then
        mkdir -p "$output/licenses/$package"
        cp -a "/ucrt64/share/licenses/$package/." "$output/licenses/$package/"
    fi
done
echo "[NOVA] Probando arranque Qt y distribución a 1920x1080..."
if ! QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 30s "$output/XuperTVNovaQt.exe" --smoke-fullscreen; then
    echo "[NOVA] FALLO el arranque de Qt. Diagnóstico:" >&2
    if [[ -f "$output/nova-qt-smoke.log" ]]; then
        cat "$output/nova-qt-smoke.log" >&2
    fi
    exit 1
fi
rm -f "$output/nova-qt-smoke.log"
echo "[NOVA] Build local terminado:"
ls -lh "$output/XuperTVNovaQt.exe"
echo "[NOVA] Carpeta: $(cygpath -w "$(pwd)/$output")"
echo "[NOVA] La configuración de usuario y los favoritos guardados no se borraron."
