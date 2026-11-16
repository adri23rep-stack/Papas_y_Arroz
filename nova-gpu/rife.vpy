import os
import vapoursynth as vs

clip = video_in
fps = float(container_fps or clip.fps or 0)
if 0 < fps <= 30:
    core = vs.core
    folder = os.path.dirname(os.path.abspath(__file__))
    core.std.LoadPlugin(path=os.path.join(folder, "librife_windows_x86-64.dll"))
    model = os.path.join(folder, "models", "rife-v4.6_ensembleFalse")

    # mpv's VapourSynth bridge supplies planar YUV. Use R65's matrix names.
    matrix = "709" if clip.width >= 1280 or clip.height >= 720 else "170m"
    rgb = core.resize.Bicubic(clip, format=vs.PresetVideoFormat.RGBS, matrix_in_s=matrix)
    luma = core.std.ShufflePlanes(clip, planes=0, colorfamily=vs.ColorFamily.GRAY)
    small = core.resize.Bilinear(luma, width=160, height=90, format=vs.PresetVideoFormat.GRAY8)
    difference = core.std.PlaneStats(small, small[1:] + small[-1])

    def mark_cut(n, f):
        frame = f[0].copy()
        frame.props["_SceneChangeNext"] = int(f[1].props["PlaneStatsDiff"] > 0.12)
        return frame

    rgb = core.std.ModifyFrame(rgb, clips=[rgb, difference], selector=mark_cut)
    # The plugin halves each source frame's actual mpv duration, including VFR.
    doubled = core.rife.RIFE(rgb, model=23, model_path=model, factor_num=2,
                             factor_den=1, gpu_thread=1, sc=True)
    clip = core.resize.Bicubic(doubled, format=clip.format.id, matrix_s=matrix)

clip.set_output()
