"""Offline checks for the observer serial-line decoder."""
import sys
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from exp10_live import decode_line, cantools


class DecoderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.db = cantools.database.load_file(str(ROOT / 'docs/protocol/exp10.dbc'))

    def test_observer_sample(self):
        stamp, payload, values = decode_line(self.db, '178591 300 8 01 0B 85 FF 88 13 01 00\r\n')
        self.assertEqual(stamp, 178591)
        self.assertEqual(len(payload), 8)
        self.assertEqual(values['Counter'], 11)
        self.assertAlmostEqual(values['Temperature'], -12.3)
        self.assertEqual(values['Voltage'], 5)

    def test_banner_and_other_id(self):
        for line in ('', 'EXP10 OBSERVER', '100 301 1 00'):
            with self.subTest(line=line):
                self.assertIsNone(decode_line(self.db, line))

    def test_invalid_frames(self):
        for line in (
            '100 300',
            '100 300 8 01 00',
            '100 300 8 01 00 GG FF 88 13 01 00',
            '100 300 1 00',
            '100 300 8 02 00 85 FF 88 13 01 00',
            '100 300 8 01 00 85 FF 88 13 03 00',
            '100 300 8 01 00 85 FF 88 13 01 01',
            '100 300 8 01 00 FF 83 88 13 01 00',
            '100 300 8 01 00 85 FF 89 13 01 00',
        ):
            with self.subTest(line=line):
                with self.assertRaises(ValueError):
                    decode_line(self.db, line)


if __name__ == '__main__':
    unittest.main()
