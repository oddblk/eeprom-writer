#include "base64.h"

unsigned char _b64bits(unsigned char b) 
{
    unsigned char v = 0xff;

    if (b & 0x80) return v;

    if (b & 0x40) {    // letter?
        v = (b | 0x20) - 0x61;
        if (v >= 26) {
            v = 0xff;
        } else if (b & 0x20) {
            v += 26;
        }
    } else if (b >= 0x30 && b < 0x3a) {   // digit?
        v = b - 0x30 + 52;
    } else {
        switch (b) {
            case '+': 
            case '-':
                v = 62; break;
            case '/': 
            case '_':
            case ',':
                v = 63; break;
            case '=': 
                v = 0; break;
        }
    }
    return v;
}

void b64_init(B64Decoder *b64, void *in, int (*read)(void *in, unsigned char *buf, int n)) {
    b64->in = in;
    b64->read = read;
    b64->n = 0;
}

int b64_read(B64Decoder *b64, unsigned char *buf, int n) {
    int i, j, skip;
    unsigned char c, data[4];
    long packed;

    for (i=0; i<n; i++) {
        if (b64->n == 0) {
            j = b64->read(b64->in, data, 4);
            /* need at least two characters with implied traling = filler */
            if (j < 2) break;
            /* add filler = as needed */
            while (j < 4) data[j++] = '=';
            /* each trailing = drops one byte from the payload: ..== is 1 byte, ...= is 2, .... is 3 */
            for (j=2; j<4 && data[j] != '='; j++) /**/ ;
            skip = 4 - j;
            b64->n = 3 - skip;
            packed = 0;
            for (j=0; j<4; j++) {
                c = _b64bits(data[j]);
                if (c == 0xff) {
                    b64->n = 0;
                    break;
                }
                packed = (packed << 6) | c;
            }
            for (j=0; j<3; j++, packed >>= 8)
                if (j-skip >= 0) b64->stack[j-skip] = packed & 0xff;
        }
        buf[i] = b64->stack[--b64->n];
    }
    return i;
}
