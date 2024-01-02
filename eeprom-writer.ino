// EEPROM Programmer - code for an Arduino Mega 2560
//
// Written by K Adcock.
//       Jan 2016 - Initial release
//       Dec 2017 - Slide code tartups, to remove compiler errors for new Arduino IDE (1.8.5).
//   7th Dec 2017 - Updates from Dave Curran of Tynemouth Software, adding commands to enable/disable SDP.
//  10th Dec 2017 - Fixed one-byte EEPROM corruption (always byte 0) when unprotecting an EEPROM
//                  (doesn't matter if you write a ROM immediately after, but does matter if you use -unprotect in isolation)
//                - refactored code a bit (split loop() into different functions)
//                - properly looked at timings on the Atmel datasheet, and worked out that my delays
//                  during reads and writes were about 10,000 times too big!
//                  Reading and writing is now orders-of-magnitude quicker.
//  30th Dec 2023 - Fixed paged write bug; added zlib support; more flexible input handling (Patrick Surry)
//
// Distributed under an acknowledgement licence, because I'm a shallow, attention-seeking tart. :)
//
// http://danceswithferrets.org/geekblog/?page_id=903
//
// This software presents a 115200-8N1 serial port.
//
// R[hex address]                         - reads 16 bytes of data from the EEPROM
// W[hex address]:[data in two-char hex]  - writes up to 16 bytes of data to the EEPROM
// P                                      - set write-protection bit (Atmels only, AFAIK)
// U                                      - clear write-protection bit (ditto)
// V                                      - prints the version string
// Z[hex address]                         - followed by lines of base64 compressed data in up to 63 byte chunks
//
// Any data read from the EEPROM will have a CRC checksum appended to it (separated by a comma).
// If a string of data is sent with an optional checksum, then this will be checked
// before anything is written.
//
// AT28C256 datasheet https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf
//

#include <avr/pgmspace.h>

extern "C" {
  #include "puff.h"
  #include "adler32.h"
  #include "base64.h"
}

const char version_string[] = {"EEPROM Version=0.03"};

static const int kPin_Addr14  = 24;
static const int kPin_Addr12  = 26;
static const int kPin_Addr7   = 28;
static const int kPin_Addr6   = 30;
static const int kPin_Addr5   = 32;
static const int kPin_Addr4   = 34;
static const int kPin_Addr3   = 36;
static const int kPin_Addr2   = 38;
static const int kPin_Addr1   = 40;
static const int kPin_Addr0   = 42;
static const int kPin_Data0   = 44;
static const int kPin_Data1   = 46;
static const int kPin_Data2   = 48;
static const int kPin_nWE     = 27;     // write enable LOW
static const int kPin_Addr13  = 29;
static const int kPin_Addr8   = 31;
static const int kPin_Addr9   = 33;
static const int kPin_Addr11  = 35;
static const int kPin_nOE     = 37;     // output enable LOW (for reading)
static const int kPin_Addr10  = 39;
static const int kPin_nCE     = 41;     // chip enable LOW
static const int kPin_Data7   = 43;
static const int kPin_Data6   = 45;
static const int kPin_Data5   = 47;
static const int kPin_Data4   = 49;
static const int kPin_Data3   = 51;
static const int kPin_WaitingForInput  = 13;
static const int kPin_LED_Red = 22;
static const int kPin_LED_Grn = 53;

static const int dataPins[8] = {
    kPin_Data0, kPin_Data1, kPin_Data2, kPin_Data3, kPin_Data4, kPin_Data5, kPin_Data6, kPin_Data7
};
static const int addrPins[15] = {
    kPin_Addr0, kPin_Addr1, kPin_Addr2,  kPin_Addr3,  kPin_Addr4,  kPin_Addr5,  kPin_Addr6,  kPin_Addr7,
    kPin_Addr8, kPin_Addr9, kPin_Addr10, kPin_Addr11, kPin_Addr12, kPin_Addr13, kPin_Addr14
};

static const int kMaxCommandSize = 120;
byte command[kMaxCommandSize]; // strings received from the controller will go in here; note  drops chars after 64

static const int kMaxBufferSize = 64;
byte buffer[kMaxBufferSize];

static const long int k_uTime_WritePulse_uS = 1;
static const long int k_uTime_ReadPulse_uS = 1;
// (to be honest, both of the above are about ten times too big - but the Arduino won't reliably
// delay down at the nanosecond level, so this is the best we can do.)

