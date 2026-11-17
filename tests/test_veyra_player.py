"""Nova's real player with a deterministic child; rendering is not under test."""
import os
from pathlib import Path
import subprocess
import sys

nova, fixture = (str(Path(p).resolve()) for p in sys.argv[1:3])
for mode in ('controls','failure','crash','unpatched','ended'):
    result=subprocess.run([nova,'--smoke-veyra'],timeout=25,env={**os.environ,
        'NOVA_VEYRA_PILOT':'1','NOVA_VEYRA_EXE':fixture,
        'NOVA_VEYRA_FIXTURE_MODE':mode,'QT_QPA_PLATFORM':'offscreen','QT_QUICK_BACKEND':'software'})
    if result.returncode:
        raise SystemExit(f'Nova IPC scenario {mode} failed: {result.returncode}')
    print(f'Nova IPC scenario {mode}: PASS',flush=True)
