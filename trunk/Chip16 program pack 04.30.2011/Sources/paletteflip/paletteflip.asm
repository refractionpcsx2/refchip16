; Palette flip (PAL OPCode Test) by Refraction
;
; r0 = line counter
; r1 = vertical position
; r2 = horizontal position
; r3 = colour
; r4 = current byte
; r5 = memory base (for sprite)
; r6 = palette number
; r7 = keycheck
; r8 = temporary value
; r9 = memory addr for palette

importbin flipped.bin 0 48 flipped
importbin original.bin 0 48 original
importbin capitals_font.bin 0 832 capitals_font			;Font data (capital letters)
importbin lowcase_font.bin 0 832 lowcase_font			;Font data (lowcase letters)
importbin special_font.bin 0 320 special_font			;Font data (special characters)

start:
  jmp original_palette ;initialize the original palette (and set r6 :P)
init:
	cls
	spr 0x0707			; 7x14 - 128 bytes
	ldi r0, 16
	ldi r1, 0
	ldi r2, 0
	ldi r3, 0
	ldi r4, 128
	ldi r5, 2048 ;memory base of sprite
	jmp fillmemory

:draw_string				;Draw string on screen
	spr #0804			;Set 8x8 pixel sprites
	ldm ra, rd			;Load characted from memory
	andi ra, #FF			;Only the lo byte is needed

	mov rb, ra			;Copy data to scratchpad
	subi rb, 255			;Remove terminator
	jz fret			;Terminator reached, break subroutine

	mov rb, ra			;Copy data to scratchpad
	muli rb, 32			;Each character is 32 bytes long
	addi rb, capitals_font		;Apply offset to font address

	drw re, rf, rb			;Draw 8x8 character on the set coordinates

	addi rd, 1			;Increase memory offset
	addi re, 9			;Increase X coordinate

	jmp draw_string

:fret					;Subroutine return function
	ret				;Return from a subroutine
	
fillmemory:		;Putting sprite in memory
	stm r3, r5    ;store 
	addi r5, 1	 ;add byte offset
  subi r4, 1    ;byte written  
	jnz, fillmemory
	andi r4, 0
	ori r4, 128   ;reset byte position
	addi r3, 17    ;increase colour by 0x11 (0x22 etc)
	subi r0, 1     ;decrease line we are on
	jnz, fillmemory ;until we have done every line
	andi r1, 0		;reset everything
	andi r2, 0
	andi r3, 0
	andi r5, 0
	ori r5, 2048


draw_lines:
	drw r2, r1, r5  ;draw lines going from top to bottom
  addi r2, 14  
  cmpi r2, 322    ;14 doesnt go in to 22 so check 322
  jbe draw_lines
  andi r2, 0
  addi r1, 7			;move the Y coord down to the next line
  addi r5, 128    ;reset the byte counter
  cmpi r1, 112    ;make sure we havent reached the bottom of the screen
  jbe draw_lines
  
  ldi re, 10			;Load string X coordinate
	ldi rf, 200			;Load string Y coordinate
	ldi rd, ascii_padup		;Load string memory location
	call draw_string
	ldi re, 10			;Load string X coordinate
	ldi rf, 220			;Load string Y coordinate
	ldi rd, ascii_paddown		;Load string memory location
	call draw_string


end_loop:  ;loop round!
	vblnk
	ldm r7, 65520       ;load in keypad to r7
	tsti r7, 1          ;check for up being pressed
	jnz flipped_palette  ;it's pressed so load the flipped palette
	tsti r7, 2          ;check if down is pressed	
	jnz original_palette ;it's pressed so load the original palette
	jmp end_loop				;loopyloopy!
	
flipped_palette:
	tsti r6, 65535      ;check r6 to see what palette is set
	jz end_loop         ;already flipped palette so dont do anything
	ldi r6, 0						;otherwise set r6 as flipped palette
	pal flipped					;read in the palette
	jmp init						;reload the sprites

original_palette:
  tsti r6, 65535		  ;check r6 to see what palette is set
	jnz end_loop			  ;already original palette so dont do anything
	ldi r6, 1						;otherwise set r6 as original palette
	pal original				;read in the palette
	jmp init						;reload the sprites


:ascii_padup
	db 15 ;P
	db 43 ;r
	db 30 ;e
	db 44 ;s
	db 44 ;s
	db 52 ;
	db 20 ;U
	db 15 ;P
	db 52 ;
	db 45 ;t
	db 40 ;o
	db 52 ;
	db 31 ;f
	db 37 ;l
	db 34 ;i
	db 41 ;p
	db 52 ;
	db 41 ;p
	db 26 ;a
	db 37 ;l
	db 30 ;e
	db 45 ;t
	db 45 ;t
	db 30 ;e
	db 255		;/0
	
:ascii_paddown
	db 15 ;P
	db 43 ;r
	db 30 ;e
	db 44 ;s
	db 44 ;s
	db 52 ;
	db 03 ;D
	db 14 ;O
	db 22 ;W
	db 13 ;N
	db 52 ; 
	db 31 ;f
	db 40 ;o
	db 43 ;r
	db 52 ;
	db 40 ;o
	db 43 ;r
	db 34 ;i
	db 32 ;g
	db 34 ;i
	db 39 ;n
	db 26 ;a
	db 37 ;l
	db 255		;/0