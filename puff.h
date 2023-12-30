/* puff.h
  Copyright (C) 2002-2013 Mark Adler, all rights reserved
  version 2.3, 21 Jan 2013

  (modified to extract IO interface by Patrick Surry, Dec 2023)
  
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */

/*
 * See puff.c for purpose and usage.
 */
#ifndef NIL
#  define NIL ((unsigned char *)0)      /* for no output option */
#endif

typedef struct _zio {
    void *in;
    int (*read)(void *in, unsigned char *buf, unsigned n);

    void *out;
    int (*write)(void *out, const unsigned char *buf, unsigned n);
    int (*rewrite)(void *out, int n, unsigned dist);    /* return -1 if too far back, else bytes copied (expect n) */
} ZIO;

int puffs(ZIO *z, unsigned validate);
