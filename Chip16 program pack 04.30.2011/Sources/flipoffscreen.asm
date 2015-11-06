; Flip & Offscreen test by Refraction
;
; r0 = General Purpose
; r1 = General Purpose 
; r2 = Flipped Horizontal Status
; r3 = Flipped Vertical Status
; r4 = Current Controller State
; r5 = Last Controller State
; r6 = Sprite Draw X pos
; r7 = Sprite Draw Y pos
; r8 = Sprite Memory Location/Ascii memory
; r9 = General Purpose
; ra = Box X position
; rb = Box Y position 
; rc = General Purpose
; rd = General Purpose
; re = General Purpose
; rf = General Purpose

importbin font.bin 0 3072 font								 ;spr #0804
importbin colorblock.bin 0 5000 box						 ;spr #6432

:init
	ldi ra, 50
	ldi rb, 50 ;start our box in the top left
	
:check_controls
	cls
  call testflips
	ldm r4, #FFF0				;Load the controller state
	andi r4, #C0				;just a and b
	xor r4, r5          ;see if the state actually changed
	cmpi r4, 0					;see if the state has changed
	jz check_movement
	ldm r5, #FFF0				;state has changed so regrab it	
	and r5, r4					;make sure we only process what's changed
:test_a
	tsti r5, #40				;Test the A button
	jz test_b
	xori r3, 1  ;flip the vertical flip
:test_b
	tsti r5, #80				;Test the B button
	jz check_movement	
	xori r2, 1  ;flip the horizontal flip
:check_movement	
	ldm r4, #FFF0				;Load the controller state
	call move_up
	call move_down
	call move_left
	call move_right
	
:draw_box
	spr #6432
	ldi r8, box
	drw ra, rb, r8

:draw_text	
	flip 0, 0 ;make sure we arent flipped for text
	ldi r8, ascii_fliphoz ;draw our title text
	ldi r6, 0 ; X = 100
	ldi r7, 230		; Y = 0
	call draw_string
	cmpi r2, 1
	jz htext_enabled
	ldi r8, ascii_disabled
:continue_draw_text
	call draw_string
	ldi r8, ascii_flipvert ;draw our title text
	ldi r6, 0 ; X = 100
	ldi r7, 220		; Y = 0
	call draw_string
	cmpi r3, 1
	jz vtext_enabled
	ldi r8, ascii_disabled
:continue_draw_text2
	call draw_string
	
:end_wait
	vblnk
	jmp check_controls

:move_up
	tsti r4, #1	;Check up
	jz returnsub
	subi rb, 1
	ret
:move_down
	tsti r4, #2	;Check down
	jz returnsub
	addi rb, 1
	ret
:move_left
	tsti r4, #4	;Check left
	jz returnsub
	subi ra, 1
	ret
:move_right
  tsti r4, #8	;Check right
	jz returnsub
	addi ra, 1
	ret
	
:testflips	
	tst r2, r3
	jnz flip_h1v1
	tsti r2, 1
	jnz flip_h1v0
	tsti r3, 1
	jnz flip_h0v1
	jmp flip_h0v0
	
:flip_h0v0
	flip 0, 0
	ret
:flip_h1v0
	flip 1, 0
	ret
:flip_h0v1
	flip 0, 1
	ret
:flip_h1v1
	flip 1, 1
	ret
	
:htext_enabled
	ldi r8, ascii_enabled
	jmp continue_draw_text
	
:vtext_enabled
	ldi r8, ascii_enabled
	jmp continue_draw_text2
	
;Stolen and modified from Shendo's Music Maker
:draw_string				;Draw string on screen
	spr #0804			;Set 8x8 pixel sprites
	ldm r1, r8			;Load character from memory
	andi r1, #FF			;Only the lo byte is needed

	cmpi r1, 255			;Remove terminator
	jz returnsub			;Terminator reached, break subroutine

	subi r1, 32
	muli r1, 32			;Each character is 32 bytes long
	addi r1, font		;Apply offset to font address

	drw r6, r7, r1			;Draw 8x8 character on the set coordinates

	addi r8, 1			;Increase memory offset
	addi r6, 9			;Increase X coordinate

	jmp draw_string ;loop around, we have text left!
	
:returnsub
	ret
	
:ascii_fliphoz
	db "(B) Horizontal Flip is "
	db 255

:ascii_flipvert
	db "(A) Vertical Flip is "
	db 255
	
:ascii_enabled
	db "enabled"
	db 255
	
:ascii_disabled
	db "disabled"
	db 255
	
:ascii_title
	db "Flip and offscreen sprite test by Refraction"
	db 255