static const unsigned long eeprom_size = 1 << 15;   // 32K

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(115200);

  pinMode(kPin_WaitingForInput, OUTPUT); digitalWrite(kPin_WaitingForInput, HIGH);
  pinMode(kPin_LED_Red, OUTPUT); digitalWrite(kPin_LED_Red, LOW);
  pinMode(kPin_LED_Grn, OUTPUT); digitalWrite(kPin_LED_Grn, LOW);

  // address lines are ALWAYS outputs
  for (int i=0; i<15; i++) pinMode(addrPins[i], OUTPUT);

  // control lines are ALWAYS outputs from Arduino perspective
  pinMode(kPin_nCE, OUTPUT); digitalWrite(kPin_nCE, LOW);   // chip always enabled
  pinMode(kPin_nWE, OUTPUT); digitalWrite(kPin_nWE, HIGH);  // disable write by default
  pinMode(kPin_nOE, OUTPUT); // LOW is reading, HIGH is writing

  SetDataDirection(INPUT);
  SetAddress(0);

  Serial.println("Ready");
}


void loop()
{
  while (true)
  {
    digitalWrite(kPin_WaitingForInput, HIGH);
    ReadString();
    digitalWrite(kPin_WaitingForInput, LOW);

    switch (command[0] & 0xDF)      // unset 0b0010 0000 to allow lower case
    {
      case 'V': Serial.println(version_string); break;
      case 'P': SetSDPState(true); break;
      case 'U': SetSDPState(false); break;
      case 'R': ReadEEPROM(); break;
      case 'W': WriteEEPROM(); break;
      case 'Z': BulkWriteEEPROM(); break;
      case 'C': ChecksumEEPROM(); break;
      case 0: break; // empty string. Don't mind ignoring this.
      default: Serial.println("ERR bad command"); break;
    }
  }
}

void ReadEEPROM()
// R <address> [<length>]
// reads up to kMaxBufferSize bytes from EEPROM, beginning at <address> (in hex)
// length defaults to 16
{
  if (command[1] == 0)
  {
    Serial.println("ERR no address");
    return;
  }

  // decode ASCII representation of address (in hex) into an actual value
  char *p = NULL;
  unsigned long addr = strtol(command+1, &p, 16),
      n = strtol(p, NULL, 16);

  if (n == 0) n = 16;
  else
    if (n > kMaxBufferSize) n = kMaxBufferSize;

  SetDataDirection(INPUT);
  delayMicroseconds(1);

  ReadEEPROMIntoBuffer(buffer, addr, n);

  // now print the results, starting with the address as hex ...
  PrintByte((addr >> 8) & 0xff);
  PrintByte(addr & 0xff);
  Serial.print(":");
  PrintBuffer(n);

  Serial.println("OK");
}

void WriteEEPROM() // W<four byte hex address>:<data in hex, two characters per byte, max of 16 bytes per line>
{
  if (command[1] == 0)
  {
    Serial.println("ERR no address");
    return;
  }

  char *p = NULL;
  unsigned long addr = strtol(command+1, &p, 16);

  while(*p && *p != ':') p++;
  if (!(*p)) {
    Serial.println("ERR no data");
    return;
  }
  p++;

  uint8_t iBufferUsed = 0;
  byte tmp;
  char *q;
  while (1) {
    // handle "ABCDEF" (3 hex digit pairs); "AB CD EF"; and "E  F 10 11" (single digits and extra spacing)
    // edge case is string ending with a single digit
    while (*p == ' ') p++;
    if (*p == 0 || *p == ',' || iBufferUsed == kMaxBufferSize) break;

    // if not past end of string, temporarily terminate so we parse at most two chars
    if (*(p+1)) {
        tmp = *(p+2);
        *(p+2) = 0;
    }
    buffer[iBufferUsed++] = strtol(p, &q, 16);
    if (q == p) {
        // stop at bad input
        Serial.print("WARN invalid input at index ");
        Serial.println(p - (char*)command);
        iBufferUsed--;
        break;
    }
    if (*(p+1)) *(p+2) = tmp;   // remove temporary terminator
    p = q;          // advance past parsed data, which might be a single char
  }

  // if we're pointing to a comma, then the optional checksum has been provided!
  if (*p == ',' && *(p+1))
  {
    byte checksum = strtol(p+1, NULL, 16);

    byte our_checksum = CalcBufferChecksum(iBufferUsed);

    if (our_checksum != checksum)
    {
      // checksum fail!
      iBufferUsed = -1;
      Serial.print("ERR expected ");
      PrintByte(checksum);
      Serial.print(" got ");
      PrintByte(our_checksum);
      return;
    }
  }

  // buffer should now contain some data
  if (iBufferUsed > 0)
  {
    WriteBufferToEEPROM(buffer, addr, iBufferUsed);
  }

  Serial.println("OK");
}


