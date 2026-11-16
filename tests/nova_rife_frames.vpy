"""Exercise the shipped .vpy inside mpv and record actual decoded output frames."""
import hashlib
import os
import runpy
import vapoursynth as vs

runpy.run_path(os.path.join(os.path.dirname(__file__), "nova-gpu", "rife.vpy"),
               init_globals={"video_in": video_in, "container_fps": container_fps})
output = vs.get_output().clip

def record(n, f):
    if n < 5:
        with open("frame-%d.txt" % n, "w") as report:
            report.write("%d/%d %s" % (
                f.props["_DurationNum"], f.props["_DurationDen"],
                hashlib.sha256(bytes(f[0])).hexdigest()))
    return f

vs.core.std.ModifyFrame(output, clips=output, selector=record).set_output()
