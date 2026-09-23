"""Run with Python + cantools; no hardware required."""
from pathlib import Path
import math
import struct
import cantools

db = cantools.database.load_file(str(Path(__file__).resolve().parents[1] / "docs/protocol/exp10.dbc"))
message = db.get_message_by_frame_id(0x300)
assert message.length == 8 and not message.is_extended_frame
for temperature in (-400, -123, 0, 253, 850, 1250):
    for mv in (0, 2500, 5000):
        for counter in (0, 42, 255):
            expected = struct.pack("<BBhHBB", 1, counter, temperature, mv, 1, 0)
            signals = dict(Version=1, Counter=counter, Temperature=temperature/10,
                           Voltage=mv/1000, SyntheticTemperature=1, ReservedFlags=0, Reserved=0)
            assert message.encode(signals) == expected
            decoded = message.decode(expected, decode_choices=False)
            for key, value in signals.items():
                assert math.isclose(decoded[key], value, abs_tol=1e-9), (key, decoded)
assert struct.pack("<BBhHBB", 1, 42, -123, 2500, 1, 0).hex() == "012a85ffc4090100"
print("PASS: 54 DBC encode/decode vectors, signed values, byte order, scaling, frame ID and length")
