# Nova MotionBoost GPU RIFE — loaded only when the optional Vulkan plugin
# and its model are present. This is a STREAMED VapourSynth filter for mpv:
# do NOT open video URLs, files or invoke a second player here.
import os
from fractions import Fraction
import vapoursynth as vs

core=vs.core
script_dir=os.path.dirname(os.path.realpath(__file__))
core.std.LoadPlugin(path=os.path.join(script_dir, "librife_windows_x86-64.dll"))

# mpv supplies video_in and container_fps. Keep video_in as the only source,
# otherwise video packets can queue indefinitely while only audio advances.
clip=video_in
source_fps=float(globals().get("container_fps", 0) or 0)
if not (0 < source_fps <= 30.0):
    source_fps=float(clip.fps.numerator)/clip.fps.denominator if clip.fps.denominator else 24.0
if not (0 < source_fps <= 30.0):
    # Do not attempt expensive interpolation on already high-FPS sources.
    clip.set_output()
else:
    fps=Fraction(source_fps).limit_denominator(1001)
    # Color conversion for the ncnn plugin (requires RGB float32).
    # Keep scene properties to avoid inventing frames across hard cuts.
    matrix="709" if clip.width>=1280 or clip.height>=720 else "601"
    if clip.format.color_family == vs.YUV:
        gray=core.std.ShufflePlanes(clip,planes=0,colorfamily=vs.GRAY)
        rgb=core.resize.Bicubic(clip,format=vs.RGBS,matrix_in_s=matrix,range_in_s="limited")
    else:
        gray=core.resize.Bicubic(clip,format=vs.GRAY8,matrix_in_s=matrix)
        rgb=core.resize.Bicubic(clip,format=vs.RGBS)
    scene=core.misc.SCDetect(gray,threshold=0.12)
    rgb=core.std.CopyFrameProps(rgb,scene,["_SceneChangePrev","_SceneChangeNext"])
    # Model 23 = RIFE 4.6 (ensemble=False). x2 by design: 24 -> 48,
    # 30 -> 60; 60fps is not guaranteed if the RTX cannot keep up.
    rgb=core.rife.RIFE(rgb,model=23,factor_num=2,factor_den=1,
                       gpu_id=0,gpu_thread=1,sc=True)
    rgb=core.std.AssumeFPS(rgb,fpsnum=fps.numerator*2,fpsden=fps.denominator)
    # mpv's streamed filter requires accurate per-frame durations after x2.
    rgb=core.std.SetFrameProps(rgb,_DurationNum=fps.denominator,
                              _DurationDen=fps.numerator*2)
    clip=core.resize.Bicubic(rgb,format=vs.YUV420P8,matrix_s=matrix,range_s="limited")
    clip.set_output()
