/* $Header: /home/karn/ace_rcs/RCS/mmxviterbi.c,v 1.3 2000/01/04 07:53:12 karn Exp $
 * Standard k=7 r=1/2 Viterbi decoder using Intel MMX assist
 * Copyright 1999 Phil Karn, KA9Q
 * May be used under the terms of the GNU Public License
 */
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include "viterbi.h"
int V_init;

/* Metric lookup table used by mmxbfly, initialized by v_init */
extern unsigned char Mettab[4][256][8];

/* Lookup table for permutating indices into ACS decisions */
static int Permtable[64];

extern char Partab[]; /* Byte parity lookup table */

#define	POLYA	0x6d
#define	POLYB	0x4f

/* Prototype for assembler code that does Viterbi butterflies with MMX */
void mmxbfly(
     unsigned char syms,
     unsigned char metrics[64],
     unsigned char nmetrics[64],
     unsigned char (*decisions)[64]);

/* Convolutional encoder for k=7, r=1/2 code
 * 4-bit output symbols are packed two to a byte, (first << 4) | second
 */
void
encode(unsigned char *syms,unsigned char *data,int nbytes,int encstate,int endstate)
{
  int i;

  /* Need to param this for CCSDS/NASA codes */
  while(nbytes-- != 0){
    for(i=7;i>=0;i--){
      encstate = (encstate << 1) | ((*data >> i) & 1);
      *syms = (Partab[encstate & POLYA] ? 15 : 0) << 4;
      *syms++ += (Partab[encstate & POLYB] ? 15 : 0);
    }
    data++;
  }
  /* Flush out tail */
  for(i=5;i>=0;i--){
    encstate = (encstate << 1) | ((endstate >> i) & 1);
    *syms =  (Partab[encstate & POLYA] ? 15 : 0) << 4;
    *syms++ += (Partab[encstate & POLYB] ? 15 : 0);
  }
}  

