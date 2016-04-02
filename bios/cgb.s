; CGB Boot Process
CGBBoot:
	PUSH AF
	PUSH BC
	PUSH DE
	PUSH HL
; Set VRAM bank to 1.
	wreg R_VBK,1
; Set up Nintendo logo tiles, offset by 1 for color 2.
	LD BC,$38
	LD DE,NintendoLogo
	LD HL,$8381
	CALL LoadByteTiles
; Set up Game Boy logo tiles.
	LD BC,$180
	LD DE,GameBoyLogo
	LD HL,$8080
	CALL LoadByteTiles
; Set up Game Boy logo palette.
	LD B,$30
	LD HL,GameBoyLogoColors
	wreg R_BCPS,$80
CGBLoadGameBoyLogoColor:
	wreg R_BCPD,[HL+]
	DEC B
	JP NZ,CGBLoadGameBoyLogoColor
; Set up Game Boy logo map.
	LD HL,$98C2
	LD B,8
CGBSetGameBoyLogoLine:
; Set VRAM bank to 0.
	wreg R_VBK,0
; Load tile ID.
	LD A,B
	LD [HL],A
; Set VRAM bank to 1.
	wreg R_VBK,1
; Load tile flags.
	LD A,8
	LD [HL+],A
	INC B
	LD A,B
	CP $18
	JR NZ,CGBCheckGameBoyLogoLine2
	LD L,$E2
CGBCheckGameBoyLogoLine2:
	CP $28
	JR NZ,CGBCheckGameBoyLogoLine3
	LD HL,$9902
CGBCheckGameBoyLogoLine3:
	CP $38
	JR NZ,CGBSetGameBoyLogoLine
; Perform boot animation
CGBBootAnimation:
; Set up DMG palette.
	wreg R_BGP,$FC
; Set up LCD.
        wreg R_LCDC,$91
; Run 106 steps.
	LD A,106
CGBStep:
; Update step counter.
	DEC A
; Wait frame.
	CALL WaitForVBlank
; Fade out from 38 to end.
	CP 38
	CALL C,CGBFadeOut
	JR C,CGBStepEnd
; Display Nintendo logo at 93.
	CP 93
	CALL Z,CGBSetNintendoLogoMap
; Wait frame.
	CALL WaitForVBlank
; Reveal Game Boy logo from 86 to 54.
	CP 86
	JR NC,CGBStepCheckSound
	CP 54
	CALL NC,CGBRevealGameBoyLogo
CGBStepCheckSound:
; Play high tone at 60.
	LD B,$83
	CP 60
	CALL Z, PlayFrequency
; Play low tone at 60.
	LD B,$C1
	CP 58
	CALL Z, PlayFrequency
CGBStepEnd:
; Loop until the counter has reached 0.
	CP 0
	JR NZ, CGBStep
; Clear VRAM banks.
	LD D,0
	LD BC,VRAM_SIZE
	LD HL,VRAM_START
        wreg R_VBK,1
	CALL FillMem
	wreg R_VBK,0
	CALL FillMem
	POP HL
	POP DE
	POP BC
	POP AF
	RET

; Set up Nintendo logo in BG map.
CGBSetNintendoLogoMap:
	PUSH AF
	PUSH BC
	PUSH HL
	LD HL, $99A7
	LD B, $38
CGBSetNintendoLogoMapTile:
; Write tile to bank 0.
	wreg R_VBK,0
	LD A,B
	LD [HL],A
; Write attributes to bank 1.
	wreg R_VBK,1
	LD A,8
	LD [HL+],A
	INC B
	LD A,B
	CP $3F
	JR NZ,CGBSetNintendoLogoMapTile
	POP HL
	POP BC
	POP AF
	RET

; Reveal part of Game Boy logo for step A.
CGBRevealGameBoyLogo:
	PUSH AF
	PUSH BC
	PUSH DE
	PUSH HL
	LD B,A
	LD A,86
	SUB B
	LD HL,$98B2
	LD B,0
	LD C,A
	ADD HL,BC
; Reveal over 3 lines of tiles.
	LD B,$03
CGBRevealGameBoyLogoLine:
; Reveal 5 sections per line. (1-6)
	LD C,$01
CGBRevealGameBoyLogoLineSection:
; Reveal 3 tiles per section.
	LD D,$03
CGBRevealGameBoyLogoTile:
	LD A,[HL]
	AND $F8
	OR C
	LD [HL+],A
	DEC D
	JR NZ,CGBRevealGameBoyLogoTile
	INC C
	LD A,C
	CP $06
	JR NZ,CGBRevealGameBoyLogoLineSection
	LD DE,$11
	ADD HL,DE
	DEC B
	JR NZ,CGBRevealGameBoyLogoLine
	POP HL
	POP DE
	POP BC
	POP AF
	RET

; Increment all colors toward white.
CGBFadeOut:
	PUSH AF
	PUSH BC
	PUSH DE
	LD B,0
; Fade red out.
CGBFadeOutRed:
	wreg R_BCPS,B
	rrega R_BCPD
	AND $1F
	CP $1F
	JR Z,CGBFadeOutGreen
	INC A
; Fade green out.
CGBFadeOutGreen:
	LD C,A
	rrega R_BCPD
	RLCA
	RLCA
	RLCA
	AND $07
	LD D,A
	INC B
	wreg R_BCPS,B
	rrega R_BCPD
	RLCA
	RLCA
	RLCA
	AND $18
	OR D
	CP $1F
	JR Z,CGBFadeOutBlue
	INC A