/* implement simple streaming reader/writer to hook up base64 deflate */

struct SerialReader {
    unsigned char buf[kMaxBufferSize];
    int buflen;
    int bufidx;
};

void sr_init(SerialReader *s);
int sr_read(SerialReader *s, unsigned char *buf, int n);

void sr_init(SerialReader *s) {
    s->buflen = 0;
    s->bufidx = 0;
}

int sr_read(SerialReader *s, unsigned char *buf, int n) {
    int req, actual, avail;

    actual = 0;
    while (actual < n) {
        if (s->bufidx >= s->buflen) {
            ReadString();
            int len = strlen(command);
            Serial.println(len);        // ack the input line with number of chars
            if (len > kMaxBufferSize) return -1;     // overflow!
            s->bufidx = 0;
            s->buflen = len;
            memcpy(s->buf, command, len);
        }
        req = n - actual;
        avail = s->buflen - s->bufidx;
        if (req > avail) req = avail;
        memcpy(buf + actual, s->buf + s->bufidx, req);
        s->bufidx += req;
        actual += req;
    }
    return actual;
}

const int ew_bufsiz = 1 << 6;      // must be a power of 2 for rewrite implementation

struct EepromWriter {
    unsigned long base_address;

    unsigned char buf[ew_bufsiz];
    int bufidx;

    unsigned long outlen;
    unsigned long chksum;
};

void ew_init(EepromWriter *ew, unsigned long base_address);
int ew_write(EepromWriter *ew, const unsigned char *buf, int n);
int ew_rewrite(EepromWriter *ew, int n, unsigned dist);
void ew_flush(EepromWriter *ew);


void ew_init(EepromWriter *ew, unsigned long base_address) {
    ew->base_address = base_address;
    ew->bufidx = 0;
    ew->outlen = 0;
    ew->chksum = 1;   // starting value for adler32
}

int ew_write(EepromWriter *ew, const unsigned char *buf, int n) {
    int actual = 0, k;

    if (n + ew->outlen > eeprom_size) n = eeprom_size - ew->outlen;

    while (actual < n) {
        if (ew->bufidx & ew_bufsiz) ew_flush(ew);
        k = n - actual;
        if (k > ew_bufsiz - ew->bufidx) k = ew_bufsiz - ew->bufidx;
        memcpy(ew->buf + ew->bufidx, buf + actual, k);
        actual += k;
        ew->bufidx += k;
        ew->outlen += k;
    }
    return actual;
}

int ew_rewrite(EepromWriter *ew, int n, unsigned dist) {
    /* return -1 if too far back, else bytes copied (expect n) */
    if (dist > ew->outlen) return -1;

    if (dist < ew_bufsiz) {
        /* for nearby backref within the rolling output buffer
         * we can do a forward copy rolling from low to high index
         * so that, e.g. rewrite(..., 10, -1) makes 10 copies of the last byte
         */
        int from = (ew->bufidx - dist) & (ew_bufsiz - 1);
        for (int i=0; i<n; i++) {
            if (ew->bufidx & ew_bufsiz) ew_flush(ew);
            if (from & ew_bufsiz) from = 0;
            ew->buf[ew->bufidx++] = ew->buf[from++];
            ew->outlen++;
        }
    } else {
        /* for backrefs outside our buffer we read and write from the eeprom */
        int actual = 0;
        while (actual < n) {
            int k = n - actual;
            if (k > kMaxBufferSize) k = kMaxBufferSize;
            /* read thru input buffer and write into our own to deal with flush etc */
            ReadEEPROMIntoBuffer(buffer, ew->base_address + ew->outlen - dist, k);
            ew_write(ew, buffer, k);
            actual += k;
        }
    }
    return n;
}

void ew_flush(EepromWriter *ew) {
    /* flush buffer to eeprom */

    /* outlen tracks the current address including the buffer contents */
    ew->chksum = adler32(ew->chksum, ew->buf, ew->bufidx);
    WriteBufferToEEPROM(ew->buf, ew->base_address + ew->outlen - ew->bufidx, ew->bufidx);
    ew->bufidx = 0;
}

