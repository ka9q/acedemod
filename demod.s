/ $Header: /home/karn/ace_rcs/RCS/demod.s,v 1.3 2000/01/04 07:53:40 karn Exp $
/ Demodulate buffer containing biphase-encoded data with square-wave carrier
/ C declaration:
/ void demod(signed short samples[], /* Input samples, 16-bit signed ints */
/  unsigned long endsync,/* Sample index containing the end-of-frame sync */
/  long symbols[],	/* Demodulated output symbols */
/  long clock,		/* Starting clock phase (2^32 counts/symbol) */
/  long cstep);		/* Clock phase increment per input sample */

	.file	"demod.s"
.text
	.align 16
.globl demod
	.type	 demod,@function
demod:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ecx
	/ 8(%ebp) = samples
	/ 12(%ebp) = endsync
	/ 16(%ebp) = symbols
	/ 20(%ebp) = clock
	/ 24(%ebp) = cstep

	movl 8(%ebp),%esi	/esi = samples
	movl 16(%ebp),%edi	/edi = symbols
	movl 20(%ebp),%ebx	/ebx = clock
	xorl %ecx,%ecx		/ecx = i = 0

	xorl %edx,%edx
	movl 24(%ebp),%edi	/ edi = cstep
	test %ebx,%ebx
.skip:	jns  .pos		/ clock already zero or positive
	incl %ecx		/ skip leading samples on neg offset
	addl %edi,%ebx
	jmp  .skip

.nextsym:
	xorl %edx,%edx		/ edx = symbol = 0
	movl 24(%ebp),%edi	/ edi = cstep
	
/ first half of symbol
.pos:	movswl (%esi,%ecx,2),%eax  / eax = symbols[i]
	incl %ecx
	addl %eax,%edx
	addl %edi,%ebx		/ clock += cstep
	jns  .pos		/ until the clock sign bit sets
	
/ second half of symbol
.neg:	movswl (%esi,%ecx,2),%eax  / eax = symbols[i]
	incl %ecx
	subl %eax,%edx
	addl %edi,%ebx		/ clock += cstep
	jnc .neg		/ until the clock wraps
	
/ clock has wrapped, end of symbol
/ we test ecx here rather than in the sample loop to save time
/ it's possible to overrun the end of the sample buffer slightly,
/ but this seems like no big deal
	cmpl 12(%ebp),%ecx
	jg   .done
	movl 16(%ebp),%edi	/ edi = symbols
	movl %edx,(%edi)	/ *symbols++ = symbol
	addl $4,%edi
	movl %edi,16(%ebp)
	jmp  .nextsym

.done:	popl %ecx
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