; Fade blue out.
CGBFadeOutBlue:
	RRCA
	RRCA
	RRCA
	LD D,A
	AND $E0
	OR C
	LD E,A
	DEC B
	wreg R_BCPS,B
	wreg R_BCPD,E
	LD A,D
	AND $03
	LD C,A
	INC B
	wreg R_BCPS,B
	rrega R_BCPD
	RRCA
	RRCA
	AND $1F
	CP $1F
	JR Z,CGBFadeOutColor
	INC A
; Finish and loop back.
CGBFadeOutColor:
	RLCA
	RLCA
	OR C
	LD E,A
	wreg R_BCPD,E
	INC B
	LD A,B
	CP $40
	JR NZ,CGBFadeOutRed
	POP DE
	POP BC
	POP AF
	RET

; Nintendo Logo + (R) Symbol
NintendoLogo:
	DB $C6,$E6,$E6,$D6,$D6,$CE,$CE,$C6
	DB $C0,$C0,$00,$DB,$DD,$D9,$D9,$D9
	DB $00,$30,$78,$33,$B6,$B7,$B6,$B3
	DB $00,$00,$00,$CD,$6E,$EC,$0C,$EC
	DB $01,$01,$01,$8F,$D9,$D9,$D9,$CF
	DB $80,$80,$80,$9E,$B3,$B3,$B3,$9E
	DB $3C,$42,$B9,$A5,$B9,$A5,$42,$3C

; Game Boy Logo
GameBoyLogo:
	DB $01,$01,$0F,$0F,$3F,$3F,$7E,$7E
	DB $FF,$FF,$FF,$FF,$C0,$C0,$00,$00
	DB $C0,$C0,$F0,$F0,$F1,$F1,$03,$03
	DB $7C,$7C,$FC,$FC,$FE,$FE,$FE,$FE
	DB $03,$03,$07,$07,$07,$07,$0F,$0F
	DB $E0,$E0,$E0,$E0,$F0,$F0,$F0,$F0
	DB $1E,$1E,$3E,$3E,$7E,$7E,$FE,$FE
	DB $0F,$0F,$0F,$0F,$1F,$1F,$1F,$1F
	DB $FF,$FF,$FF,$FF,$00,$00,$00,$00
	DB $01,$01,$01,$01,$01,$01,$03,$03
	DB $FF,$FF,$FF,$FF,$E1,$E1,$E0,$E0
	DB $C0,$C0,$F0,$F0,$F9,$F9,$FB,$FB
	DB $1F,$1F,$7F,$7F,$F8,$F8,$E0,$E0
	DB $F3,$F3,$FD,$FD,$3E,$3E,$1E,$1E
	DB $E0,$E0,$F0,$F0,$F9,$F9,$7F,$7F	
	DB $3E,$3E,$7C,$7C,$F8,$F8,$E0,$E0
	DB $F8,$F8,$F0,$F0,$F0,$F0,$F8,$F8
	DB $00,$00,$00,$00,$7F,$7F,$7F,$7F
	DB $07,$07,$0F,$0F,$9F,$9F,$BF,$BF
	DB $9E,$9E,$1F,$1F,$FF,$FF,$FF,$FF
	DB $0F,$0F,$1E,$1E,$3E,$3E,$3C,$3C
	DB $F1,$F1,$FB,$FB,$7F,$7F,$7F,$7F
	DB $FE,$FE,$DE,$DE,$DF,$DF,$9F,$9F
	DB $1F,$1F,$3F,$3F,$3E,$3E,$3C,$3C
	DB $F8,$F8,$F8,$F8,$00,$00,$00,$00
	DB $03,$03,$03,$03,$07,$07,$07,$07
	DB $FF,$FF,$FF,$FF,$C1,$C1,$C0,$C0
	DB $F3,$F3,$E7,$E7,$F7,$F7,$F3,$F3
	DB $C0,$C0,$C0,$C0,$C0,$C0,$C0,$C0
	DB $1F,$1F,$1F,$1F,$1E,$1E,$3E,$3E
	DB $3F,$3F,$1F,$1F,$3E,$3E,$3E,$3E
	DB $80,$80,$00,$00,$00,$00,$00,$00
	DB $7C,$7C,$1F,$1F,$07,$07,$00,$00
	DB $0F,$0F,$FF,$FF,$FE,$FE,$00,$00
	DB $7C,$7C,$F8,$F8,$F0,$F0,$00,$00
	DB $1F,$1F,$0F,$0F,$0F,$0F,$00,$00
	DB $7C,$7C,$F8,$F8,$F8,$F8,$00,$00
	DB $3F,$3F,$3E,$3E,$1C,$1C,$00,$00
	DB $0F,$0F,$0F,$0F,$0F,$0F,$00,$00
	DB $7C,$7C,$FF,$FF,$FF,$FF,$00,$00
	DB $00,$00,$F8,$F8,$F8,$F8,$00,$00
	DB $07,$07,$0F,$0F,$0F,$0F,$00,$00
	DB $81,$81,$FF,$FF,$FF,$FF,$00,$00
	DB $F3,$F3,$E1,$E1,$80,$80,$00,$00
	DB $E0,$E0,$FF,$FF,$7F,$7F,$00,$00
	DB $FC,$FC,$F0,$F0,$C0,$C0,$00,$00
	DB $3E,$3E,$7C,$7C,$7C,$7C,$00,$00
	DB $00,$00,$00,$00,$00,$00,$00,$00

; Game Boy Logo Palette
GameBoyLogoColors:
	DB $FF,$FF,$FF,$7F,$00,$00,$00,$00
	DB $FF,$FF,$00,$7C,$00,$00,$00,$00
	DB $FF,$FF,$E0,$03,$00,$00,$00,$00
	DB $FF,$FF,$1F,$7C,$00,$00,$00,$00
	DB $FF,$FF,$1F,$00,$00,$00,$00,$00
	DB $FF,$FF,$FF,$03,$00,$00,$00,$00
