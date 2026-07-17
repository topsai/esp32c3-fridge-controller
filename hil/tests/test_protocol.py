import unittest

from hil.protocol import HilClient, ProtocolError


class FakeTransport:
    def __init__(self, response):
        self.response = response
        self.lines = []

    def exchange(self, line, timeout=2.0):
        self.lines.append(line)
        return self.response


class ProtocolTest(unittest.TestCase):
    def test_sequence_is_added_and_validated(self):
        transport = FakeTransport('{"type":"ack","sequence":1,"ok":true}')
        client = HilClient(transport)
        response = client.command("PING")
        self.assertEqual("HIL 1 PING", transport.lines[0])
        self.assertTrue(response["ok"])

    def test_mismatched_sequence_is_rejected(self):
        client = HilClient(FakeTransport('{"type":"ack","sequence":9,"ok":true}'))
        with self.assertRaises(ProtocolError):
            client.command("PING")


if __name__ == "__main__":
    unittest.main()
