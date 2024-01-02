
typedef struct _b64decoder {
    void *in;
    int (*read)(void *in, unsigned char *buf, int n);

    unsigned char stack[3];   // stack of decoded bytes, with next at stack[n-1]
    int n;                    // available decoded bytes
} B64Decoder;


void b64_init(B64Decoder *b64, void *in, int (*read)(void *in, unsigned char *buf, int n));
int b64_read(B64Decoder *b64, unsigned char *buf, int n);
