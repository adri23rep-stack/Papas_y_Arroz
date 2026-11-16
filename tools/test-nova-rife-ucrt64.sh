#!/usr/bin/env bash
# Run the packaged filter on a local moving clip using the runner's test-only
# Vulkan driver. Never include Mesa in the user's playback ZIPs.
set -euo pipefail
stage="dist/Xuper-TV-NovaQt-Smoke/Xuper-TV-NovaQt-Preview"
cp -a dist/Xuper-TV-NovaQt-GPU-RIFE-Addon/Xuper-TV-NovaQt-Preview/. "$stage/"
cp tests/nova_rife_frames.vpy /ucrt64/bin/mpv.exe "$stage/"
cd "$stage"
/ucrt64/bin/ffmpeg.exe -hide_banner -loglevel error -y -f lavfi \
  -i "testsrc2=size=640x360:rate=24" -t 30 -c:v mpeg4 test.mp4
/usr/bin/timeout --kill-after=5s 40s ./XuperTVNovaQt.exe --smoke-video-fullscreen
for fps in 24 60; do
  /ucrt64/bin/ffmpeg.exe -hide_banner -loglevel error -y \
    -f lavfi -i "testsrc2=size=160x96:rate=$fps" -t 1 -c:v ffv1 "input-$fps.mkv"
done
stdlib=(nova-gpu/python/lib/python3.*)
export PYTHONHOME="$(cygpath -wa nova-gpu/python)"
python_lib="$(cygpath -wa "${stdlib[0]}")"
export PYTHONPATH="$python_lib;$python_lib/lib-dynload;$python_lib/site-packages"
# Explicit registration also works on elevated Windows runners where the loader
# ignores environment overrides. This job runs on a disposable hosted VM.
driver_manifest="$(cygpath -wa /ucrt64/share/vulkan/icd.d/lvp_icd.x86_64.json)"
export VK_DRIVER_FILES="$driver_manifest" VK_ICD_FILENAMES="$driver_manifest"
export VK_LOADER_DEBUG=error,warn
registry_key='HKLM\SOFTWARE\Khronos\Vulkan\Drivers'
MSYS2_ARG_CONV_EXCL='*' reg.exe add "$registry_key" /v "$driver_manifest" /t REG_DWORD /d 0 /f
trap 'MSYS2_ARG_CONV_EXCL="*" reg.exe delete "$registry_key" /v "$driver_manifest" /f >/dev/null' EXIT
/ucrt64/bin/ffmpeg.exe -hide_banner -loglevel error -y -f lavfi \
  -i "testsrc2=size=160x96:rate=24" -t 90 -c:v mpeg4 test.mp4
env -u PYTHONHOME -u PYTHONPATH /usr/bin/timeout --kill-after=5s 100s ./XuperTVNovaQt.exe --smoke-rife-fullscreen
for mode in 60 24 unavailable; do
  fps="$mode"
  rm -f frame-*.txt rife-status.txt
  runtime_path=/c/Windows/System32
  if [[ "$mode" == unavailable ]]; then
    fps=24
    # Simulate an incomplete add-on independently of the runner's actual GPUs.
    mv nova-gpu/librife_windows_x86-64.dll nova-gpu/plugin-disabled.dll
  elif [[ "$mode" == 24 ]]; then
    runtime_path="$runtime_path:/ucrt64/bin"
  fi
  echo "Checking $mode in packaged mpv"
  PATH="$runtime_path" /usr/bin/timeout --kill-after=10s 150s ./mpv.exe \
    --msg-level=vapoursynth=debug --no-config --no-audio --vo=null --frames=5 \
    '--vf=vapoursynth=file=nova_rife_frames.vpy:concurrent-frames=2:user-data=rife-status.txt' "input-$fps.mkv"
  /ucrt64/bin/python.exe - "$mode" <<'PYTEST'
from fractions import Fraction
from pathlib import Path
import sys
mode = sys.argv[1]
frames = [Path(f"frame-{n}.txt").read_text().split() for n in range(5)]
expected_fps = {"24": 48, "60": 60, "unavailable": 24}[mode]
assert all(abs(float(Fraction(t)) - 1 / expected_fps) <= 0.0011 for t, _ in frames), frames
status = Path("rife-status.txt")
if mode == "unavailable":
    assert status.read_bytes() == b"unavailable"
else:
    assert not status.exists()
if mode == "24":
    assert len({digest for _, digest in frames[:3]}) == 3, frames
print(f"{mode} -> {expected_fps} fps: {frames}")
PYTEST
done
