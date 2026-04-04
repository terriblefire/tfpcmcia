; TerribleFire PCMCIA boot ROM
;
; Assembled with: vasmm68k_mot -Fbin -no-opt -o tfpcmcia.rom tfpcmcia_rom.s
;
; This ROM lives at $600000 (PCMCIA common memory) and is executed via
; Amiga XIP (Execute-In-Place). Kickstart's card.resource finds the
; CISTPL_AMIGAXIP tuple in attribute memory, which points at the RomTag
; here. InitResident() is called with A6=ExecBase, D0=0.
;
; The init code:
;   1. Sets 9600 baud serial, prints "TF\n"
;   2. Loads the embedded tfpcmcia.device (Amiga hunk executable) from ROM
;      into Amiga RAM (parse hunks, allocate, copy, relocate)
;   3. Scans the loaded code for a RomTag
;   4. Calls InitResident() to register the device driver
;   5. Switches the board from boot ROM mode to SPIRAM mode

BASE		equ	$600000

; Amiga hardware registers
SERDAT		equ	$DFF030
SERPER		equ	$DFF032
SERDATR		equ	$DFF018

; PCMCIA board registers (attribute memory, even byte addresses)
SPI_DATA	equ	$A20200
SPI_CS		equ	$A20202
SPI_STATUS	equ	$A20204
BOARD_CTRL	equ	$A20206

; Serial constants
SERPER_9600	equ	$0173		; 9600 baud for PAL (3546895 / 9600 - 1)

TBE_BIT		equ	5		; bit 5 of SERDATR high byte = bit 13 (TBE)

; RomTag constants
RTC_MATCHWORD	equ	$4AFC
RTF_COLDSTART	equ	$01
NT_UNKNOWN	equ	$00

; Hunk type IDs
HUNK_HEADER	equ	$3F3
HUNK_CODE	equ	$3E9
HUNK_DATA	equ	$3EA
HUNK_BSS	equ	$3EB
HUNK_RELOC32	equ	$3EC
HUNK_END	equ	$3F2

; exec LVO offsets
_LVOAllocMem	equ	-198
_LVOInitResident equ	-102
_LVOCacheClearU	equ	-636
_LVOCacheControl equ	-648

; Memory flags
MEMF_PUBLIC	equ	$0001
MEMF_CLEAR	equ	$10000

MAX_HUNKS	equ	8

	org	BASE

;---------------------------------------------------------------------------
; RomTag (struct Resident)
;---------------------------------------------------------------------------
RomTag:
	dc.w	RTC_MATCHWORD		; rt_MatchWord
	dc.l	RomTag			; rt_MatchTag
	dc.l	EndSkip			; rt_EndSkip
	dc.b	RTF_COLDSTART		; rt_Flags
	dc.b	1			; rt_Version
	dc.b	NT_UNKNOWN		; rt_Type
	dc.b	4			; rt_Pri
	dc.l	Name			; rt_Name
	dc.l	IdString		; rt_IdString
	dc.l	Init			; rt_Init

Name:
	dc.b	"tfpcmcia.rom",0
	even

IdString:
	dc.b	"TerribleFire PCMCIA ROM 1.0 (2026)",0
	even

;---------------------------------------------------------------------------
; Init code - called by InitResident() with A6=ExecBase, D0=0
;---------------------------------------------------------------------------
Init:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	a6,a5			; a5 = ExecBase (preserve across calls)

	; set serial baud rate
	move.w	#SERPER_9600,SERPER

	; reset the LED to default. 
	move.b  #0,BOARD_CTRL

	; print "TF\n"
	lea	str_tf(pc),a0
	bsr	ser_puts

	move.b  #2,BOARD_CTRL

	bsr SimonTopDog

	; Disable all CPU caches
	moveq	#0,d0			; cacheBits = 0 (all off)
	move.l	#$FFFFFFFF,d1		; cacheMask = all bits
	move.l	a5,a6
	jsr	_LVOCacheControl(a6)

	move.b  #1,BOARD_CTRL

	;-------------------------------------------------------------------
	; Load embedded tfpcmcia.device from ROM into Amiga RAM
	;-------------------------------------------------------------------
	lea	DeviceBinary(pc),a2	; a2 = read pointer into ROM hunk data

	; Read HUNK_HEADER magic
	bsr	read_long		; d0 = first longword
	cmp.l	#HUNK_HEADER,d0
	beq.s	.hunkOk

	lea	str_badhunk(pc),a0
	bsr	ser_puts
	bra	.switchSpiram

