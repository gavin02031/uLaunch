import importlib.util
import pathlib
import unittest
p = pathlib.Path(__file__).resolve().parents[1] / "tools" / "glassnx_presence.py"
spec = importlib.util.spec_from_file_location("presence", p)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
class PresenceTests(unittest.TestCase):
    def test_wire_format(self):
        self.assertEqual(m.PACKET.size, 628)
        self.assertEqual(m.decode(m.PACKET.pack(m.MAGIC, 42, b"Super Mario Odyssey\0")), (42, "Super Mario Odyssey"))
    def test_partial_reads(self):
        class Socket:
            data = bytearray(m.PACKET.pack(m.MAGIC, 10, b"Game\0"))
            def recv(self, n):
                v = self.data[:min(n, 13)]
                del self.data[:len(v)]
                return v
        self.assertEqual(m.receive(Socket()), (10, "Game"))
    def test_invalid_magic(self):
        with self.assertRaises(ValueError):
            m.decode(m.PACKET.pack(0, 0, b""))
    def test_home(self):
        self.assertEqual(m.decode(m.PACKET.pack(m.MAGIC, 0, b"SNULL"))[0], 0)
if __name__ == "__main__":
    unittest.main()
