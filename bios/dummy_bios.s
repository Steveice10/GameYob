SECTION "entry",HOME[$0000]
EntryPoint:
; Set up stack.
	LD SP,$FFFE
; Jump to Main.
	JP Main

INCLUDE "common.s"

Main:
; Receive GBA mode flag from register B.
	PUSH BC
; Set up audio state.
	wreg R_NR52,$80
	wreg R_NR11,$80
	wreg R_NR12,$F3
	wreg R_NR51,$F3
	wreg R_NR50,$77
; Clear VRAM banks.
	LD D,0
	LD BC,VRAM_SIZE
	LD HL,VRAM_START
	rrega R_KEY1
	CP $FF
	JR Z,ClearVRAMBank0
        wreg R_VBK,1
	CALL FillMem
ClearVRAMBank0:
	wreg R_VBK,0
	CALL FillMem
	PUSH AF
	JR NZ,SetupLCD
; Set up Nintendo logo tiles.
	LD BC,$C8
	LD DE,NintendoLogo2x
	LD HL,$8010
	CALL LoadByteTiles
; Set up Nintendo logo map.
	LD A,$19
	LD [$9910],A
	LD HL,$992F
SetNintendoLogoLine:
	LD C,12
SetNintendoLogoTile:
	DEC A
	JR Z, SetupLCD
	LD [HL-],A
	DEC C
	JR NZ, SetNintendoLogoTile
	LD L,$F
	JR SetNintendoLogoLine
; Set up LCD state.
SetupLCD:
	wreg R_BGP,$FC
	wreg R_LCDC,$91
	POP AF
; Set appropriate A value.
	JP NZ,SetCGBA
	LD A,$01
SetCGBA:
	JP Z,SetRegisters
	LD A,$11
; Set register values.
SetRegisters:
	PUSH AF
	LD HL,SP+$0
	LD [HL],$B0
	POP AF
	POP BC
	LD C,$13
	LD DE,$00D8
	LD HL,$014D

; Slide in to main code.
SECTION "boot",HOME[$00FD]
FinishBoot:
        wreg R_BIOS,A

SECTION "data",HOME[$0200]
; Nintendo Logo 2x + (R) Symbol
NintendoLogo2x:
	DB $F0,$F0,$FC,$FC,$FC,$FC,$F3,$F3
	DB $3C,$3C,$3C,$3C,$3C,$3C,$3C,$3C
	DB $F0,$F0,$F0,$F0,$00,$00,$F3,$F3
	DB $00,$00,$00,$00,$00,$00,$CF,$CF
	DB $00,$00,$0F,$0F,$3F,$3F,$0F,$0F
	DB $00,$00,$00,$00,$C0,$C0,$0F,$0F
	DB $00,$00,$00,$00,$00,$00,$F0,$F0
	DB $00,$00,$00,$00,$00,$00,$F3,$F3
	DB $00,$00,$00,$00,$00,$00,$C0,$C0
	DB $03,$03,$03,$03,$03,$03,$FF,$FF
	DB $C0,$C0,$C0,$C0,$C0,$C0,$C3,$C3
	DB $00,$00,$00,$00,$00,$00,$FC,$FC
	DB $F3,$F3,$F0,$F0,$F0,$F0,$F0,$F0
	DB $3C,$3C,$FC,$FC,$FC,$FC,$3C,$3C
	DB $F3,$F3,$F3,$F3,$F3,$F3,$F3,$F3
	DB $F3,$F3,$C3,$C3,$C3,$C3,$C3,$C3
	DB $CF,$CF,$CF,$CF,$CF,$CF,$CF,$CF
	DB $3C,$3C,$3F,$3F,$3C,$3C,$0F,$0F
	DB $3C,$3C,$FC,$FC,$00,$00,$FC,$FC
	DB $FC,$FC,$F0,$F0,$F0,$F0,$F0,$F0
	DB $F3,$F3,$F3,$F3,$F3,$F3,$F0,$F0
	DB $C3,$C3,$C3,$C3,$C3,$C3,$FF,$FF
	DB $CF,$CF,$CF,$CF,$CF,$CF,$C3,$C3
	DB $0F,$0F,$0F,$0F,$0F,$0F,$FC,$FC
	DB $3C,$42,$B9,$A5,$B9,$A5,$42,$3C
