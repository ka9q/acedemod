/* $Header: /home/karn/ace_rcs/RCS/dotprod.h,v 1.2 2000/01/04 07:53:21 karn Exp $ */
#ifndef DOTPROD_H
#define DOTPROD_H

struct dotprod {
  int len; /* Number of coefficients */
  /* On a MMX machine, these hold 4 copies of the coefficients,
   * preshifted by 0,1,2,3 words to meet all possible input data
   * alignments (see Intel ap559 on MMX dot products)
   * On a non-MMX machine, only one copy is present
   */
  signed short *coeffs[4];
};
/* Create and return a descriptor for use with the dot product function */
void *initdp(signed short coeffs[],int len);

/* Free a dot product descriptor created earlier */
void freedp(struct dotprod *dp);

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long dotprod(struct dotprod *dp,signed short a[]);

#endif
