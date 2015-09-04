;----------------------------------
; NSF player for cxNES
; 	
; Based on Loopy's PowerPak NSF player
; This code, in source or binary form, may be copied, distributed and modified 
; without restriction.
;
; Player rom is mapped to 4000-4FFF.
; NSF header can be read at 4080-40FF.
; Vectors (4FFA-4FFF) are magically moved to FFFA-FFFF
;
; NSF registers:
;
;  4027: timer latch LSB
;  4028: timer latch MSB
;  4029: timer control+status (Read: bit7=timer wrapped,  Write: bit0=IRQ enable and clear irq)
;  402A: Expansion audio control (copy header[0x7B] here)
;  5FF6-5FFF: banking registers (as described in NSF spec)
;
;  Timer details:
;      Timer is a 16bit 1MHz counter that counts down from ($4028<<8)+$4027 to 0.
;      After the counter reaches 0, it's automatically reloaded and (optionally) an IRQ is tripped.
;      Clear the status/irq bit by writing to $4029.
;
;-----------------------------------


A       = $80
B       = $40
SELECT  = $20
START   = $10
UP      = $08
DOWN    = $04
LEFT    = $02
RIGHT   = $01

HDR_BASE	= $4180
HDR_SONGS	= HDR_BASE+$06
HDR_FIRST	= HDR_BASE+$07
HDR_NTSC_LO	= HDR_BASE+$6E
HDR_NTSC_HI	= HDR_BASE+$6F
HDR_PAL_LO	= HDR_BASE+$78
HDR_PAL_HI	= HDR_BASE+$79
HDR_BANK	= HDR_BASE+$70
HDR_INIT	= HDR_BASE+$0a
HDR_PLAY	= HDR_BASE+$0c
HDR_LOAD	= HDR_BASE+$08
HDR_TITLE	= HDR_BASE+$0e
HDR_ARTIST	= HDR_BASE+$2e
HDR_COPYRIGHT	= HDR_BASE+$4e
HDR_EXP_HW	= HDR_BASE+$7b

RAMTMP		= $1f0
CURRENT		= RAMTMP+0	;song#
PLAYING		= RAMTMP+1	;nonzero=song is playing
JOYPAD		= RAMTMP+2	;button state
JOYD		= RAMTMP+3	;button 0->1
JOYTMP		= RAMTMP+4
FRAME		= RAMTMP+5
NTSC_PAL	= RAMTMP+6	;1=PAL detected
DIVTMP		= RAMTMP+7
STR		= RAMTMP+8	;4 bytes used for printing current song #

	org $4000
	pad $4208
	db 0		;save flag

nmi ;--------------------
irq
	rti
	rti

pal_detect ;-----------------
	lda $2002
-	lda $2002
	bpl -

	ldy #24			;eat about 30000 cycles (1 NTSC frame)
	ldx #100
-	dex
	bne -
	dey
	bne -

	lda $2002		;VBL flag is set if NTSC
	bmi +
	inx
+       stx NTSC_PAL
	rts
reset ;------------------
        sei
        ldx #$ef
        txs
        stx PLAYING

        lda #$00                ;screen off
        sta $2000
        sta $2001
	jsr pal_detect

	ldx #8			;load CHR: 128 tiles = $800 bytes
        ldy #$00
        sty $2006
        sty $2006
        lda #(chr&255)
        sta 0
        lda #(chr>>8)
        sta 1
-       lda (0),y
        sta $2007
        iny
        bne -
	inc 1
	dex
	bpl -

        lda #$3f                ;set palette
        sta $2006
        lda #$00
        sta $2006
        ldx #8
-       lda #$0f
        sta $2007
        lda #$30
        sta $2007
        sta $2007
        sta $2007
        dex
        bne -

        lda #$20                ;clear screens
        sta $2006
        lda #$00
        sta $2006
        tax
        ldy #4 ;$10
        lda #$20		;" "
