; DMG Boot Process
DMGBoot:
	PUSH AF
	PUSH BC
	PUSH DE
	PUSH HL
; Set up Nintendo logo tiles.
	LD BC,$C8
	LD DE,NintendoLogo2x
	LD HL,$8010
	CALL LoadByteTiles
; Set up Nintendo logo map.
	LD A,$19
	LD [$9910],A
	LD HL,$992F
DMGSetNintendoLogoLine:
	LD C,12
DMGSetNintendoLogoTile:
	DEC A
	JR Z, DMGBootAnimation
	LD [HL-],A
	DEC C
	JR NZ, DMGSetNintendoLogoTile
	LD L,$F
	JR DMGSetNintendoLogoLine
; Perform boot animation
DMGBootAnimation:
; Set up DMG palette.
	wreg R_BGP,$FC
; Set up scroll counter for 100 steps.
        wreg R_SCY,$64
; Set up LCD.
        wreg R_LCDC,$91
DMGStep:
; Wait 2 frames between steps.
REPT 2
	CALL WaitForVBlank
ENDR
; Update scroll.
	rrega R_SCY
        DEC A
	wrega R_SCY
; Play tone at certain steps.
	LD B,$83
	CP 2
	CALL Z, PlayFrequency
	LD B,$C1
	CP 0
	CALL Z, PlayFrequency
; Loop until we reach scroll 0.
	JR NZ, DMGStep
; Wait 64 frames before continuing.
        LD A,64
DMGWaitLoop:
	CALL WaitForVBlank
	DEC A
	JR NZ,DMGWaitLoop
	POP HL
	POP DE
	POP BC
	POP AF
        RET

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
