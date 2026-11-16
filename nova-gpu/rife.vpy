import os
import vapoursynth as vs


def mark_cut(n, f):
    frame = f[0].copy()
    frame.props["_SceneChangeNext"] = int(f[1].props["PlaneStatsDiff"] > 0.12)
    return frame


def interpolate(clip):
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
    rgb = core.std.ModifyFrame(rgb, clips=[rgb, difference], selector=mark_cut)
    # The plugin halves each source frame's actual mpv duration, including VFR.
    doubled = core.rife.RIFE(rgb, model=23, model_path=model, factor_num=2,
                             factor_den=1, gpu_thread=1, sc=True)
    return core.resize.Bicubic(doubled, format=clip.format.id, matrix_s=matrix)


def set_output(clip, fps, status_path):
    if 0 < fps <= 30:
        try:
            clip = interpolate(clip)
        except (vs.Error, OSError):
            # Keep the input playable even if Vulkan cannot create its device.
            # Nova owns this temporary file and resets the selection on failure.
            if status_path:
                with open(status_path, "wb") as status:
                    status.write(b"unavailable")
    clip.set_output()


set_output(video_in, float(container_fps or video_in.fps or 0), globals().get("user_data"))
# A Python frame callback holds its module globals. Release the input there so
# mpv can destroy the streamed source on seek, toggle and shutdown.
del video_in
