/* $Header: /home/karn/ace_rcs/RCS/modem.h,v 1.3 2000/01/04 07:53:24 karn Exp $ */
#ifdef i386
#define sincos(x,s,c) { asm ("fsincos" : "=t" (*c), "=u" (*s) : "0" (x)); }
#else
#define sincos(x,s,c) { *s = sin(x); *c = cos(x); }
#endif



