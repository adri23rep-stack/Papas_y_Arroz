#!/usr/bin/env python3
"""Patch a LOCAL copy of the user's Veyra sources to accept Nova stdio controls.

No downloads, no network calls, no changes to Nova main or any GitHub branch.
Run with: python tools/Apply-VeyraNovaBridge.py C:\path\to\Veyra-NRVideo-main
"""
from pathlib import Path
import argparse
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
ANCHOR_INCLUDE = '#include "veyra/ui/QmlPlayerBridge.h"'
NEW_INCLUDE = '#include "NovaStdioBridge.hpp"'
ANCHOR_INIT = "    bridge.attachMainWindow(static_cast<qulonglong>(window->winId()));"
NEW_INIT = "    NovaStdioBridge novaStdioBridge(bridge, window, &app);"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("veyra_source", type=Path, help="Veyra source directory (must contain apps/veyra-qml/main.cpp)")
    args = parser.parse_args()
    target = args.veyra_source.resolve()
    main_cpp = target / "apps/veyra-qml/main.cpp"
    # The QML entrypoint is required; the legacy Veyra GUI has another main.cpp.
    header = target / "apps/veyra-qml/NovaStdioBridge.hpp"
    source_header = ROOT / "integrations/veyra/NovaStdioBridge.hpp"

    if not main_cpp.is_file() or not source_header.is_file():
        sys.exit("Cannot find Veyra main.cpp or NovaStdioBridge.hpp. No changes made.")
    data = main_cpp.read_text(encoding="utf-8")
    if ANCHOR_INCLUDE not in data or ANCHOR_INIT not in data:
        sys.exit("This Veyra revision has different injection points. No changes made.")
    if data.count(ANCHOR_INCLUDE) != 1 or data.count(ANCHOR_INIT) != 1:
        sys.exit("Unexpected duplicate injection points. No changes made.")

    # Keep the original source for a one-click rollback of this pilot.
    backup = main_cpp.with_suffix(".cpp.nova-pre-bridge.bak")
    if NEW_INIT not in data:
        if backup.exists():
            sys.exit(f"Backup already exists: {backup}; inspect before retrying.")
        shutil.copy2(main_cpp, backup)
        data = data.replace(ANCHOR_INCLUDE, ANCHOR_INCLUDE + "\n" + NEW_INCLUDE, 1)
        data = data.replace(ANCHOR_INIT, ANCHOR_INIT + "\n" + NEW_INIT, 1)
        # The IPC child starts with no positional URL. Do not auto-reopen a previous
        # file in the patched Veyra while Nova is waiting to send open_source.
        old_auto = 'QTimer::singleShot(600, &bridge, [&bridge] { bridge.autoResumeLastSource(); });'
        new_auto = 'if (qEnvironmentVariableIntValue("NOVA_VEYRA_STDIO") != 1) ' + old_auto
        if data.count(old_auto) != 1:
            sys.exit("Could not locate Veyra auto-resume hook; inspect original source.")
        data = data.replace(old_auto, new_auto, 1)
        main_cpp.write_text(data, encoding="utf-8")
    elif NEW_INCLUDE not in data:
        sys.exit("Incomplete previous modification; inspect Veyra main.cpp.")
    shutil.copy2(source_header, header)
    print(f"Installed Nova stdio bridge in {target}")
    print("Now compile Veyra's Qt QML executable, veyra_qml_ui.exe, with its normal CMake build.")
    print("Do not expect the already-installed Veyra binary to include this patch.")

if __name__ == "__main__":
    main()
