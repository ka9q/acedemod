/* $Header: /home/karn/ace_rcs/RCS/acedemod.c,v 1.6 2000/01/04 07:52:53 karn Exp $
 * ACE low-speed telemetry demodulation and decoding v2.0
 * Performs bit clock recovery, demodulation, Viterbi decoding and RS decoding

 * Expects standard input to be baseband receiver audio
 * sampled at 9600 Hz, 16 bits/sample, low byte first.

 * Can read from the Linux "brec" command, e.g.,
 * brec -s 9600 -b 16 | acedemod

 * Command line options:
 * -s sample_rate (default 9600 Hz)
 * -m disable automatic use of MMX in the correlator (x86 machines only)
 * -f output full frames, including frame sync and RS parities
 *    default is to output data only
 * -d debug_level
 *    Debug level 0 - silent
 *    Debug level 1 - 1-line status summary to stderr per frame
 *
 * Copyright 1999 Phil Karn, KA9Q
 * This software may be used under the terms of the GNU Public License
 */


#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include "modem.h"
#include "ccsds.h"
#include "dotprod.h"
#include "viterbi.h"
#include "ace.h"

/* Circular buffer of incoming A/D samples */
struct sampbuf {
  signed short *samples;
  int cnt;  /* Count of samples in buffer */
};

#define min(a,b) ((a) < (b) ? (a) : (b))

struct dotprod *Dp;
int RS_histogram[18];
int Debug = 1; /* Default to moderate level */
int Full = 0; /* When set, output full frame including sync and RS parities */
double Samprate = 9600;
int Corrlen;
int Mmx; /* Control use of MMX instructions (x86 machines only) */
int Sbufsize;
int Offset = 1; /* Enable clock offset searching */

unsigned long correlate(long *m,struct sampbuf *s,unsigned long start,unsigned long finish);
void init_corr(void);
void terminate(void);
void replenish(struct sampbuf *s,int amount);
void purge(struct sampbuf *s,int amount);
int demod(signed short *samples,unsigned long endsync,long *symbols,
  long clock,long cstep);

#define E32 4294967296LL /* 2^32, as a long long int */

#ifndef __i386__
int mmxavail(void) { return 0; }
#endif