.hunkOk:
	; Skip resident library names (should be 0)
	bsr	read_long
	tst.l	d0
	bne	.switchSpiram

	; Read table size (number of hunks), first hunk, last hunk
	bsr	read_long		; d0 = table size (in longs)
	bsr	read_long		; d0 = first hunk
	move.l	d0,d6			; d6 = first hunk index
	bsr	read_long		; d0 = last hunk
	move.l	d0,d7			; d7 = last hunk index

	; Allocate hunk address table on stack (MAX_HUNKS * 4 bytes)
	sub.w	#MAX_HUNKS*4,sp
	move.l	sp,a3			; a3 = hunk address table

	; Read hunk sizes and allocate memory for each
	move.l	d6,d2			; d2 = current hunk index
.allocLoop:
	cmp.l	d7,d2
	bgt.s	.allocDone

	bsr	read_long		; d0 = size in longwords (may have mem flags in upper bits)
	and.l	#$3FFFFFFF,d0		; mask off memory flags
	lsl.l	#2,d0			; convert to bytes
	move.l	d0,d3			; d3 = hunk size in bytes

	; Allocate hunk memory (+ 4 bytes for BPTR link at start)
	add.l	#4,d0
	move.l	#MEMF_PUBLIC|MEMF_CLEAR,d1
	move.l	a5,a6
	jsr	_LVOAllocMem(a6)
	tst.l	d0
	beq	.allocFail

	; Store hunk base address in table
	; The allocated block starts with a BPTR link word, data starts at +4
	move.l	d2,d4
	sub.l	d6,d4			; d4 = relative hunk index
	lsl.l	#2,d4			; d4 = offset in table
	move.l	d0,0(a3,d4.l)		; store allocation base

	addq.l	#1,d2
	bra.s	.allocLoop

.allocDone:
	;-------------------------------------------------------------------
	; Parse hunk body: HUNK_CODE, HUNK_DATA, HUNK_BSS, HUNK_RELOC32, HUNK_END
	;-------------------------------------------------------------------
	move.l	d6,d2			; d2 = current hunk index

.parseLoop:
	bsr	read_long		; d0 = hunk type
	tst.l	d0
	beq	.parseDone		; end of file

	and.l	#$3FFFFFFF,d0		; strip flags

	cmp.l	#HUNK_CODE,d0
	beq.s	.loadCode
	cmp.l	#HUNK_DATA,d0
	beq.s	.loadCode
	cmp.l	#HUNK_BSS,d0
	beq.s	.doBss
	cmp.l	#HUNK_RELOC32,d0
	beq	.doReloc32
	cmp.l	#HUNK_END,d0
	beq	.doEnd

	; Unknown hunk type — skip by reading size and skipping data
	bsr	read_long		; d0 = size in longs
	lsl.l	#2,d0
	add.l	d0,a2			; skip that many bytes
	bra.s	.parseLoop

.loadCode:
	; HUNK_CODE or HUNK_DATA: read size, copy data
	bsr	read_long		; d0 = size in longwords
	and.l	#$3FFFFFFF,d0
	lsl.l	#2,d0			; d0 = size in bytes
	move.l	d0,d3			; d3 = byte count

	; Get destination: hunk base + 4 (skip BPTR link)
	move.l	d2,d4
	sub.l	d6,d4
	lsl.l	#2,d4
	move.l	0(a3,d4.l),a4		; a4 = allocation base
	addq.l	#4,a4			; skip BPTR link

	; Copy d3 bytes from ROM (a2) to RAM (a4)
	move.l	d3,d4
	subq.l	#1,d4
	bmi.s	.parseLoop		; zero size
