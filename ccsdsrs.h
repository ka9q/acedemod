/* $Header: /home/karn/ace_rcs/RCS/ccsdsrs.h,v 1.2 2000/01/04 07:53:19 karn Exp $ */
/* Global definitions for CCSDS error control coders
 * Phil Karn KA9Q, December 1998
 */

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
 * the number of corrected symbols. If the codeword is illegal or
 * uncorrectible, the data array is unchanged and -1 is returned
 */
int eras_dec_rs(dtype data[], int eras_pos[], int no_eras);

/* Conversion lookup tables from conventional alpha to Berlekamp's
 * dual-basis representation.
 * taltab[] -- convert conventional to dual basis
 * tal1tab[] -- convert dual basis to conventional

 * Note: the RS encoder/decoder works with the conventional basis.
 * To encode/decode CCSDS data, you must run the inputs to either
 * the encoder or decoder through tal1tab[] and the outputs through
 * taltab[].
 */
extern unsigned char taltab[256],tal1tab[256];