int
main(int argc,char *argv[])
{
  extern char *optarg;
  int option_index = 0;
  extern char *optarg;
  extern int optind;
  int trial,j,synchronized = 0;
  double gain;
  long i,maxcorr,endsync,cstep,offset;
  struct sampbuf sampbuf;
  unsigned long frame,symno;
  long symbols[SYMFRAME];
  unsigned char vdsyms[SYMFRAME];
  unsigned char data[SYMFRAME/(2*8)];
  long metric;
  int mettab[2][256];	/* Viterbi metric table, [sent sym][rx symbol] (non MMX version) */
  unsigned char rsblocks[INTERLEAVE][NN];/* Reed-Solomon code blocks */
  int rs_errs[INTERLEAVE];
  int rs_errlocs[INTERLEAVE][NN-KK];
  int blks_left;
  int mmxavail(void);
  static struct option long_options[] =
  {
    {"no-mmx", 0, NULL, 'm' },      /* Disable all use of MMX */
    {"debug", 1, NULL, 'd' },       /* Specify debug level */
    {"full-frame", 0, NULL, 'f' },  /* Output full frame (sync, data, RS parities) */
    {"no-mmx-vd", 0, NULL, 'v' },   /* Don't use MMX viterbi decoder */
    {"sample-rate", 1, NULL, 's' }, /* Sample rate */
    {"no-offset", 0, NULL, 'o' },   /* Disable clock offset searching */
    {0, 0, NULL, 0 }
  };

  atexit(terminate);
  signal(SIGTERM,exit);
  signal(SIGQUIT,exit);
  signal(SIGINT,exit);
  Samprate = 9600;

  Mmx = mmxavail() ? 2 : 0;
  if(Mmx && Debug > 0)
    fprintf(stderr,"MMX available\n");

  while((i = getopt_long(argc,argv,"mfd:vs:o",long_options,&option_index))!= EOF){
    switch(i){
    case 'm':
      if(Mmx){
	Mmx = 0; /* Disable all use of MMX */
	fprintf(stderr,"MMX manually disabled\n");
      } else {
	fprintf(stderr,"MMX already disabled\n");
      }
      break;
    case 'v':
      if(Mmx){
	Mmx = 1; /* Disable use of MMX viterbi decoder */
	fprintf(stderr,"MMX Viterbi Decoder disabled\n");
      } else {
	fprintf(stderr,"MMX already disabled\n");
      }
      break;
    case 'f':
      Full++;
      break;
    case 'd':
      Debug = atoi(optarg);
      break;
    case 's':
      Samprate = atof(optarg);
      break;
    default:
      break;
    }
  }
  /* Length of sync correlator local replica, in samples
   * ~= (length of SYNC_WORD (32 bits)
   * - conv encoder constraint length (k=7))
   * 1/conv encoder rate (1/(1/2) == 2)
   * Samprate/SYMCLOCK (about 10 for 9600 samp/sec) = 500
   * Round up to next multiple of 4 (8 bytes) for MMX 
   */
  Corrlen = (int)(50*Samprate/SYMCLOCK + 4) & ~3;

  /* Set up input sample buffer */
  /* Pick a better way of extending this than an arbitrary 1 second */
  Sbufsize = Samprate * (FRAMETIME + 1);
  sampbuf.samples = malloc(sizeof(unsigned short) * Sbufsize);
  sampbuf.cnt = 0;

  init_corr();/* Initialize sync vector correlator */

  switch(Mmx){
#ifdef __i386__
  case 2:
    v_init(CCSDS); /* Initialize MMX viterbi decoder for CCSDS polys */
    break;
#endif
  default:
    /* Set up Viterbi decoder metric table for Es/No of about -1.1 dB,
     * which corresponds to a Eb/No of 2.5 dB
     */
    gen_met(mettab,16,1.29,0.0,100);
    break;
  }

  init_rs(); /* Initialize Reed-Solomon tables */
  memset(rsblocks,0,sizeof(rsblocks));

  for(frame=0;;frame++){
    if(!synchronized){
      /* When not synchronized, search an entire 16 second window for sync
       * and slide it up to the front of the buffer
       * This is a pretty CPU-intensive operation, so we assist it
       * with a dot product in MMX code, if available
       */
      purge(&sampbuf,correlate(&maxcorr,&sampbuf,0,Samprate*FRAMETIME));
    }
    /* Now look ahead 16 sec +/- 200 samples for the next sync */
    endsync = correlate(&maxcorr,&sampbuf,(Samprate*FRAMETIME) - 200,(Samprate*FRAMETIME) + 200);
    cstep = E32 * SYMFRAME/endsync; /* Clock phase increment per sample */

    /* Mark all RS code blocks as undecoded */
    for(i=0;i<INTERLEAVE;i++)
      rs_errs[i] = -1;

    /* Retry the frame with increasing alternating carrier phase offsets
     * until all RS code words decode or we run out of trials
     */
    blks_left = INTERLEAVE;
    for(trial=0;blks_left != 0;trial++){
      if(trial & 1)
	offset = -.002 * E32 * trial/2;
      else
	offset = +.002 * E32 * trial/2;
      
      if(abs(offset) >= E32 * SYMCLOCK/(2*Samprate))
	break; /* Give up */

      /* Demodulate the frame by interpolating the clock between the syncs */
      demod(sampbuf.samples,endsync,symbols,offset,cstep);
      
      /* Scale to Viterbi decoder input and Viterbi decode frame
       * starting with the first symbol after the sync word
       * We're framed by the known last 7 bits of the leading sync and the
       * first 7 bits of the trailing sync, so pin those states in the decoder.
       */
      symno = 0;
#ifdef __i386__
      if(Mmx == 2){
	/* Use MMX viterbi decoder
	 * The 5.8 factor was found by experiment to minimize the RS block
	 * erasure rate at Eb/No = 2.3 dB
	 */
	/* Symbol polarity is indicated by sign of correlator peak */
	gain = 5.8 /(maxcorr/50); /* For scaling Viterbi soft-decision symbols */
	/* Shift to offset-8, clip, and put in VD input buffer */
	for(i = 0;i<SYMFRAME;i++){
	  int s;
	  s = (symbols[i] * gain) + 8;
	  s = s > 15 ? 15 : (s < 0 ? 0 : s);
	  if(i & 1){
	    vdsyms[symno++/2] |= s;
	  } else
	    vdsyms[symno++/2] = s << 4;
	}
	metric = 0; /* Not set by MMX version */
	mmxviterbi27(data,vdsyms+26,SYMFRAME/2-SYNC_LENGTH,SYNC_WORD & 0x3f,
		     (SYNC_WORD >> 26) & 0x3f);
      } else
#endif
	{
	  /* Use conventional VD */
	  gain = 25.0 /(maxcorr/50);
	  /* Scale and quantize to offset-128, rounding to nearest integer */
	  for(i = 0;i < SYMFRAME;i++){
	    int s;
	    s = (symbols[symno] * gain) + 128.5;
	    s = s > 255 ? 255 : (s < 0 ? 0 : s);
	    vdsyms[symno++] = s;
	  }
	  viterbi27(&metric,data,vdsyms+52,SYMFRAME/2-SYNC_LENGTH,mettab,SYNC_WORD & 0x3f,
		    (SYNC_WORD >> 26) & 0x3f);
	}
      
      /* De-interleave into RS code blocks and decode */
      for(i=0;i<INTERLEAVE;i++){
	if(rs_errs[i] != -1)
	  continue; /* Block already decoded on previous pass */
	memset(rsblocks[i],0,RSPAD);
	for(j=0;j<RSDATA;j++)
	  rsblocks[i][RSPAD+j] = data[j*INTERLEAVE + i];
	if((rs_errs[i] = eras_dec_rs(rsblocks[i],rs_errlocs[i],0)) != -1)
	  blks_left--;
      }
      synchronized = (blks_left != INTERLEAVE);
      if(!Offset)
	break; /* Offset searching disabled, break unconditionally */
    }
    if(Debug > 1){
      for(i=0;i<INTERLEAVE;i++){
	if(rs_errs[i] <= 0)
	  continue;
	fprintf(stderr,"rs%ld:",i);
	for(j=0;j<rs_errs[i];j++)
	  fprintf(stderr," %d",rs_errlocs[i][j]);
	fprintf(stderr,"\n");
      }
    }
    /* Tally up final RS statistics for this frame */
    for(i=0;i<INTERLEAVE;i++)
      RS_histogram[rs_errs[i] + 1]++;
    
    if(Debug >= 1){
      if(metric != 0)
	fprintf(stderr,"Frame %lu offset %ld corr %ld freq %.2f VD metric %ld RS errors",
	      frame,offset,maxcorr,(double)cstep*Samprate/E32,metric);
      else
	fprintf(stderr,"Frame %lu offset %ld corr %ld freq %.2f RS errors",
	      frame,offset,maxcorr,(double)cstep*Samprate/E32);
      for(i=0;i<INTERLEAVE;i++)
	fprintf(stderr," %d",rs_errs[i]);
      fprintf(stderr,"\n");
    }

    if(blks_left == 0){
      /* The frame is good. Output it. */
      /* INSERT CODE HERE TO OUTPUT DECODED DATA IN DESIRED FORMAT
       * Corrected data, including RS parity, is in the 2-dimensional array
       *      rsblocks[0...3][RSPAD...NN]
       * To undo the interleaving, vary the first subscript first
       * Use the default code immediately below as a model
       */
      if(Full){ /* Include sync vector on each frame */
	for(i=3;i>=0;i--)
	  putchar(SYNC_WORD >> (8*i));
      }
      for(j=RSPAD;j<(Full?NN:KK);j++)
	for(i=0;i<INTERLEAVE;i++)
	  putchar(rsblocks[i][j]);
    }
    purge(&sampbuf,endsync);
  }
  /* Not reached */
  exit(0);
}

