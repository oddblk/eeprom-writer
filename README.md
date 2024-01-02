### eeprom-writer

Arduino firmware for EEPROM Writer project.

This code should be compiled by the Arduino IDE and run on an Arduino Mega 2560.

For project documentation, refer to http://danceswithferrets.org/geekblog/?page_id=903

Distributed under an acknowledgement licence, because I'm a shallow, attention-seeking tart. :)


This software presents a 115200-8N1 serial port, and supports the following commands:

    R[hex address] [optional hex length]   - reads length (default 16) data bytes from the EEPROM
    W[hex address]:[data in two-char hex]  - writes data bytes to the EEPROM
    P                                      - set write-protection bit (Atmels only, AFAIK)
    U                                      - clear write-protection bit (ditto)
    V                                      - prints the version string
    Z[hex address]                         - write base64 compressed data to the EEPROM
    C[hex address] [hex length]            - run XOR and adler32 checksum for range address+length

## Notes:

- Most commands can include optional whitespace and are case-insensitive,
  e.g. `W1234:010203ABCDEF` is equivalent to `w 1234 : 1 2 3 AB CD EF`

- Any data read from the EEPROM will have a CRC checksum appended to it (separated by a comma).
  If a string of data is sent with an optional checksum, then this will be checked
  before anything is written.

- The Z command is followed by one or more lines of b64-zlib data, with a maximum 64 char line length.
  Each line is acknowledged by its line length. The zlib header and adler32 checksum are also
  verified.
  You can create suitable input data in python using
  `zlib.compress(base64.b64encode(data)).encode('ascii')`
  and then splitting the result into chunks of (say) 60 characters.

- The python script `zwrite.py` can be used to write eeprom images using the `Z` command.
  You'll need the pyserial library.  Either `pip install pyserial` or use `environment.yml`
  to create a python environment, with `miniconda` or `micromamba`, e.g.
  `conda create -f environment.yml`.    Then run like `python zwrite.py -a 1234 -c rom.bin`.
  Use `python zwrite.py --help` for help.   You should see output like:

```sh
(reading rom from apple2e.rom)
(encoded 32768 source bytes to 18404 b64 compressed bytes)
(searching for arduino USB port)
(connecting to arduino at /dev/cu.usbmodem14201)
Ready
(eeprom-writer version)
EEPROM Version=0.03
(uploading data @ address 0)
B64Z
...................................................................................................................................................................................................................................................................................................................
(upload complete)
STATUS 0 BYTES 32768 ADLER32 OK
(verifying checksum, expect BYTES 32768 XOR 2B ADLER32 B8EB06B6)
BYTES 32768 XOR 2B ADLER32 B8EB06B6
```

## References

- zlib decoder based on [`puff`](https://github.com/madler/zlib/tree/develop/contrib/puff),
    along with the [adler32 checksum](https://github.com/madler/zlib/blob/develop/adler32.c) code.

- [zlib spec](https://datatracker.ietf.org/doc/html/rfc1950)
  and [deflate spec](https://datatracker.ietf.org/doc/html/rfc1951) if you're curious

- the [wikipedia base64 page](https://en.wikipedia.org/wiki/Base64) used to implement `base64.[ch]`

- [AT28C256 EEPROM datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf)

## Examples of Z upload

- a small test case with header 78 9c and trailing checksum dd 08 8a fb (3708324603)
  which decodes to 388 bytes

```
Z 1234
eJwtkEFyhTAMQ/ecQhfoP0a77BkMMeD5xmESU0pPX/HbXTKyNE96l4Z6JCyQ
q+KIaZWWWjDK9DwltXXU+V+bpa9WQ0ZXQKOA0nBLOLXzNIC+WxMHpG23ipfz
Q1y+L7hph6Bv4s60pou0omW41L2e6Ec8huGzjZYWC43G66ShWE+JSe+8Vo9l
9QthoXm95VmBzdyJ9XpoH2iD8HckkXhq0W0Jm22SSEJkEn/0Q7E01cDuwiyc
a+0K2fVtKNon1uMKbrNirm0jSlP0Ctnkh3jM3ZttRP1SopLzbnpxgCQNR7F4
Enyx5BrccVr1L0Lo00w2UJqsqDx+Ad0Iivs=

BYTES 388 XOR 40 ADLER32 DD088AFB
```

- 32K of repeating 00 01 02 ... FF

```
z 0
eJztz4dCCAAAQMFSaCChQRqEopQiJRoyk7Q0kBKVkQiJVBrIbFAUlXYymnYU
aVmVMlP2jrKirXzIuz84AcE+QsJ9+/UXERUTHzBwkMRgySFDh0lJy8gOHyE3
Ul5BUWnUaOUxY8epqI6foKY+UUNzkpb25Ck6U3X1pulPn2FgaGQ802TW7Dlz
5803XWC20HyRhaWVtc1iWzt7hyVLlzkud3Je4bJylaub++o1a9d5rPfcsNFr
0+Yt3lt9tm333eHnH7AzMCg4ZNfuPaF79+0/cPBQWHhE5OEjUdFHj8XEHj8R
F59wMjEpOSU1LT3jVObpM2fPZWXn5Obln79w8dLlK1cLrl0vLLpxs/hWSWlZ
ecXtO3fv3a+sqn5QU/vw0eMnT5/VPa9vePHy1es3b9+9//Dx0+cvjV+/NTV/
//Hz1++WP39b29o7Oru6//UI8OfPnz9//vz58+fPnz9//vz58+fPnz9//vz5
8+fPnz9//vz58+fPnz9//vz58+fPnz9//vz58+fPnz9//vz58+fPnz9//vz5
8+fPnz9//vz58+fPnz9//vz58+fPnz9//vz58+fPnz9//vz58+fPnz9//vz5
8+fPnz9//vz58+fPnz9//vz58+fPnz9//v//vT/cw7I=

BYTES 32768 XOR 00 ADLER32 3FDCC3B2
```

- 32K of 00

```
z 0
eJztwQEBAAAAgJD+r+4ICgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAYgAAAAQ==

BYTES 32768 XOR 00 ADLER32 80000001
```

- 32K of EA (6502 noop)

```
z 0
eJztwYEAAAAAgCB/1SkWqQoAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAGN0fBtw=

BYTES 32768 XOR 00 ADLER32 DD1F06DC
```