unsigned short add_mod_255(unsigned short s, unsigned char b) 
{
    s += b + 1;       // s is 0-254, v is 0-255
    if (s & 0x100) {  // overflow?
        s &= 0xff;    // s+v+1 > 255 <=> s+v > 254 so we remove 256 to get s+v+1-256 = s+v % 255
    } else {
        s--;          // otherwise just remove the test bit to get s+v+1-1 = s+v < 255
    }
    return s;
}

unsigned short fletcher16(unsigned char *buf, unsigned short n)
{
    unsigned short c0=0, c1=0;
    for (unsigned short i=0; i<n; i++) {
        c0 = add_mod_255(c0, buf[i]);
        c1 = add_mod_255(c1, c0);
    }
    return (c1 << 8) | c0;
}