/* Search the specified window in the current sample buffer, advancing
 * by the specified step between dot products, for the biggest
 * correlation peak (absolute value) with the local replica of the encoded,
 * modulated sync vector
 * 
 * The offset of this peak is the return value, and the amplitude of
 * the peak is returned through 'm' (if non-NULL).
 */
unsigned long
subcorrelate(long *m,struct sampbuf *s,unsigned long start,unsigned long finish,long step)
{
  unsigned long i,offset = 0;
  long corr,maxcorr = 0;

  if(finish+Corrlen > s->cnt)
    replenish(s,finish+Corrlen);

  /* Should also implement Massey's data-energy adjustment (IEEE 1972) */
  for(i=start;i<finish;i += step){
    corr = dotprod(Dp,&s->samples[i]);
    if(abs(corr) > abs(maxcorr)){
      maxcorr = corr;
      offset = i;
    }
  }
  if(m != NULL)
    *m = maxcorr; 
  return offset;
}

/* First do a coarse search, stepping 2 samples at a time; then
 * refine the search
 */
unsigned long
correlate(long *m,struct sampbuf *s,unsigned long start,unsigned long finish)
{
  unsigned long offset1,offset2;
  long coarsestep;

  coarsestep = Samprate/(SYMCLOCK*5);
  /* Coarse search, 5 points/symbol */
  offset1 = subcorrelate(m,s,start,finish,coarsestep);

  /* Fine search */
  offset2 = subcorrelate(m,s,offset1-coarsestep,offset1+coarsestep,1);
  return offset2;
}