.copyLoop:
	move.b	(a2)+,(a4)+
	dbf	d4,.copyLoop
	; handle >64K
	sub.l	#$10000,d4
	bpl.s	.copyLoop

	bra	.parseLoop

.doBss:
	; HUNK_BSS: read size, memory already cleared by MEMF_CLEAR
	bsr	read_long		; d0 = size in longs
	bra	.parseLoop

.doReloc32:
	; HUNK_RELOC32: apply 32-bit relocations
	; Format: count, hunk#, offsets... repeat. Terminated by count=0.
.relocBlock:
	bsr	read_long		; d0 = number of relocations
	tst.l	d0
	beq	.parseLoop		; done with reloc blocks

	move.l	d0,d3			; d3 = reloc count

	bsr	read_long		; d0 = referenced hunk number
	; Get base address of referenced hunk (+4 to skip BPTR link)
	sub.l	d6,d0
	lsl.l	#2,d0
	move.l	0(a3,d0.l),a4		; a4 = alloc base of referenced hunk
	addq.l	#4,a4			; a4 = data base of referenced hunk
	move.l	a4,d5			; d5 = base address for relocation

	; Get base address of current hunk (+4)
	move.l	d2,d4
	sub.l	d6,d4
	lsl.l	#2,d4
	move.l	0(a3,d4.l),a4
	addq.l	#4,a4			; a4 = data base of current hunk

.relocEntry:
	bsr	read_long		; d0 = offset in current hunk
	add.l	d5,0(a4,d0.l)		; add referenced hunk base
	subq.l	#1,d3
	bne.s	.relocEntry

	bra.s	.relocBlock

.doEnd:
	addq.l	#1,d2			; advance to next hunk
	cmp.l	d7,d2
	bgt	.parseDone		; all hunks processed
	bra	.parseLoop

.parseDone:
	;-------------------------------------------------------------------
	; Build BPTR segment list: each hunk's first 4 bytes = BPTR to next
	;-------------------------------------------------------------------
	move.l	d6,d2			; d2 = first hunk index
.linkLoop:
	move.l	d2,d4
	sub.l	d6,d4
	lsl.l	#2,d4
	move.l	0(a3,d4.l),a4		; a4 = current hunk alloc base

	move.l	d2,d3
	addq.l	#1,d3			; d3 = next hunk index
	cmp.l	d7,d3
	bgt.s	.lastLink

	; Get next hunk address, convert to BPTR
	move.l	d3,d4
	sub.l	d6,d4
	lsl.l	#2,d4
	move.l	0(a3,d4.l),d0		; d0 = next hunk alloc base
	lsr.l	#2,d0			; convert to BPTR
	move.l	d0,(a4)			; store as link

	addq.l	#1,d2
	bra.s	.linkLoop

.lastLink:
	clr.l	(a4)			; terminate list

	;-------------------------------------------------------------------
	; Flush caches — loaded code needs I-cache invalidation on 68020+
	;-------------------------------------------------------------------
	move.l	a5,a6
	jsr	_LVOCacheClearU(a6)

	;-------------------------------------------------------------------
	; Scan loaded code for RomTag ($4AFC pattern)
	;-------------------------------------------------------------------
	; First hunk data starts at hunk[0] alloc base + 4
	move.l	0(a3),a4		; a4 = first hunk alloc base
	addq.l	#4,a4			; a4 = first hunk data

	; Get first hunk size: re-read from stored info
	; We need to scan. Use a reasonable limit.
	; We'll scan the first hunk for the RomTag matchword.
	; Typically it's near the start of the code.
	move.l	#4096,d3		; scan up to 4KB

.scanRomTag:
	cmp.w	#RTC_MATCHWORD,(a4)
	beq.s	.foundRomTag

	addq.l	#2,a4
	subq.l	#2,d3
	bgt.s	.scanRomTag

	; RomTag not found
	lea	str_nort(pc),a0
	bsr	ser_puts
	bra.s	.freeTable