void ChecksumEEPROM() {
    // C <four byte address>  <hex length>
    // show both the simple xor and adler32 checksum for the data range
    // if address not provided, it defaults to zero
    // if length not provided it defaults to eeprom size - start

    // decode ASCII representation of address (in hex) into an actual value
    char *p = NULL;
    unsigned long addr = strtol(command+1, &p, 16),
        n = strtol(p, NULL, 16);
    if (n == 0 || addr + n > eeprom_size) n = eeprom_size - addr;

    unsigned long a32chk = 1, xorchk = 0, todo = n;

    while (todo > 0) {
        int i = todo > kMaxBufferSize ? kMaxBufferSize : todo;
        ReadEEPROMIntoBuffer(buffer, addr, i);
        a32chk = adler32(a32chk, buffer, i);
        xorchk ^= CalcBufferChecksum(i);
        addr += i;
        todo -= i;
    }
    Serial.print("BYTES ");
    Serial.print(n);
    Serial.print(" XOR ");
    PrintByte(xorchk);
    Serial.print(" ADLER32 ");
    for (int i=3; i>=0; i--) {
        unsigned char b = (a32chk >> (i<<3)) & 0xff;
        PrintByte(b);
    }
    Serial.println();
}


void BulkWriteEEPROM() {
  // Z <four byte address>
  // followed by lines of base64 encoded deflate data (max 64 char per line)

  if (command[1] == 0)
  {
    Serial.println("ERR no address");
    return;
  }

  unsigned long addr = strtol(command+1, NULL, 16);

  SerialReader sr;
  B64Decoder b64;
  EepromWriter ew;

  sr_init(&sr);
  b64_init(&b64, &sr, sr_read);
  ew_init(&ew, addr);

  ZIO zio = {
      &b64, b64_read,
      &ew, ew_write, ew_rewrite,
  };

  Serial.println("B64Z");   // acknowledge ready for data

  int status = puffs(&zio, 1);
  ew_flush(&ew);
  unsigned long expected=0;
  unsigned char data[4];
  b64_read(&b64, data, 4);
  for(int i=0; i<4; i++)
      expected = (expected << 8) | data[i];

  Serial.print("STATUS ");
  Serial.print(status);
  Serial.print(" BYTES ");
  Serial.print(ew.outlen);
  Serial.print(" ADLER32 ");
  if (expected == ew.chksum) {
    Serial.println("OK");
  } else {
    Serial.print("FAIL expected ");
    Serial.print(expected);
    Serial.print(" but got ");
    Serial.println(ew.chksum);
  }
}

// Important note: the EEPROM needs to have data written to it immediately after sending the "unprotect" command, so that the buffer is flushed.
// So we read byte 0 from the EEPROM first, then use that as the dummy write afterwards.
// It wouldn't matter if this facility was used immediately before writing an EEPROM anyway ... but it DOES matter if you use this option
// in isolation (unprotecting the EEPROM but not changing it).

void SetSDPState(bool bWriteProtect)
{
  digitalWrite(kPin_LED_Red, HIGH);

  SetDataDirection(INPUT);

  byte bytezero = ReadByteFrom(0);

  SetDataDirection(OUTPUT);

  if (bWriteProtect)
  {
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0xA0);
  }
  else
  {
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0x80);
    WriteByteTo(0x1555, 0xAA);
    WriteByteTo(0x0AAA, 0x55);
    WriteByteTo(0x1555, 0x20);
  }

  WriteByteTo(0x0000, bytezero); // this "dummy" write is required so that the EEPROM will flush its buffer of commands.

  digitalWrite(kPin_LED_Red, LOW);

  Serial.print("OK SDP ");
  if (bWriteProtect)
  {
    Serial.println("enabled");
  }
  else
  {
    Serial.println("disabled");
  }
}

// ----------------------------------------------------------------------------------------

void ReadEEPROMIntoBuffer(unsigned char *buf, unsigned long addr, int size)
{
  // nCE and nOE LOW; nWE HIGH for reading
  digitalWrite(kPin_LED_Grn, HIGH);
  SetDataDirection(INPUT);

  for (int i = 0; i < size; i++)
  {
    buf[i] = ReadByteFrom(addr + i);
  }

  digitalWrite(kPin_LED_Grn, LOW);
}

