"""Read-only serial decoder for the EXP10 observer sketch."""
import argparse
from pathlib import Path
import re
import sys
import time

import cantools
import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parents[1]


def decode_line(db, line):
    fields = line.split()
    if not fields or not fields[0].isdigit():
        return None  # Sketch banner, not a frame.
    if len(fields) < 3:
        raise ValueError("Eksik seri satir")
    stamp = int(fields[0])
    frame_id = int(fields[1], 16)
    dlc = int(fields[2])
    if not 0 <= frame_id <= 0x7FF or not 0 <= dlc <= 8:
        raise ValueError("ID/DLC aralik disi")
    if len(fields) != 3 + dlc or any(not re.fullmatch(r"[0-9A-Fa-f]{2}", x) for x in fields[3:]):
        raise ValueError("Bayt sayisi veya hex bicimi hatali")
    if frame_id != 0x300:
        return None
    if dlc != 8:
        raise ValueError("EXP10 DLC 8 olmali")
    payload = bytes.fromhex(" ".join(fields[3:]))
    values = db.decode_message(frame_id, payload, decode_choices=False)
    if values["Version"] != 1 or values["ReservedFlags"] != 0 or values["Reserved"] != 0:
        raise ValueError("Surum/rezerve alan hatasi")
    if not -40 <= values["Temperature"] <= 125 or not 0 <= values["Voltage"] <= 5:
        raise ValueError("Sinyal aralik disi")
    return stamp, payload, values


def describe(decoded):
    stamp, payload, values = decoded
    kind = "YAPAY" if values["SyntheticTemperature"] else "OLCULEN"
    return (f"{stamp:>9} ms | sayac={int(values['Counter']):3d} | "
            f"sicaklik={values['Temperature']:6.1f} C ({kind}) | "
            f"gerilim={values['Voltage']:.3f} V | {payload.hex(' ').upper()}")


def choose_port():
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    if sys.platform == "darwin":
        ports = [p for p in ports if p.device.startswith('/dev/cu.') and 'Bluetooth' not in p.device]
    if not ports:
        raise ValueError("Port bulunamadi. Nano'yu veri tasiyan USB kablosuyla bagla ve yeniden ac.")
    print("Nano portunu sec. Uno portunu secme; acmak karti resetleyebilir.")
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port.device} — {port.description}")
    answer = input("Port numarasi (iptal: q): ").strip()
    if answer.lower() == 'q':
        return None
    if not answer.isdigit() or not 1 <= int(answer) <= len(ports):
        raise ValueError("Gecersiz secim; programi yeniden ac.")
    return ports[int(answer)-1].device


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Nano serial port, e.g. /dev/cu.usbserial-...')
    parser.add_argument('--demo', action='store_true', help='Decode a synthetic example without USB')
    args = parser.parse_args()
    db = cantools.database.load_file(str(ROOT / 'docs/protocol/exp10.dbc'))
    if args.demo:
        print("DEMO — kayitli ornek, canli donanim verisi degil")
        print(describe(decode_line(db, '178591 300 8 01 0B 85 FF 88 13 01 00')))
        return 0
    print("EXP10 CAN / DBC canli okuyucu — 115200 baud")
    print("Once Nano'nun Arduino Seri Monitorunu kapat. Cikis: Ctrl+C.")
    print("Gerilim 5 V ADC referansi varsayar; sicaklik test verisidir.")
    port = args.port or choose_port()
    if port is None:
        return 0
    # Opening may reset the Nano. No commands or CAN frames are sent.
    with serial.Serial(port, 115200, timeout=0.2, exclusive=True) as connection:
        print(f"Acildi: {port}. Nano resetlenebilir; veri bekleniyor...")
        pending = bytearray()
        last_valid = time.monotonic()
        stale = False
        counter = None
        while True:
            chunk = connection.read(min(connection.in_waiting or 1, 4096))
            pending.extend(chunk)
            while b'\n' in pending:
                raw, _, rest = pending.partition(b'\n')
                pending = bytearray(rest)
                line = raw.decode('ascii', errors='replace').strip()
                try:
                    decoded = decode_line(db, line)
                    if decoded is None:
                        if line:
                            print("NANO:", line[:200])
                        continue
                    current = int(decoded[2]['Counter'])
                    sequence = "SYNC" if counter is None else "OK" if current == (counter+1) % 256 else "SIRA UYARISI"
                    print(describe(decoded), '|', sequence, flush=True)
                    counter = current
                    last_valid = time.monotonic()
                    stale = False
                except (ValueError, KeyError, cantools.database.errors.DecodeError) as error:
                    print(f"RED: {error} | {line[:160]}", flush=True)
            if len(pending) > 4096:
                pending.clear()
                print("RED: satir siniri asildi; dogru Nano portunu/baud'u kontrol et.")
            if not stale and time.monotonic()-last_valid > 2:
                print("VERI YOK: 2 saniyedir gecerli EXP10 satiri gelmedi. Portu ve yayin yapan Uno'yu kontrol et.", flush=True)
                stale = True
                counter = None


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nDurduruldu; seri port kapatildi.")
    except (serial.SerialException, OSError, ValueError, EOFError) as error:
        print(f"HATA: {error}\nArduino Seri Monitorunu kapatip USB/port secimini kontrol et.", file=sys.stderr)
        sys.exit(1)
