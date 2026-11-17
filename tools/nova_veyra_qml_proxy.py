#!/usr/bin/env python3
"""Opt-in stock Veyra 2.1.9 QML bridge; NO FFmpeg/NVIDIA build required.

Nova (Qt/QProcess) <-> JSON stdin/stdout <-> this proxy <-localhost HTTP->
a QML-only NovaControl in a SEPARATE COPY of the official Veyra portable.

Requires Python 3.10+ on Windows and an official Veyra runtime with the QML
adapter staged; no app binary patching or remote listener. All URLs stay
off the Veyra process command line. Run ONLY through Nova's pilot.
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes
import http.server
import json
import os
from pathlib import Path
import queue
import secrets
import subprocess
import sys
import threading
import urllib.parse

MAX_COMMAND = 65536
output_lock = threading.Lock()
pending: queue.Queue[dict] = queue.Queue(maxsize=100)
ready = threading.Event()
token = secrets.token_hex(32)


def output(message: dict) -> None:
    """Send only newline-delimited JSON messages to the parent Nova process."""
    with output_lock:
        sys.stdout.write(json.dumps(message, ensure_ascii=True, separators=(",", ":")) + "\n")
        sys.stdout.flush()


def input_worker() -> None:
    try:
        for line in sys.stdin:
            if len(line) > MAX_COMMAND:
                continue
            try:
                command = json.loads(line)
            except ValueError:
                continue
            if not isinstance(command, dict) or command.get("protocol") != 1:
                continue
            if command.get("cmd") not in {
                "open_source", "pause", "stop", "seek_by", "resume",
                "volume", "speed", "audio_track", "subtitle_track",
                "fullscreen", "next_monitor", "gpu_feature",
                "fg_multiplier", "quit",
            }:
                continue
            try:
                pending.put(command, timeout=2)
            except queue.Full:
                output({"protocol": 1, "type": "nova-error"})
                return
    except (OSError, UnicodeError):
        return


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args) -> None:
        # URLs may contain private session tokens; NEVER log request data.
        return

    def authorized(self) -> bool:
        parsed = urllib.parse.urlsplit(self.path)
        supplied = urllib.parse.parse_qs(parsed.query).get("token", [""])[0]
        return secrets.compare_digest(token, supplied)

    def reply(self, code: int, value: dict) -> None:
        data = json.dumps(value, separators=(",", ":"), ensure_ascii=True).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(data)
        except OSError:
            pass
        self.close_connection = True

    def do_OPTIONS(self) -> None:
        self.reply(200 if self.authorized() else 403, {})

    def do_GET(self) -> None:
        if not self.authorized():
            self.reply(403, {})
            return
        if urllib.parse.urlsplit(self.path).path != "/poll":
            self.reply(404, {})
            return
        if not ready.is_set():
            ready.set()
            output({"protocol": 1, "type": "nova-ready"})
        try:
            command = pending.get_nowait()
        except queue.Empty:
            command = {}
        self.reply(200, command)

    def do_POST(self) -> None:
        if not self.authorized():
            self.reply(403, {})
            return
        if urllib.parse.urlsplit(self.path).path != "/status":
            self.reply(404, {})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.reply(400, {})
            return
        if not 0 < length <= MAX_COMMAND:
            self.reply(413, {})
            return
        try:
            message = json.loads(self.rfile.read(length))
            if not isinstance(message, dict) or message.get("protocol") != 1 or message.get("type") != "nova-status":
                self.reply(400, {})
                return
            # Do not forward keys that could hold source URLs, credentials or
            # arbitrary text from a compromised QML client.
            whitelist = {
                "running", "paused", "failed", "ended", "hasSource",
                "position", "duration", "audioTracks", "selectedAudioTrack",
                "subtitleTracks", "selectedSubtitle", "fgEnabled", "fgMultiplier",
                "fgMultiplierChoices", "fgMaxMultiplier", "fgBackend",
                "srEnabled", "nrEnabled", "hdrEnabled", "sourceFps",
                "submitFps", "submitFpsKnown",
            }
            message = {key: value for key, value in message.items() if key in whitelist}
            message.update({"type": "nova-status", "protocol": 1})
            output(message)
            self.reply(200, {})
        except (ValueError, UnicodeError):
            self.reply(400, {})


def job_for_child(child: subprocess.Popen) -> object | None:
    """Close-on-parent-death Windows Job: avoids orphaned GPU processes."""
    if os.name != "nt":
        return None
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel.CreateJobObjectW.restype = ctypes.c_void_p
    kernel.SetInformationJobObject.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    kernel.SetInformationJobObject.restype = wintypes.BOOL
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.AssignProcessToJobObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    kernel.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.CloseHandle.restype = wintypes.BOOL

    class Basic(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64),
                    ("PerJobUserTimeLimit", ctypes.c_int64),
                    ("LimitFlags", wintypes.DWORD),
                    ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t),
                    ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t),
                    ("PriorityClass", wintypes.DWORD),
                    ("SchedulingClass", wintypes.DWORD)]

    class Io(ctypes.Structure):
        _fields_ = [(key, ctypes.c_uint64) for key in (
            "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
            "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class Extended(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", Basic), ("IoInfo", Io),
                    ("ProcessMemoryLimit", ctypes.c_size_t),
                    ("JobMemoryLimit", ctypes.c_size_t),
                    ("PeakProcessMemoryUsed", ctypes.c_size_t),
                    ("PeakJobMemoryUsed", ctypes.c_size_t)]

    handle = kernel.CreateJobObjectW(None, None)
    if not handle:
        raise OSError("Could not create kill-on-close job")
    limits = Extended()
    limits.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
    if not kernel.SetInformationJobObject(handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
        kernel.CloseHandle(handle)
        raise OSError("Could not set kill-on-close job")
    process = kernel.OpenProcess(0x0101, False, child.pid)  # SET_QUOTA | TERMINATE
    if not process:
        kernel.CloseHandle(handle)
        raise OSError("Could not open Veyra child process for job")
    assigned = kernel.AssignProcessToJobObject(handle, process)
    kernel.CloseHandle(process)
    if not assigned:
        kernel.CloseHandle(handle)
        raise OSError("Could not attach Veyra process to kill-on-close job")
    return (kernel, handle)


def main() -> int:
    exe = Path(os.environ.get("NOVA_VEYRA_QML_ORIGINAL_EXE", "")).resolve()
    qml = Path(os.environ.get("NOVA_VEYRA_QML_ROOT", "")).resolve()
    config = qml / "NovaBridgeConfig.js"
    if not exe.is_file() or not config.is_file() or not (qml / "NovaControl.qml").is_file():
        output({"protocol": 1, "type": "nova-error"})
        return 2
    if qml.resolve() != (exe.parent / "qml" / "Veyra").resolve():
        output({"protocol": 1, "type": "nova-error"})
        return 3

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.timeout = 0.25
    port = server.server_address[1]
    temp = config.with_suffix(".nova.tmp")
    temp.write_text(
        "var endpoint = " + json.dumps("http://127.0.0.1:" + str(port)) + "\n"
        "var token = " + json.dumps(token) + "\n", encoding="utf-8")
    os.replace(temp, config)
    child = None
    job = None
    try:
        env = os.environ.copy()
        env["QML_DISABLE_DISK_CACHE"] = "1"  # per-session JS settings must reload
        for key in ("NOVA_VEYRA_QML_ORIGINAL_EXE", "NOVA_VEYRA_QML_ROOT"):
            env.pop(key, None)
        # Separate Nova pilot settings from the user's installed Veyra,
        # and prevent the stock app from auto-resuming an unrelated last video.
        data_dir = exe.parent / "NovaPilotData"
        data_dir.mkdir(parents=True, exist_ok=True)
        child = subprocess.Popen([str(exe), "--data-dir", str(data_dir)],
                                 cwd=str(exe.parent), env=env,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        job = job_for_child(child)
        threading.Thread(target=input_worker, daemon=True).start()
        while child.poll() is None:
            server.handle_request()
        return 0 if child.returncode == 0 else 4
    except (OSError, ValueError):
        output({"protocol": 1, "type": "nova-error"})
        return 5
    finally:
        server.server_close()
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
        if job is not None:
            kernel, handle = job
            kernel.CloseHandle(handle)
        # Leave the QML source valid but inert if the next user starts it alone.
        try:
            config.write_text('var endpoint = ""\nvar token = ""\n', encoding="utf-8")
        except OSError:
            pass


if __name__ == "__main__":
    sys.exit(main())
