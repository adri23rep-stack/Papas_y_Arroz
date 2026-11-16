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
echo "[NOVA] Fase 2/2: instalando dependencias oficiales..."
# SDL2-compat provides SDL2's headers, libs, and CMake config. Other apps
# (FFmpeg, SDL2_ttf, whisper.cpp) may already depend on SDL2-compat.
# NEVER try to replace it with the legacy real-SDL2 package automatically.
sdl2_package="mingw-w64-ucrt-x86_64-sdl2-compat"
if pacman -Q mingw-w64-ucrt-x86_64-SDL2 >/dev/null 2>&1; then
    sdl2_package="mingw-w64-ucrt-x86_64-SDL2"
fi
echo "[NOVA] SDL2: usando $sdl2_package sin desinstalar nada."
packages=(
  mingw-w64-ucrt-x86_64-gcc
  mingw-w64-ucrt-x86_64-cmake
  mingw-w64-ucrt-x86_64-ninja
  mingw-w64-ucrt-x86_64-pkgconf
  "$sdl2_package"
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
pacman -S --needed --noconfirm "${packages[@]}"

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
for name in QtQuick QtQuick/Window QtQuick/Shapes QtQuick/Effects; do
    if [[ ! -s "$output/qml/$name/qmldir" ]]; then
        echo "[NOVA] Falta el modulo Qt $name. Revisa el resultado de windeployqt6." >&2
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
for name in cacert.pem service.template.json; do
    if [[ -f "packaging/runtime/switch/xuperswitch/$name" ]]; then
        cp "packaging/runtime/switch/xuperswitch/$name" "$output/$name"
    fi
done
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
