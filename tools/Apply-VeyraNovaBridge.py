#!/usr/bin/env python3
r"""Install, check or remove Nova's optional bridge in a COPY of Veyra's sources.

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


TRANSPORT_ANCHOR = "void QmlPlayerBridge::tickPlaylist() {\n"
TRANSPORT_GUARD = """    // Nova owns episode progression; expose exact EOF without advancing Veyra's playlist.
    if (qEnvironmentVariableIntValue("NOVA_VEYRA_STDIO") == 1) {
        setProperty("_novaPlaybackEnded", impl_->snapshot.transport == engine::TransportState::Ended);
        return;
    }
"""


def patched_main(data: str) -> str:
    return (data.replace(ANCHOR_INCLUDE, ANCHOR_INCLUDE + "\n" + NEW_INCLUDE, 1)
            .replace(ANCHOR_INIT, ANCHOR_INIT + "\n" + NEW_INIT, 1)
            .replace(AUTO_RESUME, GUARDED_RESUME, 1))


def patch(target: Path, *, check_only: bool = False, restore: bool = False) -> str:
    main_cpp = target / "apps/veyra-qml/main.cpp"
    transport = target / "src/ui/bridge/Playback.cpp"
    header = target / "apps/veyra-qml/NovaStdioBridge.hpp"
    source_header = ROOT / "integrations/veyra/NovaStdioBridge.hpp"
    if not all(path.is_file() for path in (main_cpp, transport, source_header)):
        raise ValueError("Missing Veyra Qt sources or Nova adapter source. No changes made.")
    specs = [
        (main_cpp, (ANCHOR_INCLUDE, ANCHOR_INIT, AUTO_RESUME),
         (NEW_INCLUDE, NEW_INIT, GUARDED_RESUME), patched_main),
        (transport, (TRANSPORT_ANCHOR,), (TRANSPORT_GUARD,),
         lambda data: data.replace(TRANSPORT_ANCHOR, TRANSPORT_ANCHOR + TRANSPORT_GUARD, 1)),
    ]
    # Validate every file before writing any of them. Earlier main-only installs
    # are upgraded without replacing their original backup.
    changes = []
    for path, anchors, markers, transform in specs:
        data = path.read_text(encoding="utf-8")
        backup = path.with_suffix(".cpp.nova-pre-bridge.bak")
        installed = all(marker in data for marker in markers)
        if installed:
            if not backup.is_file():
                raise ValueError("Patched Veyra has no original backup.")
            original = backup.read_text(encoding="utf-8")
            if transform(original) != data:
                raise ValueError("The Veyra entrypoint has changed since installation. Nothing restored.")
        else:
            if any(marker in data for marker in markers):
                raise ValueError("Partial or modified Nova bridge present.")
            if backup.exists():
                raise ValueError("A prior Nova backup exists. Inspect it before applying the bridge.")
            if any(data.count(anchor) != 1 for anchor in anchors):
                raise ValueError("Veyra injection points do not match the reviewed source ZIP.")
            original = data
        if restore and not installed:
            if path == main_cpp:
                raise ValueError("No Nova bridge backup exists. Nothing restored.")
            continue # legacy main-only install
        changes.append((path, backup, installed, original, transform(original)))
    if check_only:
        if restore:
            return "Bridge can be restored."
        return "Bridge already installed." if all(c[2] for c in changes) else "Injection anchors valid; ready to install."

    adapter = source_header.read_text(encoding="utf-8")
    # Keep pre-operation bytes for rollback; never restore a user's later edits.
    paths = [header] + [p for c in changes for p in c[:2]]
    before = {path: path.read_bytes() if path.exists() else None for path in paths}
    try:
        if not restore:
            atomic_text_write(header, adapter)
        for path, backup, installed, original, modified in changes:
            if restore:
                atomic_text_write(path, original)
                backup.unlink()
            elif not installed:
                shutil.copy2(path, backup)
                atomic_text_write(path, modified)
        if restore and header.is_file() and header.read_bytes() == source_header.read_bytes():
            header.unlink()
    except Exception:
        for path, content in before.items():
            if content is None:
                path.unlink(missing_ok=True)
            elif not path.exists() or path.read_bytes() != content:
                # The rollback must also work if a mocked/failed text writer caused
                # the original exception. These are byte-identical saved originals.
                path.write_bytes(content)
        raise
    if restore:
        return "Original Veyra sources restored. Check adapter header if it was modified."
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
