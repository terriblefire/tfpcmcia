| __mulsi3 - 32-bit multiply for 68000
| unsigned long __mulsi3(unsigned long a, unsigned long b)
| a in d0, b in d1 (due to -mregparm=4)
| Actually with standard ABI, args are on stack:
| 4(sp) = a, 8(sp) = b, result in d0

	.globl	___mulsi3
___mulsi3:
	move.w	4(sp),d0	| a.hi
	mulu.w	8+2(sp),d0	| a.hi * b.lo
	move.w	8(sp),d1	| b.hi
	mulu.w	4+2(sp),d1	| b.hi * a.lo
	add.w	d1,d0
	swap	d0
	clr.w	d0
	move.w	4+2(sp),d1	| a.lo
	mulu.w	8+2(sp),d1	| a.lo * b.lo
	add.l	d1,d0
	rts
