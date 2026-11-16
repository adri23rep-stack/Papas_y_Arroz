# VapourSynth smoke test: no GPU/plugin dependency.
# Used by the Windows CI runner to verify the mpv -> VapourSynth bridge.
import vapoursynth as vs
video_in.set_output()
