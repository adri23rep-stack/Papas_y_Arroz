import os
import vapoursynth as vs


def mark_cut(n, f):
    frame = f[0].copy()
    difference = f[1].props["PlaneStatsDiff"]
    frame.props["_SceneChangeNext"] = int(difference == 0 or difference > 0.12)
    return frame


def interpolate(clip, max_width):
    core = vs.core
    folder = os.path.dirname(os.path.abspath(__file__))
    core.std.LoadPlugin(path=os.path.join(folder, "librife_windows_x86-64.dll"))
    model = os.path.join(folder, "models", "rife-v4.6_ensembleFalse")

    # mpv's VapourSynth bridge supplies planar YUV. Use R65's matrix names.
    matrix = "709" if clip.width >= 1280 or clip.height >= 720 else "170m"
    work = clip
    if max_width and clip.width > max_width:
        height = max(2, round(clip.height * max_width / clip.width / 2) * 2)
        work = core.resize.Bilinear(clip, width=max_width, height=height)
    rgb = core.resize.Bicubic(work, format=vs.PresetVideoFormat.RGBS, matrix_in_s=matrix)
    luma = core.std.ShufflePlanes(work, planes=0, colorfamily=vs.ColorFamily.GRAY)
    small = core.resize.Bilinear(luma, width=160, height=90, format=vs.PresetVideoFormat.GRAY8)
    difference = core.std.PlaneStats(small, small[1:] + small[-1])
    rgb = core.std.ModifyFrame(rgb, clips=[rgb, difference], selector=mark_cut)
    # The plugin halves each source frame's actual mpv duration, including VFR.
    doubled = core.rife.RIFE(rgb, model=23, model_path=model, factor_num=2,
                             factor_den=1, gpu_thread=1, sc=True)
    if work is clip:
        return core.resize.Bicubic(doubled, format=clip.format.id, matrix_s=matrix)
    # Only synthesized odd frames travel through the reduced-resolution model.
    # SelectEvery and Interleave adjust frame durations, including fractional FPS.
    odd = core.std.SelectEvery(doubled, cycle=2, offsets=1)
    odd = core.resize.Bicubic(odd, width=clip.width, height=clip.height,
                              format=clip.format.id, matrix_s=matrix)
    return core.std.Interleave([clip, odd])


def set_output(clip, fps, status_path):
    if 0 < fps <= 30:
        try:
            mode = os.path.basename(os.fsdecode(status_path or ""))
            width = {"rife-balanced": 640, "rife-fast": 384}.get(mode, 0)
            clip = interpolate(clip, width)
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
