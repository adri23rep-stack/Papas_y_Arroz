"""Offline contract checks for the stock Veyra 2.1.9 Win32 IPC shim.

No real media, Windows GUI, executable, external network or user config needed.
This does NOT prove that a GPU frame is actually decoded.
"""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "tools" / "nova_veyra_win32_proxy.py"
spec = importlib.util.spec_from_file_location("nova_veyra_win32_proxy", script)
assert spec and spec.loader
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class FakeWin32:
    def __init__(self):
        self.calls = []
    def SendMessageTimeoutW(self, hwnd, event, wp, lp, flags, ms, result):
        self.calls.append((hwnd, event, wp))
        return 1

class FakeKernel:
    def __init__(self):
        self.calls = []
    def WritePrivateProfileStringW(self, section, key, value, ini):
        self.calls.append((section, key, value, ini))
        return True


class Win32BridgeContract(unittest.TestCase):
    def test_source_matches_released_win32_message_ids(self):
        self.assertEqual((bridge.PLAY,bridge.STOP,bridge.RECENT,bridge.FULLSCREEN), (102,103,113,120))

    def test_accepts_only_existing_absolute_local_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / "Veyra.exe"
            media = root / "Película prueba.mp4"
            exe.write_bytes(b"fixture")
            media.write_bytes(b"fixture")
            user, kernel = FakeWin32(), FakeKernel()
            got = bridge.open_local_file(kernel,user,100,exe,str(media))
            self.assertEqual(got, media.name)
            self.assertEqual(user.calls,[(100,bridge.WM_COMMAND,bridge.RECENT)])
            self.assertEqual(kernel.calls[0][0:3],("Player","最近打开",str(media.resolve())))
            self.assertEqual(Path(kernel.calls[0][3]), root / "runtime_local" / "veyra.ini")

    def test_rejects_bad_urls_nonexistent_files_and_long_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            user,kernel = FakeWin32(),FakeKernel()
            for value in ("https://example.test/video.m3u8", "../video.mp4",
                          "x" * 9000, "", str(root / "missing.mp4"), None, 123):
                self.assertIsNone(bridge.open_local_file(kernel,user,100,root/"Veyra.exe",value))
            self.assertFalse(user.calls)
            self.assertFalse(kernel.calls)


if __name__ == "__main__":
    unittest.main()
