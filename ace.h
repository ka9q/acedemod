/* $Header: /home/karn/ace_rcs/RCS/ace.h,v 1.2 2000/01/04 07:49:17 karn Exp $ */
/* ACE signal constants */
#define SYMCLOCK 996.0   /* Nominal symbol clock rate, Hz */
#define FRAMETIME 16.0   /* seconds/frame, including sync */
#define SYMFRAME 15936   /* Channel symbols per frame, including sync */
#define RSDATA  248      /* Number of RS symbols per codeword */
#define RSPAD (255-RSDATA) /* Pad bytes at beginning of each RS codeword */
#define INTERLEAVE 4     /* Inter-coder interleaver depth */
#define BITRATE 434. /* User data rate without coding */
