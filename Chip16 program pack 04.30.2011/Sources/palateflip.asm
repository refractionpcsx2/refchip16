; Palate flip (PAL OPCode Test) by Refraction
;
; r0 = line counter
; r1 = vertical position
; r2 = horizontal position
; r3 = colour
; r4 = current byte
; r5 = memory base (for sprite)
; r6 = palate number
; r7 = keycheck
; r8 = temporary value
; r9 = memory addr for palate

importbin flipped.bin 0 100 flipped
importbin original.bin 0 150 original

start:
  jmp original_palate ;initialize the original palate (and set r6 :P)
init:
	cls
	spr 0x0707			; 7x14 - 128 bytes
	ldi r0, 16
	ldi r1, 0
	ldi r2, 0
	ldi r3, 0
	ldi r4, 128
	ldi r5, 512 ;memory base of sprite
	
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
	ori r5, 512
	
draw_lines:
	drw r2, r1, r5  ;draw lines going from top to bottom
  addi r1, 7			;move the Y coord down to the next line
  addi r5, 128    ;reset the byte counter
  cmpi r1, 240    ;make sure we havent reached the bottom of the screen
  jbe draw_lines

end_loop:  ;loop round!
	vblnk
	ldm r7, 65520       ;load in keypad to r7
	tsti r7, 1          ;check for up being pressed
	jnz flipped_palate  ;it's pressed so load the flipped palate
	tsti r7, 2          ;check if down is pressed	
	jnz original_palate ;it's pressed so load the original palate
	jmp end_loop				;loopyloopy!
	
flipped_palate:
	tsti r6, 65535      ;check r6 to see what palate is set
	jz end_loop         ;already flipped palate so dont do anything
	ldi r6, 0						;otherwise set r6 as flipped palate
	pal flipped					;read in the palate
	jmp init						;reload the sprites

original_palate:
  tsti r6, 65535		  ;check r6 to see what palate is set
	jnz end_loop			  ;already original palate so dont do anything
	ldi r6, 1						;otherwise set r6 as original palate
	pal original				;read in the palate
	jmp init						;reload the sprites
