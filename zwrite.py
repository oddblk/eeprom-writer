from serial import Serial
from serial.tools.list_ports import comports  # type: ignore
from time import sleep

# see https://danceswithferrets.org/geekblog/?p=1325
from zlib import compress
from base64 import b64encode


def chunked(s: bytes, chunk_size: int = 60):
    return [
        s[i:i+chunk_size] for i in range(0, len(s), chunk_size)
    ]


source = open('hhg.txt', 'rb').read()

payload = b64encode(compress(source))

print(f"(encoded {len(source)} bytes to {len(payload)} b64 zlib bytes)")

usb_port = next((p.device for p in comports() if p.vid), None)
assert usb_port, "Couldn't find usb port, is Arduino connected?"

print(f"(connecting to arduino at {usb_port})")

# defaults match arduino bytesize=EIGHTBITS, parity=PARITY_NONE, stopbits=STOPBITS_ONE
conn = Serial(port=usb_port, baudrate=9600, timeout=1)

print(conn.readline().decode('ascii'))

print('(checking version)')
conn.write(b'V\n')

print(conn.readline().decode('ascii').rstrip())

print('(sending data)')
conn.write(b'Z 1234\n')
sleep(0.1)

while conn.in_waiting:
    print(conn.readline().decode('ascii').rstrip())
    sleep(0.1)

for line in chunked(payload):
    print(len(line), line)
    conn.write(line + b'\n')
    sleep(0.1)
    while conn.in_waiting:
        print(conn.readline().decode('ascii').rstrip())

sleep(0.1)
while conn.in_waiting:
    print(conn.readline().decode('ascii').rstrip())

conn.close()
