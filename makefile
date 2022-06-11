# $Header: /home/karn/ace_rcs/RCS/makefile,v 1.4 2000/01/04 07:54:02 karn Exp $
# comment out the next line on non-Intel machines
INTEL_OBJS=mmxdotprod.o mmxviterbi.o mmxbfly.o rdtsc.o demod.o
# comment out the next line on Intel machines
#NON_INTEL_OBJS=demod_c.o

CFLAGS= -Wall -g -O9

all: acedemod gensig

gensig: gensig.o acelib.a
	gcc -g -o gensig gensig.o acelib.a -lm

acedemod: acedemod.o acelib.a
	gcc -g -o acedemod acedemod.o acelib.a -lm

acelib.a: ccsdsrs.o ccsds_viterbi.o dotprod.o tab.o metrics.o $(INTEL_OBJS) $(NON_INTEL_OBJS)
	ar rv acelib.a ccsdsrs.o ccsds_viterbi.o dotprod.o tab.o metrics.o $(INTEL_OBJS) $(NON_INTEL_OBJS)
	ranlib acelib.a

acedemod.o: acedemod.c modem.h ccsds.h dotprod.h viterbi.h ace.h
gensig.o: gensig.c ccsds.h modem.h ace.h
ccsdsrs.o: ccsdsrs.c ccsdsrs.h
ccsds_viterbi.o: ccsds_viterbi.c viterbi27.h
dotprod.o: dotprod.c dotprod.h

clean:
	rm -f *.o gensig acedemod acelib.a