void
mmxviterbi27(
unsigned char *data,  /* Decoded output data */
unsigned char *symbols, /* Received symbols, two nybbles/byte */
int nbits,             /* Number of output bits */
int startstate,        /* Encoder starting state */
int endstate)          /* Encoder ending state */
{
  /* The following data structures are used by MMX instructions, so
   * they must all be aligned on 8-byte boundaries even though they
   * are arrays of characters. Calling malloc seems to be the only
   * way to guarantee alignment in an auto context; the aligned
   * attribute doesn't seem to work on auto variables in GCC
   */
#define USE_MALLOC
#ifdef USE_MALLOC
  unsigned char *metrics;
  unsigned char *nmetrics;
  unsigned char (*decisions)[64];
#else
  unsigned char metrics[64] __attribute__ ((aligned));
  unsigned char nmetrics[64] __attribute__ ((aligned));
  unsigned char decisions[nbits+6][64] __attribute__ ((aligned));
#endif
  int i;
  unsigned char *m1,*m2;

#if DEBUG
  int minmet,maxmet;
#endif

  if(V_init == 0)
    v_init(CCSDS);

#ifdef USE_MALLOC
  metrics = (unsigned char *)malloc(64*sizeof(unsigned char));
  nmetrics = (unsigned char *)malloc(64*sizeof(unsigned char));
  decisions = (unsigned char (*)[64])malloc(64*sizeof(unsigned char)*(nbits+6));
#endif

#if DEBUG
  fprintf(stderr,"metrics = %p, nmetrics = %p, decisions = %p\n",metrics,nmetrics,decisions);
#endif
#if DEBUG
  fprintf(stderr,"viterbi(%p,%p,%d,%d,%d\n",data,symbols,nbits,startstate,endstate);
#endif
  m1 = metrics;
  m2 = nmetrics;

  memset(metrics,0,64);
  metrics[startstate & 63] = 50; /* Bias known start state */

  /* Do add-compare-select butterflies */
  for(i=0;i<nbits+6;i++){
    unsigned char *tmp;
#if DEBUG
    int j;
#endif
    mmxbfly(symbols[i],m1,m2,&decisions[i]);
#if DEBUG
    fprintf(stderr,"i = %d; symbols = %02x\n",i,symbols[i]);
    minmet = maxmet = m2[0];
    for(j=1;j<64;j++){
      if((signed char)(m2[j] - maxmet) > 0)
	maxmet = m2[j];
      if((signed char)(m2[j] - minmet) < 0)
	minmet = m2[j];
    }
    fprintf(stderr,"metrics - min %d max %d range %d:\n",minmet,maxmet,(signed char)(maxmet-minmet));
    for(j=0;j<64;j++){
      fprintf(stderr," %3u",m2[j]);
      if((j % 16) == 15)
	fprintf(stderr,"\n");
    }
    fprintf(stderr,"decisions:\n");
    for(j=0;j<64;j++)
      fprintf(stderr,"%d",decisions[i][j] & 1);
    fprintf(stderr,"\n");
#endif
    /* Swap metrics for next set of butterflies */
    tmp = m1;
    m1 = m2;
    m2 = tmp;
  }
  asm("emms"); /* Done with MMX instructions for now */
  /* Perform chainback */
  {
  int byte = 0;
  int mask;

  nbits--;
  endstate &= 63;
  mask = 0x80 >> (nbits & 7);
  data += nbits/8;
  for(;nbits >= 0;nbits--){
    int k;

    /* The permutation is necessary because of the order in which
     * we store our ACS decisions in the MMX code
     */
    k = decisions[nbits+6][Permtable[endstate]];  /* FF or 00 */
    byte |= k & mask;
    endstate = (endstate >> 1) | (k & 32);
    if((mask <<= 1) == 0x100){
      *data-- = byte;
      mask = 1;
      byte = 0;
    }
  }
  }
#ifdef USE_MALLOC
  free(metrics);
  free(nmetrics);
  free(decisions);
#endif
}
/* Initialize Viterbi decoder tables */
void
v_init(enum vcode vcode)
{
  int state;
  int sym1,sym2,m,s1,s2;
  int tab[2][16] = {
#if 0
    /* Symmetric metric table used when an erasure symbol is needed */
    {+7,+7,+6,+5,+4,+3,+2,+1,0,-1,-2,-3,-4,-5,-6,-7},
    {-7,-7,-6,-5,-4,-3,-2,-1,0,+1,+2,+3,+4,+5,+6,+7}
#else
    /* Conventional symbol table, used when no erasure symbol is needed.
     * Performs better at too-low gain settings by degrading to hard
     * decision rather than erasing everything
     */
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15}
#endif
  };

  for(state=0;state < 32;state++){
    switch(vcode){
    case CCSDS:
      s1 = Partab[(2*state) & POLYB];
      s2 = !Partab[(2*state) & POLYA];
      break;
    case NASA:
      s1 = Partab[(2*state) & POLYA];
      s2 = Partab[(2*state) & POLYB];
      break;
    default:
      fprintf(stderr,"v_init: Unknown code %d\n",vcode);
      return;
    }
    for(sym1=0;sym1<16;sym1++){
      for(sym2=0;sym2<16;sym2++){
	m = tab[s1][sym1] + tab[s2][sym2];
	/* Scale down to ensure the metric spread at
	 * any decoder stage is never more than 128, to prevent
	 * wraparound errors. Since the free distance for this
	 * k=7 r=1/2 code is 10, the max spread is bounded by
	 * 10x the max branch metric
	 */
	Mettab[state >> 3][(sym1<<4) + sym2][state & 7] = m/2;
      }
    }
  }
  for(state=0;state<64;state++)
    Permtable[state] = ((state & 0xf) >> 1) + (state & ~0xf) +
      ((state & 1) << 3);

  V_init = 1;
}
