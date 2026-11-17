#!/usr/bin/env python3
"""Experimental NOVA <-> official Veyra 2.1.9 Win32 proof of concept.

Only works with an isolated COPY created by Stage-NovaVeyraWin32.ps1.
Controls supported: open LOCAL media, toggle pause, fullscreen, stop/quit.
No streaming, remote subtitles, GPU changes, seek, resume, or duration.
No third-party Python dependencies. Never run against an installed Veyra.
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time

# Confirmed in user's Veyra 2.1.9 source:
# apps/veyra/ui/AppShell.cpp => enum Open=101, Play=102, Stop=103,
# Recent=113, Fullscreen=120. case Recent opens localDataDirectory()/veyra.ini.
# src/base/RuntimePaths.cpp => localDataDirectory() = exe.parent/runtime_local.
WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
PLAY = 102
STOP = 103
RECENT = 113
FULLSCREEN = 120
SMTO_ABORTIFHUNG = 0x0002

stdout_lock = threading.Lock()
commands: queue.Queue[dict] = queue.Queue(maxsize=64)
parent_closed = threading.Event()

def report(data: dict) -> None:
    with stdout_lock:
        sys.stdout.write(json.dumps(data, separators=(",", ":"), ensure_ascii=True) + "\n")
        sys.stdout.flush()


def listen() -> None:
    try:
        for raw in sys.stdin.buffer:
            if len(raw) > 65536:
                continue
            try:
                obj = json.loads(raw.decode("utf-8", "strict"))
            except (ValueError, UnicodeError):
                continue
            if isinstance(obj, dict) and obj.get("protocol") == 1:
                try:
                    commands.put(obj, timeout=1)
                except queue.Full:
                    report({"protocol": 1, "type": "nova-error"})
                    break
    except OSError:
        pass
    finally:
        parent_closed.set()


def win32_api():
    user = ctypes.WinDLL("user32", use_last_error=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    enum_proc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = (enum_proc, wintypes.LPARAM)
    user.EnumWindows.restype = wintypes.BOOL
    user.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user.GetWindowThreadProcessId.restype = wintypes.DWORD
    user.IsWindowVisible.argtypes = (wintypes.HWND,)
    user.IsWindowVisible.restype = wintypes.BOOL
    user.IsWindow.argtypes = (wintypes.HWND,)
    user.IsWindow.restype = wintypes.BOOL
    user.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user.GetWindowTextW.restype = ctypes.c_int
    user.SendMessageTimeoutW.argtypes = (
        wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t,
        wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t))
    user.SendMessageTimeoutW.restype = ctypes.c_size_t
    kernel.WritePrivateProfileStringW.argtypes = (
        wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.LPCWSTR)
    kernel.WritePrivateProfileStringW.restype = wintypes.BOOL
    return user, kernel, enum_proc


def window_title(user, hwnd) -> str:
    buffer = ctypes.create_unicode_buffer(1024)
    user.GetWindowTextW(hwnd, buffer, len(buffer))
    return buffer.value


def find_main_window(user, enum_proc, pid: int) -> int:
    matched = []
    @enum_proc
    def callback(hwnd, _):
        child_pid = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(child_pid))
        if child_pid.value == pid and user.IsWindowVisible(hwnd):
            title = window_title(user, hwnd)
            if title.startswith("Veyra"):
                matched.append(hwnd)
        return True
    user.EnumWindows(callback, 0)
    return matched[0] if matched else 0


def send_command(user, hwnd: int, command: int) -> bool:
    result = ctypes.c_size_t()
    sent = user.SendMessageTimeoutW(
        hwnd, WM_COMMAND, command, 0, SMTO_ABORTIFHUNG, 1200, ctypes.byref(result))
    return bool(sent)


def open_local_file(kernel, user, hwnd: int, exe: Path, value: object) -> str | None:
    if not isinstance(value, str) or len(value) > 8192:
        return None
    source = Path(value)
    if not source.is_absolute() or not source.is_file():
        return None
    path = str(source.resolve())
    local_dir = exe.parent / "runtime_local"
    local_dir.mkdir(parents=True, exist_ok=True)
    settings = local_dir / "veyra.ini"
    # This path is in the isolated portable, NEVER the installed program.
    ok = kernel.WritePrivateProfileStringW(
        "Player", "最近打开", path, str(settings))
    if not ok or not send_command(user, hwnd, RECENT):
        return None
    return source.name


def main() -> int:
    if os.name != "nt":
        report({"protocol": 1, "type": "nova-error"})
        return 2
    exe_value = os.environ.get("NOVA_VEYRA_WIN32_EXE", "")
    if not exe_value:
        report({"protocol": 1, "type": "nova-error"})
        return 2
    exe = Path(exe_value).resolve()
    marker = exe.parent / "NovaWin32Pilot.marker"
    if exe.name.casefold() != "veyra.exe" or not exe.is_file() or not marker.is_file():
        report({"protocol": 1, "type": "nova-error"})
        return 2
    user, kernel, enum_proc = win32_api()
    proc = None
    job = None
    try:
        # Keep the official Win32 engine and runtime intact; no URL on argv.
        proc = subprocess.Popen([str(exe)], cwd=str(exe.parent),
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # Assign child to a Windows kill-on-close job, so parent death cannot
        # leave a rogue GPU process consuming resources.
        from nova_veyra_qml_proxy import job_for_child
        job = job_for_child(proc)
        hwnd = 0
        for _ in range(70):
            if proc.poll() is not None:
                report({"protocol": 1, "type": "nova-error"})
                return 4
            hwnd = find_main_window(user, enum_proc, proc.pid)
            if hwnd:
                break
            time.sleep(0.1)
        if not hwnd:
            report({"protocol": 1, "type": "nova-error"})
            return 4
        threading.Thread(target=listen, daemon=True).start()
        report({"protocol": 1, "type": "nova-ready"})
        target_basename = ""
        last_status = 0.0
        while proc.poll() is None and not parent_closed.is_set():
            try:
                msg = commands.get(timeout=0.1)
            except queue.Empty:
                msg = {}
            cmd = msg.get("cmd")
            if cmd == "quit":
                break
            if cmd == "open_source":
                name = open_local_file(kernel, user, hwnd, exe, msg.get("source"))
                if not name:
                    report({"protocol": 1, "type": "nova-error"})
                    break
                target_basename = name
            elif cmd == "pause":
                send_command(user, hwnd, PLAY)
            elif cmd == "stop":
                send_command(user, hwnd, STOP)
            elif cmd == "fullscreen":
                send_command(user, hwnd, FULLSCREEN)
            if not user.IsWindow(hwnd):
                report({"protocol": 1, "type": "nova-error"})
                break
            now = time.monotonic()
            if now - last_status >= 0.5:
                last_status = now
                # This only confirms the window accepted the file. It is NOT
                # proof that decoding/FG/NR succeeded. Do not report fake FPS.
                loaded = bool(target_basename and target_basename in window_title(user, hwnd))
                report({"protocol": 1, "type": "nova-status",
                        "experimentalWin32": True, "running": loaded,
                        "hasSource": loaded, "paused": False, "failed": False,
                        "position": 0, "duration": 0})
        if proc.poll() is not None and proc.returncode:
            report({"protocol": 1, "type": "nova-error"})
            return 4
        return 0
    except (OSError, ValueError):
        report({"protocol": 1, "type": "nova-error"})
        return 5
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
        if job is not None:
            kernel_handle, handle = job
            kernel_handle.CloseHandle(handle)


if __name__ == "__main__":
    sys.exit(main())
