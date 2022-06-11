Advanced Composition Explorer (ACE) Real Time Solar Wind (RTSW) telemetry decoder

This is version 3.0.2 of the ACE spacecraft demodulator, released 3 Jan 2000
It is copyright 2000 by Phil Karn, KA9Q. It may be used under the terms of
the GNU Public License (GPL).

This version is a major revision from version 2.0. Here are the changes:

1. The MMX (multimedia extensions) instructions on the newer x86-class
machines are used to significantly speed up certain demodulator
operations, including sync vector search and Viterbi decoding. You can
determine if your CPU supports MMX instructions by looking at the
special system file /proc/cpuinfo under Linux. If the CPU supports MMX
you will see "mmx" listed in the line beginning with "flags".

On non-x86 systems, and on x86 systems without MMX support, C-language
implementations are used in place of the MMX routines.

Acedemod will note the availability of MMX on standard error at
startup.

2. Support for user-specified sampling rates (the -s or --sampling-rate
option).

3. Improved performance (1-2 dB estimated) at low Eb/No ratios. This comes
from several enhancements:

  a. Support for higher A/D sampling rates to allow for more precise
     determination of subcarrier phase during sync vector detection.

  b. If a frame does not fully decode, frame demodulation and decoding is
     repeated with increasing subcarrier clock phase offsets
     until the frame decodes or a retry limit is reached. This helps
     especially at lower sampling rates where the clock phase
     determined by the sync vector correlator is necessarily coarse.

  c. Use of a square wave subcarrier matching that used by the ACE spacecraft.
     Acedemod 2.0 used a sinusoidal subcarrier.

4. Improved decoding speed. In addition to the use of MMX (when available),
the manchester demodulator has been reimplemented in x86 assembler. (This
routine still accounts for fully half of the CPU time when operating at
a 48kHz sampling rate). Some unnecessary code has been removed.

5. Addition of new command line options. They are as follows:

   acedemod [--no-mmx|-m] [--debug #|-d #] [--full-frame|-f] [--no-mmx-vd|-v]
     [--sample rate #|-s #] [--no-offset|-o]

   --no-mmx (-m) disables all use of MMX even if it is available

   --debug (-d) specifies the debugging level. The default level is 1.

   --full-frame (-f) specifies that full frames including sync and RS parity
     symbols are to be output. The default is to output only the frame data.

   --no-mmx-vd (-v) forces use of the conventional (non-MMX) Viterbi decoder
     when MMX is available. The default is to use the MMX version of the
     Viterbi decoder if MMX instructions are available

   --sample-rate (-s) allows the specification of an A/D sample rate. The
     default is 9600 Hz. Higher sample rates permit more accurate determination
     of subcarrier phase and provide somewhat better performance at low Eb/No
     ratios, at the expense of greater CPU time for the demodulation step.

   --no-offset (-o) disables iterated decoding with subcarrier phase offsets
     on frames that don't decode on the first attempt

6. Gensig has also been updated for this release. A serious integer
   overflow bug has been fixed in the generation of low Eb/No test signals.
   This bug produced test signals with much lower Eb/No than specified.

   Gensig now takes the following command line options:

   gensig [-s sample_rate] [-a amplitude] [-f subcarrier frequency] [-e ebno]

