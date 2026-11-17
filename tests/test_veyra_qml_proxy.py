"""Offline security/protocol checks for stock Veyra QML loopback bridge.

No GPU, Python third-party packages, credentials or media source required.
"""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import queue
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
import http.server

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nova_veyra_qml_proxy", ROOT / "tools/nova_veyra_qml_proxy.py")
assert spec is not None and spec.loader is not None
proxy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(proxy)


class ProxyHTTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        proxy.token = "unit-test-token-no-media"
        proxy.pending = queue.Queue(maxsize=100)
        proxy.ready.clear()
        cls.events = []
        cls.original_output = proxy.output
        proxy.output = lambda message: cls.events.append(message)
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), proxy.Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = "http://127.0.0.1:%d" % cls.server.server_port

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=4)
        proxy.output = cls.original_output

    def test_rejects_bad_token(self):
        proxy.ready.clear()  # tests are independent of unittest method ordering
        with self.assertRaises(HTTPError) as context:
            urlopen(self.base + "/poll?token=wrong", timeout=3)
        self.assertEqual(context.exception.code, 403)
        self.assertFalse(proxy.ready.is_set())

    def test_handshake_empty_and_ordered_queue(self):
        token = "?token=unit-test-token-no-media"
        with urlopen(self.base + "/poll" + token, timeout=3) as response:
            self.assertEqual(json.loads(response.read()), {})
        self.assertTrue(proxy.ready.is_set())
        self.assertEqual(self.events[-1], {"protocol": 1, "type": "nova-ready"})
        for cmd in ("pause", "seek_by", "volume"):
            proxy.pending.put({"protocol": 1, "cmd": cmd})
        for cmd in ("pause", "seek_by", "volume"):
            with urlopen(self.base + "/poll" + token, timeout=3) as response:
                self.assertEqual(json.loads(response.read())["cmd"], cmd)

    def test_status_forwards_only_public_fields(self):
        message = {
            "protocol": 1, "type": "nova-status", "running": True,
            "position": 42.0, "source": "https://example.test/private?token=not-for-stdout",
            "password": "not-for-stdout", "fgEnabled": True,
        }
        request = Request(self.base + "/status?token=unit-test-token-no-media",
                          json.dumps(message).encode(),
                          {"Content-Type": "application/json"}, method="POST")
        with urlopen(request, timeout=3) as response:
            self.assertEqual(response.status, 200)
        status = self.events[-1]
        self.assertEqual(status["type"], "nova-status")
        self.assertEqual(status["position"], 42)
        self.assertTrue(status["fgEnabled"])
        self.assertNotIn("source", status)
        self.assertNotIn("password", status)

    def test_rejects_bad_status_shape_and_unauthorized(self):
        for payload in ({"type": "status"}, {"protocol": 2, "type": "nova-status"}):
            request = Request(self.base + "/status?token=unit-test-token-no-media",
                              json.dumps(payload).encode(), method="POST")
            with self.assertRaises(HTTPError) as context:
                urlopen(request, timeout=3)
            self.assertEqual(context.exception.code, 400)
        request = Request(self.base + "/status?token=bad",
                          b'{"protocol":1,"type":"nova-status"}', method="POST")
        with self.assertRaises(HTTPError) as context:
            urlopen(request, timeout=3)
        self.assertEqual(context.exception.code, 403)

    def test_qml_shim_source_present_and_inert_config(self):
        source = (ROOT / "integrations/veyra/qml/NovaControl.qml").read_text(encoding="utf-8")
        config = (ROOT / "integrations/veyra/qml/NovaBridgeConfig.js").read_text(encoding="utf-8")
        self.assertIn("veyra.openNetworkAddress(source)", source)
        self.assertIn("veyra.fgBackendName", source)
        self.assertIn("nova-status", source)
        self.assertIn('var endpoint = ""', config)
        self.assertIn('var token = ""', config)


if __name__ == "__main__":
    unittest.main()
