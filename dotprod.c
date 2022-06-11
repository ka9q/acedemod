/* $Header: /home/karn/ace_rcs/RCS/dotprod.c,v 1.3 2000/01/04 07:53:02 karn Exp $
 * 16-bit signed integer dot product
 * Use MMX assist if running on a MMX-enabled machine,
 * else use portable C code
 * Copyright 1999 Phil Karn
 * May be used under the terms of the GNU public license
 */
#include <stdio.h>
#include <malloc.h>
#include "dotprod.h"

#ifdef i386
long mmxdotprod(short *a,short *b,int cnt);
extern int Mmx;
#endif

extern int Debug;

/* Create and return a descriptor for use with the dot product function */
void *
initdp(signed short coeffs[],int len)
{
  struct dotprod *dp;
  int j;

  dp = (struct dotprod *)calloc(1,sizeof(struct dotprod));
  dp->len = len;

#ifdef i386
  if(Mmx){
    int i;
    for(i=0;i<4;i++){
      /* Round (len+i) up to nearest multiple of 4 */
      int blen;
      blen = (len+i+3) & ~3;
      dp->coeffs[i] = (signed short *)calloc(blen,sizeof(signed short));
      for(j=0;j<len;j++)
	dp->coeffs[i][j+i] = coeffs[j];
    }
  } else
#endif
    {
    dp->coeffs[0] =  (signed short *)calloc(len,sizeof(signed short));
    for(j=0;j<len;j++)
      dp->coeffs[0][j] = coeffs[j];
  }
  return (void *)dp;
}

/* Free a dot product descriptor created earlier */
void
freedp(struct dotprod *dp)
{
  int i;

  for(i=0;i<4;i++)
    if(dp->coeffs[i])
      free(dp->coeffs[i]);
  free(dp);
}

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long
dotprod(struct dotprod *dp,signed short a[])
{
#ifdef i386
  if(Mmx){
    /* Set up for MMX assist */
    int al,blen;
    signed short *ar;

    /* Round input data address down to 8 byte boundary
     * NB: depending on the alignment of a[], up to 6 bytes of memory
     * before a[] will be accessed. The contents don't matter since they'll
     * be multiplied by zero coefficients. I can't conceive of any
     * situation where this could cause a segfault since memory protection
     * in the x86 machines is done on much larger boundaries than 8 bytes
     */
    ar = (signed short *)((int)a & ~7);

    /* Choose one of 4 sets of pre-shifted coefficients. al is both the
     * index into dp->coeffs[] and the number of 0 words padded onto
     * that coefficients array for alignment purposes
     */
    al = a - ar;

    /* Round (dp->len+al) up to nearest multiple of 4 words */
    blen = (dp->len+al+3) & ~3;

    return mmxdotprod(ar,dp->coeffs[al],blen);
  } else
#endif
    {
    /* conventional dot-product in C */
    long corr = 0;
    long cnt = dp->len;
    signed short *cp = dp->coeffs[0];
    while(cnt-- != 0)
      corr += (long)*a++ * *cp++;
    return corr;
  }
}

