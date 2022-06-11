/* $Header: /home/karn/ace_rcs/RCS/ccsds.h,v 1.3 2000/01/04 07:53:17 karn Exp $
 * Global definitions for CCSDS encoding and error control coders
 * Phil Karn KA9Q, December 1998
 */

#define SYNC_WORD 0x1acffc1d /* 32-bit frame sync vector */
#define SYNC_LENGTH 32

/* (255,223) Reed-Solomon code */

/* Don't change these in this version */
#define MM  8		/* RS code over GF(2**MM) */
#define KK  223		/* KK = number of information symbols */
#define	NN ((1 << MM) - 1)

typedef unsigned char dtype;

/* Initialization function -- call this before doing anything else */
void init_rs(void);

/* Reed-Solomon encoding
 * data[] is the input block, parity symbols are placed in bb[]
 * bb[] may lie past the end of the data, e.g., for (255,223):
 *	encode_rs(&data[0],&data[223]);
 */
int encode_rs(dtype data[], dtype bb[]);

/* Reed-Solomon erasures-and-errors decoding
 * The received block goes into data[], and a list of zero-origin
 * erasure positions, if any, goes in eras_pos[] with a count in no_eras.
 *
 * The decoder corrects the symbols in place, if possible and returns
 * the number of corrected symbols. If eras_pos is non-NULL, their positions
 * are returned there. If the codeword is illegal or
 * uncorrectible, the data array is unchanged and -1 is returned
 */
int eras_dec_rs(dtype data[], int eras_pos[], int no_eras);

/* r=1/2 k=7 convolutional encoder polynomials - used in the order
   POLYB, POLYA by the CCSDS standard */
#define	POLYA	0x6d
#define	POLYB	0x4f

/* batch-mode r=1/2 k=7 Viterbi decoder */
int
viterbi27(
long *metric,	/* Final path metric (returned value) */
unsigned char *data,	/* Decoded output data */
unsigned char *symbols,	/* Raw deinterleaved input symbols */
unsigned int nbits,	/* Number of output bits */
int mettab[2][256],	/* Metric table, [sent sym][rx symbol] */
int startstate,         /* Encoder starting state */
int endstate            /* Encoder terminating state */
);

/* Viterbi decoder metric table generator */
void
gen_met(
int mettab[2][256],	/* Metric table, [sent sym][rx symbol] */
int amp,		/* Signal amplitude, units */
double noise,		/* Relative noise voltage */
double bias,		/* Metric bias; 0 for viterbi, rate for sequential */
int scale		/* Scale factor */
);


extern unsigned char Partab[]; /* Byte parity lookup table */
