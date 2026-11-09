#!/usr/bin/env python3
"""One-time, offline setup for an APK the user already owns.

The APK and extracted configuration never leave the user's PC and must never
be included in a public release, an issue, or a GitHub Actions artifact.
"""
import json
import os
import shutil
import sys
from pathlib import Path

# PyInstaller --windowed sets stdout and stderr to None on Windows.
# Androguard/Loguru may write to these streams during import, before the UI
# is shown. Give them valid, local, non-network output streams instead.
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w", encoding="utf-8")
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w", encoding="utf-8")

import tkinter as tk
from tkinter import filedialog, messagebox

from configure_from_apk import configure


def main():
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    folder = (
        Path(sys.executable).resolve().parent
        if getattr(sys, "frozen", False)
        else Path(__file__).resolve().parent.parent
    )
    config = folder / "service.json"
    state = folder / "device_state.json"

    if state.exists() and not config.is_file():
        messagebox.showerror(
            "Xuper TV Desktop",
            "Se encontro una activacion anterior sin service.json. "
            "Restaura ese archivo primero para conservar tu identidad.",
            parent=root,
        )
        return 2

    if config.is_file() and not messagebox.askyesno(
        "Xuper TV Desktop",
        "Ya existe la configuracion del servicio. "
        "Deseas actualizarla desde tu APK? "
        "Se conservara la identidad de esta instalacion.",
        parent=root,
    ):
        return 0

    apk = filedialog.askopenfilename(
        parent=root,
        title="Selecciona tu APK de Xuper TV",
        filetypes=[("Paquete Android", "*.apk")],
    )
    if not apk:
        return 1

    pending = folder / ".service.json.pending"
    if pending.exists():
        messagebox.showerror(
            "Xuper TV Desktop",
            "Hay una configuracion temporal previa. Cierra el programa "
            "y elimina .service.json.pending antes de reintentar.",
            parent=root,
        )
        return 2

    try:
        apk_file = Path(apk)
        if apk_file.stat().st_size > 300 * 1024 * 1024:
            raise ValueError("El APK supera el limite de 300 MB.")
        if config.is_file():
            shutil.copyfile(config, pending)
        # Local parsing only: no requests to the service are made here.
        configure(apk_file, pending, platform="windows")
        local = json.loads(pending.read_text(encoding="utf-8"))
        for key in ("portal_hosts", "key_base64", "password_salt", "app_id",
                    "installation_serial", "device_id"):
            if not local.get(key):
                raise ValueError("El APK no contiene una configuracion compatible.")
        if local.get("ca_file") != "cacert.pem":
            raise ValueError("La ruta de certificados de Windows es incorrecta.")
        pending.replace(config)
    except Exception as exc:
        try:
            pending.unlink(missing_ok=True)
        except OSError:
            pass
        messagebox.showerror(
            "Xuper TV Desktop",
            "No se pudo importar este APK. No se modifico la configuracion "
            "actual.\n\n" + str(exc),
            parent=root,
        )
        return 2

    messagebox.showinfo(
        "Xuper TV Desktop",
        "Configuracion guardada solo en esta PC. "
        "Xuper TV intentara cargar el catalogo al iniciar. "
        "El servidor puede exigir permisos adicionales. "
        "No compartas service.json.",
        parent=root,
    )
    return 0


if __name__ == "__main__":
    # CI can launch the real frozen, windowless EXE to catch import crashes
    # without opening the APK selection dialogue.
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test-imports":
        raise SystemExit(0)
    raise SystemExit(main())