/* Generate local replica of convolutionally
 * encoded & modulated sync marker for correlator
 */
void
init_corr()
{
  unsigned int sr;
  int sp,i,syms[50];
  signed short encoded_sync[Corrlen]; /* Local replica of encoded, modulated sync vector */
  long oy,clock,cstep;

  memset(encoded_sync,0,sizeof(encoded_sync));
  cstep = E32 * SYMCLOCK/Samprate; /* Clock wraps once per cycle */

  /* Convolutionally encode the last 25 bits of the sync vector,
   * starting with the first part of it in the encoder register
   */
  sr = SYNC_WORD >> 25;
  for(sp=0,i=24;i>=0;i--){
    syms[sp++] = Partab[sr & POLYB];    /* CCSDS puts second symbol first */
    syms[sp++] = !Partab[sr & POLYA];   /* then first symbol inverted */
    sr = (sr << 1) | ((SYNC_WORD >> i) & 1);    
  }
  /* Manchester encode the convolutionally encoded symbols:
   * for the first half of the clock cycle, emit the symbol as +/-1;
   * for the second half, emit it inverted
   */
  clock = 0;
  sp = 0;
  i = 0;
  do {
    encoded_sync[sp++] = 1 - 2*((clock >= 0) ^ syms[i]);
    oy = clock;
    clock += cstep;
    /* When clock crosses 0 going positive, increment the symbol
     * pointer. When this hits 50, quit
     */
    if(clock >= 0 && oy < 0)
      i++; /* Clock just crossed zero positive going, start next bit */
  } while(i < 50);
  Dp = initdp(encoded_sync,sp);
}

/* Replenish sample buffer with new data so we have at least
 *   "amount" samples in it
 */
void
replenish(struct sampbuf *s,int amount)
{
  int cnt = min(amount,Sbufsize) - s->cnt;

  if(cnt > 0){
    if(fread(s->samples+s->cnt,sizeof(signed short),cnt,stdin) != cnt)
      exit(0); /* EOF on input */
    s->cnt += cnt;
  }
}

/* Discard old sample data from front of sample buffer */
void
purge(struct sampbuf *s,int amount)
{
  if(amount > s->cnt)
    amount = s->cnt;
  s->cnt -= amount;
  memmove(s->samples,s->samples+amount,sizeof(signed short)*s->cnt);
}


/* Called at termination to dump RS correction statistics */
void
terminate(void)
{
  int i;
  int total;
  
  if(Debug == 0)
    return;

  fprintf(stderr,"RS error summary:\n");
  total = RS_histogram[0];
  if(RS_histogram[0] != 0)
    fprintf(stderr,"Failed: %d\n",RS_histogram[0]);
  for(i=1;i<=17;i++){
    if(RS_histogram[i] != 0){
      total += RS_histogram[i];
      fprintf(stderr,"%d: %d\n",i-1,RS_histogram[i]);
    }
  }  
  fprintf(stderr,"total RS frames %d failed RS frames %d (%.1f%%)\n",
	  total,RS_histogram[0],100.*RS_histogram[0]/total);
}