void WriteBufferToEEPROM(unsigned char *buf, unsigned long addr, int size)
{
  // with OE high and CE low, pulse WE low to write current byte
  digitalWrite(kPin_LED_Red, HIGH);

  SetDataDirection(OUTPUT);

  for (int i=0; i < size; i++)
  {
    WriteByteTo(addr++, buf[i]);
    /*
     * we can latch multiple bytes for writing if we write each within 150us of the previous
     * but when we cross a 64-byte page boundary or write the final byte we need to wait for write cycle 
     * to start and complete (up to 10ms)
     */
    if ((addr & 0x3f) == 0 || i+1 == size) AwaitWriteComplete(buf[i]);
  }

  digitalWrite(kPin_LED_Red, LOW);
}

// ----------------------------------------------------------------------------------------

// this function assumes that data lines have already been set as INPUTS, and that
// nOE is set LOW.
byte ReadByteFrom(unsigned long addr)
{
  SetAddress(addr);
  delayMicroseconds(k_uTime_ReadPulse_uS);
  return ReadData();
}

// this function assumes that data lines have already been set as OUTPUTS, and that
// nOE is set HIGH.
void WriteByteTo(unsigned long addr, byte b)
{
  SetAddress(addr);
  SetData(b);

  // The address is latched on the falling edge of WE (or CE), whichever occurs last.
  // The data is latched by the first rising edge of WE (or CE).
  digitalWrite(kPin_nWE, LOW);  // pulse low to write byte  (falling edge)
  delayMicroseconds(k_uTime_WritePulse_uS);
  digitalWrite(kPin_nWE, HIGH); // end pulse (rising edge)
}

void AwaitWriteComplete(byte b) {
    /*
     * per the datasheet:
     * Once a byte write has been started it will automatically time itself to completion.
     * During the write cycle (max 10ms == 10000us), a read operation will effectively be a polling operation.
     * Reading the last byte written shows bit 7 complemented and bit 6 toggling until
     * the write cycle is done, so we can just wait for the full byte to match what we wrote
     */
    digitalWrite(kPin_LED_Grn, HIGH);
    SetDataDirection(INPUT);
    while (ReadData() != b) delayMicroseconds(100);
    SetDataDirection(OUTPUT);
    digitalWrite(kPin_LED_Grn, LOW);
}

// ----------------------------------------------------------------------------------------

// INPUT means we're reading from eeprom
void SetDataDirection(int direction)
{
  for (int i=0; i<8; i++) pinMode(dataPins[i], direction);
  // set eeprom to output when arduino wants to input, and vice versa
  digitalWrite(kPin_nOE, direction == INPUT ? LOW: HIGH);
  delayMicroseconds(1);
}

void SetAddress(unsigned long addr)
{
  for (int i=0, bit=1; i<15; i++, bit <<= 1) digitalWrite(addrPins[i], (addr & bit) ? HIGH: LOW);
}

// this function assumes that data lines have already been set as OUTPUTS.
void SetData(byte b)
{
  for (int i=0, bit=1; i<8; i++, bit <<= 1) digitalWrite(dataPins[i], (b & bit) ? HIGH: LOW);
}

// this function assumes that data lines have already been set as INPUTS.
byte ReadData()
{
  byte b = 0;
  for (int i=0, bit=1; i<8; i++, bit <<= 1) if (digitalRead(dataPins[i]) == HIGH) b |= bit;
  return b;
}

// ----------------------------------------------------------------------------------------

void PrintByte(byte b) 
{
    if (b < 16) Serial.print("0"); // force leading 0
    Serial.print(b, HEX);
}

void PrintBuffer(int size)
{
  uint8_t chk = 0, v;

  for (uint8_t i = 0; i < size; i++)
  {
    v = buffer[i];
    PrintByte(v);
    chk ^= v;
  }

  Serial.print(",");
  PrintByte(chk);
  Serial.println("");
}

void ReadString()     // read input excluding whitespace until \n into null-terminated command
{
  int i = 0;
  byte c;

  command[0] = 0;
  do {
    while (Serial.available()) {
      c = Serial.read();
      if (c > 31) command[i++] = c;
    }
  } while (c != 10 && i < kMaxCommandSize-1);
  command[i] = 0;
}

uint8_t CalcBufferChecksum(uint8_t size)
{
  uint8_t chk = 0;

  for (uint8_t i = 0; i < size; i++) chk ^= buffer[i];

  return chk;
}

