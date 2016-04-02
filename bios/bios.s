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
; Set up audio.
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
	JR NZ,ClearVRAMBank0
        wreg R_VBK,1
	CALL FillMem
ClearVRAMBank0:
	wreg R_VBK,0
	CALL FillMem
; Jump to appropriate boot sequence.
        CALL Z,DMGBoot
        CALL NZ,CGBBoot
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

SECTION "bios",HOME[$0200]
INCLUDE "dmg.s"
INCLUDE "cgb.s"
