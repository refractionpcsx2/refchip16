; Chip16 v1.3 Specification Test - Chip1613ST
; Tests only new opcodes: MOD,REM,NOT,NEG
FontSize   		equ		$0804        ; 8 x 8 pixel
FontWidth		equ     8
FontHeight		equ     8
ScreenWidth		equ     320
ScreenHeight	equ		240

; R6 = Current Opcode Test
; R7 = Current address of char Cursor
Main:
	call SetN
	call UnsetN
	ldi  R7, StartOSD
	addi R7, 53
	call DrawChars

; Test Ax - Division extensions, MOD, REM
TEST_1:
	call IncreaseTestNumber
; A3 - MOD RX, HHLL
	ldi  R0, 31313
	modi R0, 13 ;=> R0=9
	cmpi R0, 9
	jnz  WriteFailedCurrentTest
TEST_2:
	call IncreaseTestNumber
; A4 - MOD RX, RY
	ldi  R0, -31313
	ldi  R1, 13
	mod  R0, R1 ;=> R0=9
	cmpi R0, 9
	jnz  WriteFailedCurrentTest
TEST_3:
	call IncreaseTestNumber
; A5 - MOD RX, RY, RZ
	ldi  R0, 0
	ldi  R1, -31313
	ldi  R2, -13
	mod  R0,R1,R2 ;=> R0=9
	cmpi R0, 9
	jnz  WriteFailedCurrentTest
TEST_4:
	call IncreaseTestNumber
; A6 - REM RX, HHLL
	ldi  R0, -12345
	remi R0, 45 ;=> R0=-15
	cmpi R0, -15
	jnz  WriteFailedCurrentTest
TEST_5:
	call IncreaseTestNumber
; A7 - REM RX, RY
	ldi  R0, 31999
	ldi  R1, -45
	rem  R0,R1 ;=> R0=4
	cmpi R0, 4
	jnz  WriteFailedCurrentTest
TEST_6:
	call IncreaseTestNumber
; A8 - REM RX, RY, RZ
	ldi  R1, -23456
	ldi  R2, -57
	rem  R0,R1,R2 ;=> R0=-29
	cmpi R0, -29
	jnz  WriteFailedCurrentTest
; Test Ex - Not/Neg
TEST_7:
	call IncreaseTestNumber
; E0 - NOT RX, HHLL
	noti R0, 0x1234
	cmpi R0, 0xEDCB
	jnz  WriteFailedCurrentTest
TEST_8:
	call IncreaseTestNumber
; E1 - NOT RX
	ldi  R0, 0x4567
	not  R0
	cmpi R0, 0xBA98
	jnz  WriteFailedCurrentTest
TEST_9:
	call IncreaseTestNumber
; E2 - NOT RX, RY
	ldi  R1, 0xAFAF
	not  R0, R1
	cmpi R0, 0x5050
	jnz  WriteFailedCurrentTest
TEST_10:
	call IncreaseTestNumber
; E3 - NEG RX, HHLL
	negi R0, 4660
	cmpi R0, -4660
	jnz  WriteFailedCurrentTest
TEST_11:
; E4 - NEG RX
	call IncreaseTestNumber
	ldi  R0, -32123
	neg  R0
	cmpi R0, 32123
	jnz  WriteFailedCurrentTest	
TEST_12:
	call IncreaseTestNumber
; E5 - NEG RX, RY
	ldi  R1, 32767
	neg  R0, R1
	cmpi R0, -32767
	jnz  WriteFailedCurrentTest
	
	call WriteALLTESTSOK
	call DrawChars
Loop:
	jmp  Loop
	
; Draw Chars
DrawChars:
	pushall
OSDInit:
	pal  Palette
	spr  FontSize    	; set sprite size to 8x8
	ldi  R0, StartOSD
	ldi  R1, ScreenWidth
	ldi  R2, MarioFont
	addi R2, 3 			; skip 3 byte header
	ldi  R3, 0
	ldi  R4, 0
OSDDrawLoop:
	ldm  R5, R0
	andi R5, 0xFF
	cmpi R5, 0x20 ;space handled seperately
	jnz  NoSpace
	addi R5, 0x20 ;0x20+0x20-0x30 = 0x10 (space) 
NoSpace:
	subi R5, 0x30
	shl  R5, 5
	add  R5, R2
	drw  R3, R4, R5
	addi R0, 1
	addi R3, FontWidth
	cmpi R3, 320
	jnz  OSDDrawLoop
	ldi  R3, 0
	addi R4, 8
	cmpi R4, 16
	jnz  OSDDrawLoop
	popall
	ret
	
WriteFailedCurrentTest:
	call WriteFAILED
	call WriteCurrentNumber
	call DrawChars
LoopError:
	jmp   LoopError
	
; Write ALL TESTS OK
WriteALLTESTSOK:
	ldi  R0, AllTestsOk
	call WriteString
	ret
; Write FAILED
WriteFAILED:
	ldi  R0, FAILED
	call WriteString
	ret
	
; Write Current Number
WriteCurrentNumber:
	ldi  R0, CurrentTest
	call WriteString
	ret

; Write string starting at R0
; at addr in R7
WriteString:
	call ReadByte
	;check of ending @, end if found!
	cmpi R1, 0x40 ; ASCII of @
	jz   WriteStringEnd
	push R0 ; store source addr on stack
	mov  R0, R7
	call WriteByte
	pop  R0 ; restore source addr from stack
	addi R7, 1
	addi R0, 1
	jp   WriteString
WriteStringEnd:
	ret

; Increase ASCII-BCD 2 byte number, works for (00-99) 
IncreaseTestNumber:
	;push R2
	ldi  R0, CurrentTest
	addi R0, 1
	;ldm  R0, R2
	call ReadByte
	addi R1, 1
	cmpi R1, 0x3A
	jnz  WriteBCD0Back
	ldi  R1, 0x30
	;stm  R0, R2
	call WriteByte
	subi R0, 1
	;ldm  R0, R2 ; read BCD1
	call ReadByte
	addi R1, 1
	call WriteByte
	;stm  R0, R2
	jp   EndIncreaseTestNumber
WriteBCD0Back:
	;stm  R0, R2
	call WriteByte
EndIncreaseTestNumber:
	;pop R2
	ret
include ByteAccessLib.asm
include ManipulateFlagsLib.asm
EndOfProgram:
db 0x00

StartOSD: ; A=16(0x10)
db "     CHIP16 V1"
db 0x3D
db "3 SPECIFICATION TEST     "
db "                                        "
EndOSD:
db 0x00
CurrentTest: ; written in ASCII BCD
db 0x30, 0x30
db "@"
; strings
FAILED:
db "FAILED TEST @"
AllTestsOk:
db "<ALL TESTS OK<@"

Palette:
db 0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
db 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF

; Use Full MarioFont - ASCII correct for alpha&digit => (CHAR-0x30)
importbin C:\Share\Chip16\tchip16_1.4.5\Mario\MarioFontLong.bmp.bin 0 1539 MarioFont
