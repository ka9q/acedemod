/ $Header: /home/karn/ace_rcs/RCS/mmxbfly.s,v 1.2 2000/01/04 07:53:43 karn Exp $	
/ MMX implementation of Viterbi ACS butterflies for 64-state (k=7) code
/ Copyright 1999 Phil Karn, KA9Q
/ This code may be used under the terms of the GNU Public License

/ void mmxbfly(
/   unsigned char syms, /* input symbol pair, 0-15, in high/low nybbles	*/
/   unsigned char metrics[64], /* Incoming state metrics [0...63] (input) */
/   unsigned char nmetrics[64], /* Next state metrics [0...63] (output) */
/   unsigned char decisions[8]) /* ACS decisions (output), values 00/ff, order: */

/ 00 02 04 06 08 10 12 14
/ 01 03 05 07 09 11 13 15
/ 16 18 20 22 24 26 28 30
/ 17 19 21 23 25 27 29 31
/ 32 34 36 38 40 42 44 46
/ 33 35 37 39 41 43 45 47
/ 48 50 52 54 56 58 60 62
/ 49 51 53 55 57 59 61 63

.text	
.global mmxbfly
	.type mmxbfly,@function
	.align 16
mmxbfly:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ebx

	xorl %ebx,%ebx
	movb 8(%ebp),%bl	/ ebx = input symbols (low byte)
	movl 12(%ebp),%esi	/ esi->metrics
	movl 16(%ebp),%edi	/ edi->nmetrics
	movl 20(%ebp),%edx	/ edx->decisions
	pxor %mm7,%mm7		/ mm7 = 0 constant

/ invert both symbol nybbles into %eax according to the following map:
/ [0 1 ... 15] -> [15 15 ... 1]
/ note special case:	 0 maps to 15, nothing maps to 0
	movl %ebx,%eax
/	test $0xf,%eax		/ if(second_symbol == 0)
/	jnz  lownz
/	orl  $0x1,%eax		/	second_symbol = 1
/lownz:	test $0xf0,%eax		/ if(first_symbol == 0)
/	jnz  hinz
/	orl  $0x10,%eax		/	first_symbol = 1
/hinz:	xorl $0xff,%eax		/ compute 2s complement of both symbols
/	addl $0x11,%eax
	xorl $0xff,%eax		/ test for symmetric symbols 11/22

/ adjust for 8-byte element size
	shll $3,%eax
	shll $3,%ebx

/ add starting address of metric table	
	addl $Mettab,%eax
	addl $Mettab,%ebx

/ each invocation of this macro will do 8 butterflies in parallel
	.MACRO butterfly GROUP
/ load branch metrics into mm0-3
	movq (2048*\GROUP)(%ebx),%mm0	/ branch metric
	movq (2048*\GROUP)(%eax),%mm1	/ branch metric for inverted symbols
	movq ((8*\GROUP)+32)(%esi),%mm5	/ Incoming path metric, high bit = 1
	movq (8*\GROUP)(%esi),%mm4	/ Incoming path metric, high bit = 0
	movq %mm0,%mm3
	movq %mm1,%mm2

	paddb %mm5,%mm1
	paddb %mm5,%mm3	
	paddb %mm4,%mm0
	paddb %mm4,%mm2

/ Find survivors, leave decision flags in mm4,5
	movq %mm1,%mm4
	movq %mm3,%mm5	
	psubb %mm0,%mm4
	psubb %mm2,%mm5
	pcmpgtb %mm7,%mm4	/ ff -> 1-branch is better, 00-> 0-branch
	pcmpgtb %mm7,%mm5

/ stash decisions
/ We could use the punpck instructions to store these in proper order,
/ but it's quicker overall to save them permuted and adapt in the traceback
/ since there we examine only one decision for each bit anyway
	movq %mm4,(16*\GROUP)(%edx)
	movq %mm5,(16*\GROUP+8)(%edx)

/ select surviving path metrics, leave in mm1,3
	pand %mm4,%mm1		
	pandn %mm0,%mm4
	pand %mm5,%mm3
	pandn %mm2,%mm5
	por %mm4,%mm1
	por %mm5,%mm3

/ interleave and store new branch metrics	
	movq %mm1,%mm0
	punpckhbw %mm3,%mm1	/ interleave second 8 new metrics
	punpcklbw %mm3,%mm0	/ interleave first 8 new metrics
	movq %mm1,(16*\GROUP+8)(%edi)
	movq %mm0,(16*\GROUP)(%edi)
	.endm

/ invoke macro 4 times for a total of 32 butterflies
	butterfly GROUP=0
	butterfly GROUP=1
	butterfly GROUP=2
	butterfly GROUP=3

/ Note:	 emms is executed from C at the end of the frame
done:	popl %ebx
	popl %edx
	popl %edi
	popl %esi
	popl %ebp
	ret

/ unsigned char Mettab[4][256][8]; 
/ Branch metric lookup table, indexed by the state and input symbols
/ The first index is the loop number (states are done in groups of 8)
/ The second index is the input symbol pair as two adjacent nybbles
/ The third index is the state within the group of 8 done in parallel	
.global	Mettab
	.comm Mettab,8192,32

	.end