-       sta $2007
        inx
        bne -
        dey
        bne -

	jsr timer_init

        lda #$22                
        sta $2006
        lda #$10
        sta $2006
        lda #$2f               ;"/"
        sta $2007
        lda HDR_SONGS
        jsr deci                
-       lda STR,y               ;print #songs
        sta $2007
        dey
        bpl -

        lda #$20		;print song infos
        sta $2006
        lda #$c2
        sta $2006
	ldy #0
-	lda HDR_TITLE,y
	beq +
        sta $2007
	iny
	cpy #30
	bne -
+
        lda #$21
        sta $2006
        lda #$02
        sta $2006
	ldy #0
-	lda HDR_ARTIST,y
	beq +
        sta $2007
	iny
	cpy #30
	bne -
+
        lda #$21
        sta $2006
        lda #$42
        sta $2006
	ldy #0
-	lda HDR_COPYRIGHT,y
	beq +
        sta $2007
	iny
	cpy #30
	bne -
+
	jsr bank_init

	lda HDR_EXP_HW		;enable extra audio HW (also PRG write enable for FDS)
	sta $402a

	lda HDR_FIRST		;init first song
        sta CURRENT
        jsr init

        lda #%00001010          ;screen on, sprites off
        sta $2001

busywait
	lda $4029
	bpl busywait
	sta $4029

        inc FRAME
        jsr joyread
        jsr play
	jmp busywait

init ;------------------        ;begin CURRENT song
 jmp foo
	lda #$60		;clear 6000-7fff
	sta 1
	lda #0
	sta 0
	tax
-	sta (0),x
	inx
	bne -
	inc 1
	bpl -
 foo
        lda #0                  ;clear 0000-07ff
        tax
-       sta $00,x
        sta $200,x              ;(not 100-1ff)
        sta $300,x
        sta $400,x
        sta $500,x
        sta $600,x
        sta $700,x
        inx
        bne -

	tsx			;clear unused stack
-	sta $100,x
	dex
 	bne -
	sta $100

        lda CURRENT             ;print song#
        jsr deci
        lda #$0f        ;<---
-       iny
        sta STR,y
        cpy #3
        bne -
-       lda $2002
        bpl -
        lda #$22
        sta $2006
        lda #$0d
        sta $2006
        lda STR+2
        sta $2007
        lda STR+1
        sta $2007
        lda STR
        sta $2007

        lda #$00                ;reset scroll
        sta $2006
        sta $2006

        jsr stopsound
        lda PLAYING
        bne +
        rts
+
	jsr bank_init
        ldx CURRENT             ;get song#
        dex
        txa                     ;jmp (INIT)
        ldx NTSC_PAL
        jmp (HDR_INIT)
bank_init ;---------------
	lda #0
	ldx #7
-	ora HDR_BANK,x
	dex
	bpl -
	tax
	bne banked_nsf
  not_banked:
	lda HDR_LOAD+1
	lsr
	lsr
	lsr
	lsr
	tax
	ldy #0
-	tya
	sta $5ff0,x
	iny
	inx
	cpx #$10
	bne -
	rts
  banked_nsf:
        ldx #7
-       lda HDR_BANK,x
        sta $5ff8,x
        dex
        bpl -
	lda HDR_BANK+6		;FDS also has banks @ 6000-7FFF
	sta $5ff6
	lda HDR_BANK+7
	sta $5ff7
	rts
timer_init ;-------------
	lda HDR_NTSC_LO
	ldy HDR_NTSC_HI
	ldx NTSC_PAL
	beq +
	lda HDR_PAL_LO
	ldy HDR_PAL_HI
+
	cpy #0
	beq fixit
+	cmp #$41
	bne time_ok
	cpy #$1a
	bne time_ok
  fixit
	lda time_lo,x
	ldy time_hi,x
  time_ok
	sta $4027
	sty $4028
	rts

  time_hi db $41, $4e
  time_lo db $1a, $20

