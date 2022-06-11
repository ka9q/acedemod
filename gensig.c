/* $Header: /home/karn/ace_rcs/RCS/gensig.c,v 1.3 2000/01/04 07:53:05 karn Exp $
 * Generate synthetic test signal for ACE demodulator
 * Usage: gensig [-e ebno] < input file
 * Input is data to be transmitted
 * Output is 16-bit 2's complement samples
 * Can be piped directly into acedemod
 * Copyright 1999 Phil Karn, KA9Q
 * May be used under the terms of the GNU Public License
 */

#include <stdio.h>
#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "ccsds.h"
#include "modem.h"
#include "ace.h"

#define LEVEL 500
#define DATASIZE (INTERLEAVE * (KK-RSPAD))
#define RSSIZE ((INTERLEAVE * (NN-RSPAD)) + (SYNC_LENGTH/8))

#define E32 4294967296LL

long Clock,Cstep;
double Samprate = 9600;
double Noise = 0;
double Signal_energy,Noise_energy;
double Amplitude = 100; /* Avoid 16-bit saturation at low Eb/No */

void putbyte(int),putbit(int),putsym(int);
double normal_rand(double mean, double std_dev);

int
main(int argc,char *argv[])
{
  dtype rsbuf[INTERLEAVE][NN];
  int i,d,j;
  time_t t;
  double ebn0 = -1;
  extern char *optarg;
  double freq = 996;
  double ecn0;

  time(&t);
  srandom(t);
  init_rs();
  memset(rsbuf,0,sizeof(rsbuf));

  Clock = 0;

  while((d = getopt(argc,argv,"a:e:f:s:")) != EOF){
    switch(d){
    case 's':
      Samprate = atof(optarg);
      break;
    case 'a':
      Amplitude = atof(optarg);
      break;
    case 'f':
      freq = atof(optarg);
      break;
    case 'e': /* Specify this last */
      ebn0 = pow(10.,atof(optarg)/10.);
      break;
    }
  }
  if(ebn0 != -1){
    ecn0 = ebn0 * BITRATE / Samprate;
    Noise = Amplitude * sqrt(0.5 /ecn0);
  }
  Cstep = E32 * (freq/Samprate);
  fprintf(stderr,"Amplitude = %f, freq = %f (Cstep = %ld), Noise = %f\n",
	  Amplitude,freq,Cstep,Noise);
  while(!feof(stdin)){
    /* Send sync vector */
    for(i=SYNC_LENGTH-8;i>= 0;i-=8)
      putbyte(SYNC_WORD >> i);

    /* Fill interleaver & send data */
    d = 0;
    for(j=RSPAD;j<KK;j++){
      for(i=0;i<INTERLEAVE;i++){
	d = getchar();
	if(d == EOF)
	  d = 0;
	rsbuf[i][j] = d;
	putbyte(d);
      }
    }
    /* Generate RS parity bytes */
    for(i=0;i<INTERLEAVE;i++)
      encode_rs(&rsbuf[i][0],&rsbuf[i][KK]);

    /* Transmit RS parity bytes*/
    for(j=KK;j<NN;j++){
      for(i=0;i<INTERLEAVE;i++){
	putbyte(rsbuf[i][j]);
      }
    }
  }
  /* Send final sync vector and flush the encoder */
  for(i=SYNC_LENGTH-8;i>= 0;i-=8)
    putbyte(SYNC_WORD >> i);
  for(i=0;i<4;i++)
    putbyte(0);
  fprintf(stderr,"SNR %.2f dB Eb/No %.2f\n",
	  10*log10(Signal_energy/Noise_energy),
	  10*log10(Samprate/(2.*BITRATE)*Signal_energy/Noise_energy));
  exit(0);
}

void
putbyte(int byte)
{
  int i;

  for(i=7;i >= 0; i--)
    putbit((byte >> i) & 1);
}

void
putbit(int bit)
{
  static sr = 0;
  sr = (sr << 1) | bit;
  putsym(Partab[sr & POLYB]); /* Second polynomial first */
  putsym(!Partab[sr & POLYA]);/* First polynomial second, inverted */
}

void
putsym(int bit)
{
  double sig,noise;
  signed short s;
  long oy;
  static int symcnt = 0;

  /* Convert 0/1 to -1/+1 */
  bit = 2*bit - 1;
  do {
    sig = Amplitude * (Clock >= 0 ? bit : -bit);
#if 0
    fprintf(stderr,"Clock = %ld sig = %.0f\n",Clock,sig);
#endif
    Signal_energy += sig * sig;
    if(Noise != 0){
      noise = normal_rand(0., Noise);
      Noise_energy += noise * noise;
    } else
      noise = 0;
    sig += noise;
    s = (sig > 32767) ? 32767 : (sig < -32768 ? -32768 : sig);
    putchar(s); putchar(s >> 8);
    oy = Clock;
    Clock += Cstep;
  } while(oy >= 0 || Clock <= 0);
  symcnt++;
}
#define	MAX_RANDOM	0x7fffffff

/* Generate gaussian random double with specified mean and std_dev */
double
normal_rand(double mean, double std_dev)
{
  double fac,rsq,v1,v2;
  static double gset;
  static int iset;

  if(iset){
    /* Already got one */
    iset = 0;
    return mean + std_dev*gset;
  }
  /* Generate two evenly distributed numbers between -1 and +1
   * that are inside the unit circle
   */
  do {
    v1 = 2.0 * (double)random() / MAX_RANDOM - 1;
    v2 = 2.0 * (double)random() / MAX_RANDOM - 1;
    rsq = v1*v1 + v2*v2;
  } while(rsq >= 1.0 || rsq == 0.0);
  fac = sqrt(-2.0*log(rsq)/rsq);
  gset = v1*fac;
  iset = 1;
  return mean + std_dev*v2*fac;
}
