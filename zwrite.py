# Script for uploading rom images to eeprom-writer
# python zwrite.py --help for help.

from serial import Serial
from serial.tools.list_ports import comports  # type: ignore
from time import sleep
import argparse
import sys
from functools import reduce

from zlib import compress
from base64 import b64encode


eeprom_size = 1 << 15           # 32K eeprom


def chunked(s: bytes, chunk_size: int = 60) -> list[bytes]:
    """Split a string (or bytes) into fixed size chunks"""
    return [
        s[i:i+chunk_size] for i in range(0, len(s), chunk_size)
    ]


parser = argparse.ArgumentParser(
    prog='zwrite',
    description='Write binary rom images with eeprom-writer',
)

parser.add_argument('romfile', nargs='?',
    help="Binary ROM image to upload")
parser.add_argument('-f', '--fill', nargs='+',
    help="Fill with hex byte(s)")
parser.add_argument('-a', '--address', default='0',
    help="hex start address for upload (default 0)")
parser.add_argument('-l', '--length',
    help="truncate fill pattern (or rom) after hex length bytes")
parser.add_argument('-p', '--port',
    help="Arduino USB port, like /dev/cu.usbmodem14101 (default autodetect)")
parser.add_argument('-c', '--check', action='store_true',
    help="Re-read to test checksum after writing")

args = parser.parse_args()

if not args.romfile and not args.fill:
    sys.stderr.write("error: specify either romfile or --fill option\n")
    parser.print_help()
    sys.exit(1)

start = int(args.address, 16)
length = int(args.length, 16) if args.length else None

if args.romfile:
    print(f"(reading rom from {args.romfile})")
    rom = open(args.romfile, 'rb').read()
    if length:
        rom = rom[:length]
else:
    pattern = bytes([int(v, 16) for v in args.fill])
    s = ''.join(' {:02x}'.format(b).upper() for b in pattern)
    print(f"(filling {length} bytes with{s})")
    if not length:
        length = eeprom_size - start
    rom = (pattern * (length//len(pattern) + 1))[:length]

zipped = compress(rom)
payload = b64encode(zipped)

print(f"(encoded {len(rom)} source bytes to {len(payload)} b64 compressed bytes)")

if args.port:
    usb_port = args.port
else:
    print(f"(searching for arduino USB port)")
    usb_port = next((p.device for p in comports() if p.vid), None)
    assert usb_port, "Couldn't find usb port, is Arduino connected?"

print(f"(connecting to arduino at {usb_port})")

# pyserial defaults match 8N1 sketch, i.e. bytesize=EIGHTBITS, parity=PARITY_NONE, stopbits=STOPBITS_ONE
conn = Serial(port=usb_port, baudrate=115200, timeout=5)

# readline can timeout on long write cycles, so wait for actual content
def await_response(show=True):
    while True:
        line = conn.readline().decode('ascii').rstrip()
        if line:
            break
    if show:
        print(line)
    return line


await_response()      # wait for ready

print('(eeprom-writer version)')
conn.write(b'V\n')
await_response()

print(f'(uploading data @ address {args.address})')
conn.write(f"Z {args.address}\n".encode('ascii'))
await_response()

# write each line of encoded payload, and wait for ack
ok = True
for line in chunked(payload):
    conn.write(line + b'\n')
    ack = await_response(show=False)
    ok = ack == str(len(line))
    if not ok:
        print(f"(arduino reported unexpected line length {ack})")
        break
    print('.', end='')
    sys.stdout.flush()

print()

if ok:
    print('(upload complete)')
    await_response()

    if args.check:
        adler32 = '{:08x}'.format(int.from_bytes(zipped[-4:], 'big')).upper()
        xor = '{:02x}'.format(reduce(lambda x, y: x^y, rom)).upper()
        print(f"(verifying checksum, expect BYTES {len(rom)} XOR {xor} ADLER32 {adler32})")
        conn.write(f"C {args.address} {len(rom):04x}\n".encode('ascii'))
        await_response()
else:
    print('(upload failed)')

conn.close()