stopsound ;---------------
        ldx #$00                ;reset sound regs
        stx $4015
        stx $4008
        ldx #$10
        stx $4000
        stx $4004
        stx $400c
        ldx #$0f
        stx $4015

	lda HDR_EXP_HW
	asl

_SUN5B	asl
	bpl _NAMCO
	ldy #$00
	ldx #$0d
-	stx $c000
	sty $e000
	dex
	bne -
	ldx #$07		;Sunsoft-5B stop
	stx $c000
	ldx #$3f
	stx $e000

_NAMCO	asl
	bpl _MMC5
	tay
	clc			;Namco 163 stop
	ldx #$00
	lda #$47
-	sta $F800
	stx $4800
	adc #$08
	bpl -
	tya

_MMC5	asl
	bpl _FDS
        ldx #$00                ;MMC5 stop
        stx $5015
        ldx #$10
        stx $5000
        stx $5004
        ldx #$03
        stx $5015

_FDS	asl
	bpl _VRC7
	ldx #$c0		;FDS stop
	stx $4083
	stx $4080
	stx $4084
	stx $4087
	stx $4089
	ldx #$ff
	stx $408a

_VRC7   asl
	bpl _VRC6

_VRC6	asl
	bpl +
	ldx #$00		;VRC6 stop
	stx $9000
	stx $a000
	stx $b000

+       rts

play ;--------------------
        lda PLAYING
        beq +
        lda #DOWN
        bit JOYPAD
        beq ++
        lda FRAME
        lsr
        bcc +
++      lda #UP
        bit JOYPAD
        beq playhere
        jsr playhere
        jsr playhere
  playhere
        jmp (HDR_PLAY)             ;jmp (PLAY)
+       rts
joyread ;----------------
        lda JOYPAD
        pha
	jsr joyread2
retry	lda JOYPAD
	sta JOYTMP
	jsr joyread2
	lda JOYPAD
	eor JOYTMP
	bne retry

        pla
        eor JOYPAD
        and JOYPAD
        sta JOYD

        asl             ;A
        bcc +
_A          ldx CURRENT
            cpx HDR_SONGS
            bcc ++
            ldx #$00
++          inx
            stx CURRENT
            jmp init
+
        asl             ;B
        bcc +
_B          dec CURRENT
            bne ++
            lda HDR_SONGS
            sta CURRENT
++          jmp init
+
        asl             ;SEL
        bcc +
            lda #$00
            sta PLAYING
            jmp stopsound
+
        asl             ;START
        bcc +
            lda #$01
            sta PLAYING
            jmp init
+
        asl             ;UP
        asl             ;DOWN
        asl             ;LEFT
        bcs _B          
 
        asl             ;RIGHT
        bcs _A
        rts
joyread2 ;----------------
        ldx #1
        stx $4016
        dex
        stx $4016
        ldx #$08
-       clc
	lda $4016
	ora #$fc
	adc #3
        rol JOYPAD
        dex
        bne -
	rts
div10 ;------------------
        ldx #0                  ;in: A=#
        stx DIVTMP                ;out: X=#/10, A=remainder
        cmp #%10100000
        bcc +
        sbc #%10100000
+       rol DIVTMP
        cmp #%01010000
        bcc +
        sbc #%01010000
+       rol DIVTMP
        cmp #%00101000
        bcc +
        sbc #%00101000
+       rol DIVTMP
        cmp #%00010100
        bcc +
        sbc #%00010100
+       rol DIVTMP
        cmp #%00001010
        bcc +
        sbc #%00001010
+       rol DIVTMP
        ldx DIVTMP
        rts
deci ;-------------------       ;in: A=#
        ldy #$ff                ;out: A,X=?  Y=strlen-1
-       jsr div10
        iny
        sta STR,y
        txa
        bne -
        rts

;------------------------
 chr     .bin "font.chr" ;our font (ascii chr set)

       .pad $4ffa
       .WORD nmi
       .WORD reset
       .WORD irq
	
