; setup constants
	.const c20, 1.0, 1.0, 1.0, 1.0

; setup outmap
	.out o0, result.position, 0xF
	.out o1, result.color, 0xF
	.out o2, result.texcoord0, 0x3

	.vsh vmain, end_vmain

; code
	vmain:
		; result.pos = in.pos
		mov o0, v0 (0x5)
		; result.color = (1.0, 1.0, 1.0, 1.0)
		mov o1, c20 (0x5)
		; result.texcoord = in.texcoord
		mov o2, v1 (0x5)
		; end
		nop
		end
	end_vmain:

; operand descriptors
	.opdesc x___, xyzw, xyzw ; 0x0
	.opdesc _y__, xyzw, xyzw ; 0x1
	.opdesc __z_, xyzw, xyzw ; 0x2
	.opdesc ___w, xyzw, xyzw ; 0x3
	.opdesc xyz_, xyzw, xyzw ; 0x4
	.opdesc xyzw, xyzw, xyzw ; 0x5
	.opdesc x_zw, xyzw, xyzw ; 0x6
	.opdesc xyzw, yyyw, xyzw ; 0x7
	.opdesc xyz_, wwww, wwww ; 0x8
	.opdesc xyz_, yyyy, xyzw ; 0x9
