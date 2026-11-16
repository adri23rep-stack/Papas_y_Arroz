#!/usr/bin/env bash
# Run the packaged filter on a local moving clip using the runner's test-only
# Vulkan driver. Never include Mesa in the user's playback ZIPs.
set -euo pipefail
stage="dist/Xuper-TV-NovaQt-Smoke/Xuper-TV-NovaQt-Preview"
cp -a dist/Xuper-TV-NovaQt-GPU-RIFE-Addon/Xuper-TV-NovaQt-Preview/. "$stage/"
cp tests/nova_rife_frames.vpy /ucrt64/bin/mpv.exe "$stage/"
cd "$stage"
for fps in 24 60; do
  /ucrt64/bin/ffmpeg.exe -hide_banner -loglevel error -y \
    -f lavfi -i "testsrc2=size=160x96:rate=$fps" -t 1 -c:v ffv1 "input-$fps.mkv"
done
stdlib=(nova-gpu/python/lib/python3.*)
export PYTHONHOME="$(cygpath -wa nova-gpu/python)"
python_lib="$(cygpath -wa "${stdlib[0]}")"
export PYTHONPATH="$python_lib;$python_lib/lib-dynload;$python_lib/site-packages"
export VK_DRIVER_FILES="$(cygpath -wa /ucrt64/share/vulkan/icd.d/lvp_icd.x86_64.json)"
test -s /ucrt64/share/vulkan/icd.d/lvp_icd.x86_64.json
for fps in 60 24; do
  rm -f frame-*.txt
  # The 60fps pass-through also checks embedded Python with no MSYS DLL path.
  # Lavapipe's LLVM runtime is needed only by the 24fps Vulkan test.
  runtime_path=/c/Windows/System32
  [[ "$fps" == 24 ]] && runtime_path="$runtime_path:/ucrt64/bin"
  PATH="$runtime_path" ./mpv.exe --no-config --no-audio --vo=null --frames=5 \
    '--vf=vapoursynth=file=nova_rife_frames.vpy:concurrent-frames=2' "input-$fps.mkv"
  /ucrt64/bin/python.exe - "$fps" <<'PY'
from fractions import Fraction
from pathlib import Path
import sys
fps = int(sys.argv[1])
frames = [Path(f"frame-{n}.txt").read_text().split() for n in range(5)]
# Matroska timestamps are rounded to milliseconds.
expected = 1 / (48 if fps == 24 else 60)
assert all(abs(float(Fraction(t)) - expected) <= 0.0011 for t, _ in frames), frames
if fps == 24:
    assert len({digest for _, digest in frames[:3]}) == 3, frames
print(f"{fps} fps input: {frames}")
PY
done
