/ $Header: /home/karn/ace_rcs/RCS/rdtsc.s,v 1.2 2000/01/04 07:53:49 karn Exp $
.text
.globl rdtsc
	.type rdtsc,@function
rdtsc:	rdtsc
	ret
