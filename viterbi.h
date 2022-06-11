/* $Header: /home/karn/ace_rcs/RCS/viterbi.h,v 1.2 2000/01/04 07:53:27 karn Exp $ */
enum vcode { NASA,CCSDS };

void v_init(enum vcode vcode);

void encode27(unsigned char *syms,unsigned char *data,int nbytes,int startstate,int endstate);

void mmxviterbi27(
unsigned char *data,  /* Decoded output data */
unsigned char *symbols, /* Received symbols, two nybbles/byte */
int nbits,             /* Number of output bits */
int startstate,        /* Encoder starting state */
int endstate);          /* Encoder ending state */