.foundRomTag:
	; a4 = pointer to struct Resident in RAM
	; Verify rt_MatchTag points to itself
	cmp.l	2(a4),a4		; rt_MatchTag at offset 2
	bne.s	.scanRomTag		; false positive, keep scanning

	lea	str_initres(pc),a0
	bsr	ser_puts

	; Call InitResident(resident, segList)
	; InitResident is exec LVO -102
	; d1 = segList (BPTR to first hunk)
	; a1 = resident pointer
	move.l	a4,a1			; a1 = Resident*
	move.l	0(a3),d0		; d0 = first hunk alloc base
	lsr.l	#2,d0			; convert to BPTR
	move.l	d0,d1			; d1 = segList
	move.l	a5,a6			; a6 = ExecBase
	jsr	_LVOInitResident(a6)

.freeTable:
	; Free hunk table from stack
	add.w	#MAX_HUNKS*4,sp
	bra.s	.switchSpiram

.allocFail:
	lea	str_nomem(pc),a0
	bsr	ser_puts
	add.w	#MAX_HUNKS*4,sp

.switchSpiram:
	; We're executing from ROM at $600000. Switching to SPIRAM mode
	; will unmap this ROM, so we must copy the final instructions to
	; RAM (the stack) and jump there.
	lea	.trampoline(pc),a0
	move.l	#(.trampEnd-.trampoline),d0
	sub.l	d0,sp			; reserve space on stack
	move.l	sp,a1
.copyTramp:
	move.b	(a0)+,(a1)+
	subq.l	#1,d0
	bne.s	.copyTramp

	; Flush caches before executing from stack
	move.l	a5,a6
	jsr	_LVOCacheClearU(a6)
	jmp	(sp)			; jump to trampoline on stack

	; This code is copied to RAM and executed from there.
	; No PC-relative references allowed — only absolute addresses.
.trampoline:
	
	adda.w	#(.trampEnd-.trampoline),sp	; skip past trampoline on stack
	movem.l	(sp)+,d2-d7/a2-a6
	moveq	#0,d0
	rts
.trampEnd:

;---------------------------------------------------------------------------
; read_long - read a 32-bit big-endian value from ROM at (a2)
; in:  a2 = read pointer
; out: d0.l = value, a2 advanced by 4
;---------------------------------------------------------------------------
read_long:
	move.l	(a2)+,d0
	rts

;---------------------------------------------------------------------------
; Strings
;---------------------------------------------------------------------------
str_tf:		dc.b	"TF TopDOG wtf!",$0A,0
str_badhunk:	dc.b	"bad hunk",$0A,0
str_nort:	dc.b	"no RT",$0A,0
str_nomem:	dc.b	"no mem",$0A,0
str_initres:	dc.b	"IR",$0A,0
	even

;---------------------------------------------------------------------------
; ser_putc - send one character to serial port
; in:  d0.b = character
; trashes: none
;---------------------------------------------------------------------------
ser_putc:
.wait:
	btst	#TBE_BIT,SERDATR
	beq.s	.wait
	and.w	#$00FF,d0
	or.w	#$0100,d0		; stop bit
	move.w	d0,SERDAT
	rts

;---------------------------------------------------------------------------
; ser_puts - print null-terminated string
; in:  a0 = string pointer
; trashes: d0, a0
;---------------------------------------------------------------------------
ser_puts:
	move.b	(a0)+,d0
	beq.s	.done
	bsr.s	ser_putc
	bra.s	ser_puts
.done:
	rts

;---------------------------------------------------------------------------
; Embedded device binary
;---------------------------------------------------------------------------
	even
DeviceBinary:
	incbin	"tfpcmcia.device"
DeviceBinaryEnd:
SimonTopDog:
	incbin	"std_lz4.bin"

EndSkip:

; pad to 128KB
	dcb.b	$20000-(EndSkip-RomTag),$FF
