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
  Use `python zwrite.py --help` for help.

## References

- zlib decoder based on [`puff`](https://github.com/madler/zlib/tree/develop/contrib/puff),
    along with the [adler32 checksum](https://github.com/madler/zlib/blob/develop/adler32.c) codde.

- [zlib spec](https://datatracker.ietf.org/doc/html/rfc1950)
  and [deflate spec](https://datatracker.ietf.org/doc/html/rfc1951) if you're curious

- the [wikipedia base64 page](https://en.wikipedia.org/wiki/Base64) used to implement `base64.[ch]`

- AT28C256 datasheet https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf
