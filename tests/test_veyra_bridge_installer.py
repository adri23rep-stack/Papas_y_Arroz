"""Offline regression checks for the optional Nova -> Veyra source patcher."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tools" / "Apply-VeyraNovaBridge.py"
spec = importlib.util.spec_from_file_location("nova_veyra_patcher", PATCHER)
assert spec and spec.loader
patcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patcher)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.qml = self.root / "apps" / "veyra-qml"
        self.qml.mkdir(parents=True)
        self.entrypoint = self.qml / "main.cpp"
        self.original = (
            '#include "veyra/ui/QmlPlayerBridge.h"\n'
            'void startup() {\n'
            '    bridge.attachMainWindow(static_cast<qulonglong>(window->winId()));\n'
            '    QTimer::singleShot(600, &bridge, [&bridge] { bridge.autoResumeLastSource(); });\n'
            '}\n'
        )
        self.entrypoint.write_text(self.original, encoding="utf-8")

    def test_apply_is_idempotent_and_restorable(self):
        self.assertIn("ready to install", patcher.patch(self.root, check_only=True))
        self.assertEqual(self.entrypoint.read_text(encoding="utf-8"), self.original)
        self.assertIn("installed", patcher.patch(self.root))
        modified = self.entrypoint.read_text(encoding="utf-8")
        self.assertIn(patcher.NEW_INIT, modified)
        self.assertIn(patcher.NEW_INCLUDE, modified)
        self.assertIn(patcher.GUARDED_RESUME, modified)
        self.assertNotEqual(modified, self.original)
        self.assertTrue((self.qml / "NovaStdioBridge.hpp").is_file())
        self.assertTrue(self.entrypoint.with_suffix(".cpp.nova-pre-bridge.bak").is_file())
        self.assertIn("installed", patcher.patch(self.root))
        self.assertEqual(modified, self.entrypoint.read_text(encoding="utf-8"))
        self.assertIn("restored", patcher.patch(self.root, restore=True))
        self.assertEqual(self.original, self.entrypoint.read_text(encoding="utf-8"))
        self.assertFalse(self.entrypoint.with_suffix(".cpp.nova-pre-bridge.bak").exists())

    def test_reject_unknown_entrypoint_without_writing_backup(self):
        self.entrypoint.write_text("#include <QApplication>\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "injection points"):
            patcher.patch(self.root)
        self.assertFalse(self.entrypoint.with_suffix(".cpp.nova-pre-bridge.bak").exists())

    def test_reject_partial_patch_and_do_not_damage_source(self):
        partial = self.original + patcher.NEW_INCLUDE
        self.entrypoint.write_text(partial, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Partial"):
            patcher.patch(self.root)
        self.assertEqual(partial, self.entrypoint.read_text(encoding="utf-8"))

    def test_reject_restore_without_original_backup(self):
        with self.assertRaisesRegex(ValueError, "No Nova bridge backup"):
            patcher.patch(self.root, restore=True)

    def test_restore_preserves_edits_after_install(self):
        patcher.patch(self.root)
        edited=self.entrypoint.read_text(encoding="utf-8") + "// unrelated user change\n"
        self.entrypoint.write_text(edited,encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"changed since installation"):
            patcher.patch(self.root,restore=True)
        self.assertEqual(self.entrypoint.read_text(encoding="utf-8"),edited)

    def test_failed_adapter_write_leaves_original_entrypoint(self):
        with mock.patch.object(patcher,"atomic_text_write",side_effect=OSError("disk full")):
            with self.assertRaises(OSError):patcher.patch(self.root)
        self.assertEqual(self.entrypoint.read_text(encoding="utf-8"),self.original)
        self.assertFalse(self.entrypoint.with_suffix(".cpp.nova-pre-bridge.bak").exists())
        self.assertFalse((self.qml/"NovaStdioBridge.hpp").exists())

    def test_failed_entrypoint_write_rolls_back_adapter(self):
        real_write=patcher.atomic_text_write
        def fail_main(path,data):
            if path==self.entrypoint:raise OSError("disk full")
            real_write(path,data)
        with mock.patch.object(patcher,"atomic_text_write",side_effect=fail_main):
            with self.assertRaises(OSError):patcher.patch(self.root)
        self.assertEqual(self.entrypoint.read_text(encoding="utf-8"),self.original)
        self.assertFalse((self.qml/"NovaStdioBridge.hpp").exists())


if __name__ == "__main__":
    unittest.main()
