#!/usr/bin/env bash
# Build the optional Vulkan RIFE add-on from pinned upstream releases. Only
# Windows MSYS2 UCRT64. No personal tokens, pairing data or model downloads
# occur at end-user startup.
set -euo pipefail

cd -- "$(dirname -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")")"
out="dist/Xuper-TV-NovaQt-GPU-RIFE-Addon/Xuper-TV-NovaQt-Preview"
gpu="$out/nova-gpu"
models="$gpu/models/rife-v4.6_ensembleFalse"
mkdir -p "$models"
cp nova-gpu/rife.vpy "$gpu/rife.vpy"

# Pin official upstream release and verify its GitHub-published sha256.
plugin_url="https://github.com/styler00dollar/VapourSynth-RIFE-ncnn-Vulkan/releases/download/r9_mod_v33/librife_windows_x86-64.dll"
curl --fail --location --retry 3 --silent --show-error "$plugin_url" \
  -o "$gpu/librife_windows_x86-64.dll"
echo "36a25b471be88e6f915320c818022dc8657dd9beac22a8c3158bd7f4260cc410  $gpu/librife_windows_x86-64.dll" | sha256sum -c -

# Pin the corresponding 4.6 model to a repository commit and Git blob SHA.
repo="https://raw.githubusercontent.com/styler00dollar/VapourSynth-RIFE-ncnn-Vulkan"
commit="c3ec6aabc07c8fa37a4f58d7fed9e2ad1fc1b13f"
for f in flownet.bin flownet.param; do
  curl --fail --location --retry 3 --silent --show-error \
    "$repo/$commit/models/rife-v4.6_ensembleFalse/$f" -o "$models/$f"
done
[[ "$(git hash-object "$models/flownet.bin")" == "5e16e8112ec5213348cc8928705f1cd8dd19e8f9" ]]
[[ "$(git hash-object "$models/flownet.param")" == "609feeed0ac8b446de68deb9a5e3e272b6b52be2" ]]
test "$(wc -c < "$models/flownet.bin")" -eq 10629680
test "$(wc -c < "$models/flownet.param")" -eq 26915

# VapourSynth in MSYS2 is Python-driven. Ship a private, isolated Python
# standard library plus its VapourSynth binding in the OPTIONAL addon, not
# in the 98MB primary Qt zip.
python_version=$(/ucrt64/bin/python.exe -c 'import sys;print(f"{sys.version_info.major}.{sys.version_info.minor}")')
python_root="/ucrt64/lib/python$python_version"
test -d "$python_root"
test -s /ucrt64/bin/libvapoursynth-script-0.dll
test -s /ucrt64/bin/libvapoursynth.dll
mkdir -p "$gpu/python/lib/python$python_version/site-packages"
# Copy stdlib without optional development/tests to reduce the ZIP size.
rsync -a "$python_root/" "$gpu/python/lib/python$python_version/" \
  --exclude='site-packages/' --exclude='test/' --exclude='tests/' \
  --exclude='__pycache__/' --exclude='tkinter/' --exclude='idlelib/' \
  --exclude='turtledemo/' --exclude='ensurepip/' --exclude='venv/' \
  --exclude='pydoc_data/' --exclude='lib2to3/'
# MSYS2 VapourSynth has a platform-tagged .pyd, not generic MSVC .pyd.
for module in "$python_root"/site-packages/vapoursynth*.pyd; do
  test -s "$module"
  cp "$module" "$gpu/python/lib/python$python_version/site-packages/"
done
for module in "$python_root"/site-packages/VapourSynth*.egg-info; do
  [[ -e "$module" ]] && cp -a "$module" "$gpu/python/lib/python$python_version/site-packages/" || true
done

# libmpv already ships normally; the GPU add-on adds optional dynamic
# VapourSynth/Python DLLs and their missing transitive UCRT64 dependencies.
for source in /ucrt64/bin/libvapoursynth-script-0.dll /ucrt64/bin/libvapoursynth.dll; do
  cp "$source" "$out/"
done
# On Windows, mpv loads VapourSynth by the official DLL name VSScript.dll.
# MSYS2 uses libvapoursynth-script-0.dll for the same VSScript API.
cp /ucrt64/bin/libvapoursynth-script-0.dll "$out/VSScript.dll"
ntldd -R /ucrt64/bin/libvapoursynth-script-0.dll > "$gpu/vs-dependencies.txt" || true
ntldd -R /ucrt64/bin/libvapoursynth.dll >> "$gpu/vs-dependencies.txt" || true
while IFS= read -r dll; do
  [[ -z "$dll" ]] && continue
  path=$(find /ucrt64/bin -maxdepth 1 -iname "$dll" -print -quit)
  [[ -n "$path" ]] && cp -n "$path" "$out/" || true
done < <(sed -nE 's/^[[:space:]]*([^[:space:]]+\.dll)[[:space:]]+=>.*/\1/ip' "$gpu/vs-dependencies.txt" | sort -fu)
# Different releases of Python use a different DLL basename.
for python_dll in /ucrt64/bin/libpython*.dll /ucrt64/bin/python3*.dll; do
  [[ -f "$python_dll" ]] && cp -n "$python_dll" "$out/" || true
done
rm "$gpu/vs-dependencies.txt"

test -s "$out/libvapoursynth-script-0.dll"
test -s "$out/VSScript.dll"
test -s "$out/libvapoursynth.dll"
test -s "$gpu/librife_windows_x86-64.dll"
test -s "$gpu/rife.vpy"

cat > "$gpu/README.txt" <<'README'
Nova MotionBoost RIFE GPU Vulkan (experimental add-on)
Source: styler00dollar/VapourSynth-RIFE-ncnn-Vulkan r9_mod_v33
Model: RIFE v4.6, MIT licensed plugin with upstream model licenses.
Requires a Vulkan-capable GPU and working NVIDIA/AMD/Intel drivers.
Place this add-on in the SAME folder as the Nova Qt + playback addon.
The big Python runtime belongs in nova-gpu/python and is only used for RIFE.
Nova will never run CPU minterpolate as a fallback.
RTX 3060: target x2 (24->48, 30->60 FPS), not guaranteed real time.
README
echo "GPU RIFE add-on prepared:"
du -sh "$out" "$gpu/python"
