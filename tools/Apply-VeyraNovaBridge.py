#!/usr/bin/env python3
"""Install, check or remove Nova's optional bridge in a COPY of Veyra's sources.

Usage:
  python tools/Apply-VeyraNovaBridge.py --check "C:\Veyra-NRVideo-main"
  python tools/Apply-VeyraNovaBridge.py "C:\Veyra-NRVideo-main"
  python tools/Apply-VeyraNovaBridge.py --restore "C:\Veyra-NRVideo-main"

No downloads, no changes to installed Veyra, Nova main, or other repositories.
Requires the Veyra Qt QML source tree from the user's source ZIP.
"""
from pathlib import Path
import argparse
import os
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ANCHOR_INCLUDE = '#include "veyra/ui/QmlPlayerBridge.h"'
NEW_INCLUDE = '#include "NovaStdioBridge.hpp"'
ANCHOR_INIT = '    bridge.attachMainWindow(static_cast<qulonglong>(window->winId()));'
NEW_INIT = '    NovaStdioBridge novaStdioBridge(bridge, window, &app);'
AUTO_RESUME = 'QTimer::singleShot(600, &bridge, [&bridge] { bridge.autoResumeLastSource(); });'
GUARDED_RESUME = 'if (qEnvironmentVariableIntValue("NOVA_VEYRA_STDIO") != 1) ' + AUTO_RESUME


def atomic_text_write(path: Path, data: str) -> None:
    # A failure cannot leave a half-written Veyra entrypoint.
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix="nova_bridge_", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as out:
            out.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def patch(target: Path, *, check_only: bool = False, restore: bool = False) -> str:
    main_cpp = target / "apps/veyra-qml/main.cpp"
    header = target / "apps/veyra-qml/NovaStdioBridge.hpp"
    backup = main_cpp.with_suffix(".cpp.nova-pre-bridge.bak")
    source_header = ROOT / "integrations/veyra/NovaStdioBridge.hpp"
    if not main_cpp.is_file() or not source_header.is_file():
        raise ValueError("Missing Veyra Qt main.cpp or Nova adapter source. No changes made.")
    data = main_cpp.read_text(encoding="utf-8")
    patched = NEW_INCLUDE in data and NEW_INIT in data and GUARDED_RESUME in data

    if restore:
        if not backup.is_file():
            raise ValueError("No Nova bridge backup exists. Nothing restored.")
        if not patched:
            raise ValueError("The Veyra entrypoint has changed. Refusing to overwrite it.")
        if check_only:
            return "Bridge can be restored."
        # Preserve another person's changes in NovaStdioBridge.hpp if it diverged.
        original = backup.read_text(encoding="utf-8")
        atomic_text_write(main_cpp, original)
        backup.unlink()
        if header.is_file() and header.read_bytes() == source_header.read_bytes():
            header.unlink()
        return "Original Veyra main.cpp restored. Check adapter header if it was modified."

    if not patched:
        if NEW_INCLUDE in data or NEW_INIT in data or GUARDED_RESUME in data:
            raise ValueError("Partial or modified Nova bridge present. Inspect main.cpp manually.")
        if backup.exists():
            raise ValueError("A prior Nova backup exists. Inspect it before applying the bridge.")
        if any(data.count(anchor) != 1 for anchor in (ANCHOR_INCLUDE, ANCHOR_INIT, AUTO_RESUME)):
            raise ValueError("Veyra injection points do not match the reviewed source ZIP.")
        patched_source = data.replace(ANCHOR_INCLUDE, ANCHOR_INCLUDE + "\n" + NEW_INCLUDE, 1)
        patched_source = patched_source.replace(ANCHOR_INIT, ANCHOR_INIT + "\n" + NEW_INIT, 1)
        patched_source = patched_source.replace(AUTO_RESUME, GUARDED_RESUME, 1)
    else:
        patched_source = data
        if not backup.exists():
            raise ValueError("Patched Veyra has no original backup; refusing to claim success.")

    if check_only:
        return "Bridge already installed." if patched else "Injection anchors valid; ready to install."
    if not patched:
        shutil.copy2(main_cpp, backup)
        try:
            atomic_text_write(main_cpp, patched_source)
        except Exception:
            backup.unlink(missing_ok=True)
            raise
    shutil.copy2(source_header, header)
    return "Nova bridge installed. Compile Veyra's veyra_qml_ui.exe from this separate source copy."


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("veyra_source", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="Only verify whether the patch can be installed.")
    mode.add_argument("--restore", action="store_true", help="Restore original main.cpp from the Nova backup.")
    args = parser.parse_args()
    try:
        print(patch(args.veyra_source.resolve(), check_only=args.check, restore=args.restore))
    except (OSError, ValueError) as exc:
        sys.exit(f"Nova Veyra bridge: {exc}")


if __name__ == "__main__":
    main()
