; Palate flip (PAL OPCode Test) by Refraction
;
; r0 = Scratchpad
; r1 = Scratchpad
; r2 = Scratchpad
; r3 = Player Lives
; r4 = Scratchpad
; r5 = General Purpose (Random number destination)
; r6 = Frame counter
; r7 = Spawn Timer
; r8 = memory address for current data (sprite/position information)
; r9 = current alien 
; ra = current sprite X-Axis 
; rb = current sprite Y-Axis
; rc = current ship position (X Axis only, ship doesnt move up :P)
; rd = player score
; re = current level
; rf = aliens killed
; base memory location for alien information = alieninfo
; each 32 bit location contains 
; status=lower 16bit lower byte, X=upper 16bit upper byte, Y upper 16bit lower byte
; status = (0 = not active/available 1-2 = active 3-6 = exploding frame)
importbin bullet.bin 0 32 spr_bullet 							;Bullet Sprite 8x8
importbin alien.bin 0 256 spr_alien								;Alien Sprite Animation 16x32
importbin aliendie.bin 0 512 spr_aliendie					;Alien Death Sprite Animation 16x64
importbin ship.bin 0 128 spr_ship									;Ship Sprite 16x16
importbin shipdie.bin 0 512 spr_shipdie						;Ship Death Sprite Animation 16x64
importbin original.bin 0 48 pallate								;Original C16 palatte
importbin font.bin 0 3072 font										;Font data
importbin logo.bin 0 2500 logo										;kickass logo!

:init
	ldi r6, 0 ;reset frame counter
	ldi r3, 3 ;start with 3 lives, customary i guess :P
	ldi re, 1 ;start on level 1, customary also i guess :P
	ldi rd, 0 ;score reset to 0
	stm r3, player_lives
	stm re, current_level
	stm rd, current_score
	
:startmenu
	vblnk
	cls
	ldm r0, #FFF0
	tsti r0, #20 ; AND check "Start" bit
	jnz newgamestart
	spr #3232
	ldi rb, 95 ; Y = 95 
	ldi ra, 110 ; X = 110
	ldi r8, logo ;awesomeness right here
	drw ra, rb, r8 ;drawing of awesomeness
	addi r6, 1   ;frame count	
	cmpi r6, 20  ;frame count
	jbe startmenu
	ldi rb, 200 ; Y = 116 (half way down)
	ldi ra, 110 ; X = 110 (half way across)
	ldi r8, press_start_ascii
	call draw_string
	cmpi r6, 40  ;frame count
	jbe startmenu
	jmp init
	
	
:newgamestart
	ldi r7, 120   ;set spawn speed
	ldi r8, spawn_timer ;point to spawn timer
	stm r7, r8 ;set the spawn timer

:showlevel
	ldi r6, 0 ;reset frame counter
	ldi r4, 512			;zero amount
	addi r4, font		;Apply offset to font address
:showlevel_loop	
	vblnk	
	cls	
	ldi rb, 116 ; Y = 116 (half way down)
	ldi ra, 120 ; X = 120 (half way across)
	ldi r8, player_level_ascii
	call draw_string
	spr #0804			;Set 8x8 pixel sprites
	ldm r1, current_level			;Load characted from memory
	call counter_under_hundred
	addi r6, 1  ;add a frame
	cmpi r6, 180 ; wait 3 seconds before restart
	jnz showlevel_loop
	
:start
	
	ldi r6, 0   ;frame counter to zero
	ldi rc, 160 ;ship always starts in the middle, cos it always has done xD
	ldi r9, 0   ;set current alien to zero
	ldi r0, 0   ;clear reg for alien clearing
	ldi r8, alieninfo  ;grab current alien status mem location
	
clear_aliens:	
	stm r0, r8				 ;Store inactive in current alien status
	addi r8, 6         ;Move memory pointer to next alien
	addi r9, 1				 ;Increment which alien we're dealing with
	cmpi r9, 15        ;All possible aliens delt with?
	jnz clear_aliens   ;they arent so lets try the next one
	
	ldi r8, bulletfired  ;grab bullet status mem location
	stm r0, r8				 	 ;Store not fired in bullet status
	bgc 0	
	
:main_loop	
	cls	
	call handle_statusbar
	call handle_controller
	call handle_ship
	call handle_bullet
	call handle_aliens	
:main_loop_difficulty_inc
	;Difficulty check
	mov r2, re  ;grab current level
	muli r2, 10 ;multiply it by 10 to find target score
	cmp r2, rd  ; compare multiplied score with player score
	jnz main_loop_death
	cmpi re, 12
	jae main_loop_death
	addi re, 1 ;increase difficulty!
	stm re, current_level
	subi r7, 10 ;lower spawn timer
	ldi r6, 0 ;reset the frame counter
	stm r7, spawn_timer ;set the spawn timer
	jmp showlevel
	
:main_loop_death	
	cmpi rc, #5000 ;check if we've exploded
	jae game_ended
	vblnk
	addi r6, 1  ;add a frame
	jmp main_loop

:game_ended
	subi r3, 1
  stm r3, player_lives
	ldi r6, 0 ;reset frame counter
:game_end_loop
	vblnk
	addi r6, 1  ;add a frame
	cmpi r6, 60 ; wait 3 seconds before restart
	jnz game_end_loop
	cmpi r3, 0
	jnz newgamestart
	
:game_over_screen
	ldi r6, 0 ;reset frame counter
:game_over_loop	
	vblnk	
	cls	
	ldi rb, 116 ; Y = 116 (half way down)
	ldi ra, 80  ; X = 120 (half way across)
	ldi r8, game_over_ascii
	call draw_string	
	ldi rb, 130 ; Y = 116 (half way down)
	ldi ra, 80  ; X = 80 (half way across)
	ldi r8, you_scored_ascii
	call draw_string
	ldi r8, current_score
	call draw_counter_numbers
	drw ra, rb, r4			;Draw 8x8 character on the set coordinates
	addi r6, 1  ;add a frame
	cmpi r6, 180 ; wait 3 seconds before restart
	jz init
	jmp game_over_loop
	
;Stolen and modified from Shendo's Music Maker
:draw_string				;Draw string on screen
	spr #0804			;Set 8x8 pixel sprites
	ldm r1, r8			;Load characted from memory
	andi r1, #FF			;Only the lo byte is needed

	cmpi r1, 255			;Remove terminator
	jz returnsub			;Terminator reached, break subroutine

	subi r1, 32
	muli r1, 32			;Each character is 32 bytes long
	addi r1, font		;Apply offset to font address

	drw ra, rb, r1			;Draw 8x8 character on the set coordinates

	addi r8, 1			;Increase memory offset
	addi ra, 9			;Increase X coordinate

	jmp draw_string ;loop around, we have text left!
	
	
:draw_counter_numbers				;Draw numbers on screen
	spr #0804			;Set 8x8 pixel sprites
	ldm r1, r8			;Load characted from memory
	ldi r4, 512			;zero amount
	addi r4, font		;Apply offset to font address
	
	cmpi r1, 1000
	jb counter_under_thousandz
	mov r2, r1  ;copy the number
	divi r2, 1000 ;divide it by 1000 to get number of 1000's
	muli r2, 1000 ;multiply it backup
	sub r1, r2
	divi r2, 1000
	addi r2, 16
	muli r2, 32			;Each character is 32 bytes long
	addi r2, font		;Apply offset to font address
	drw ra, rb, r2			;Draw 8x8 character on the set coordinates
	addi ra, 9			;Increase X coordinate
	jmp counter_under_thousand
	
:counter_under_thousandz
	drw ra, rb, r4			;Draw 8x8 character on the set coordinates
	addi ra, 9			;Increase X coordinate
	
:counter_under_thousand
	cmpi r1, 100
	jb counter_under_hundredz
	mov r2, r1  ;copy the number
	divi r2, 100 ;divide it by 10 to get number of 10's
	muli r2, 100 ;multiply it backup
	sub r1, r2
	divi r2, 100
	addi r2, 16
	muli r2, 32			;Each character is 32 bytes long
	addi r2, font		;Apply offset to font address
	drw ra, rb, r2			;Draw 8x8 character on the set coordinates
	addi ra, 9			;Increase X coordinate
  jmp counter_under_hundred
	
:counter_under_hundredz
	drw ra, rb, r4			;Draw 8x8 character on the set coordinates
	addi ra, 9			;Increase X coordinate
	
:counter_under_hundred
	cmpi r1, 10
	jb counter_under_tenz
	
	mov r2, r1  ;copy the number
	divi r2, 10 ;divide it by 10 to get number of 10's
	muli r2, 10 ;multiply it backup
	sub r1, r2  ;take that value off the original amount
	divi r2, 10
	addi r2, 16
	muli r2, 32			;Each character is 32 bytes long
	addi r2, font		;Apply offset to font address
	drw ra, rb, r2			;Draw 8x8 character on the set coordinates
	
	addi ra, 9			;Increase X coordinate
	jmp counter_under_ten
	
:counter_under_tenz
	drw ra, rb, r4			;Draw 8x8 character on the set coordinates
	addi ra, 9			;Increase X coordinate
:counter_under_ten	
	
	addi r1, 16
	muli r1, 32			;Each character is 32 bytes long
	addi r1, font		;Apply offset to font address

	drw ra, rb, r1			;Draw 8x8 character on the set coordinates

	addi ra, 9			;Increase X coordinate
	ret
	
;Subroutines

:handle_statusbar
	ldi rb, 2 ; Y = 2 (Very top mild offset)
	ldi ra, 2 ; X = 0 (Far left)
	
	ldi r8, player_score_ascii
	call draw_string
	ldi r8, current_score
	call draw_counter_numbers
	drw ra, rb, r4			;Draw 8x8 character on the set coordinates
	
	ldi ra, 120 ; X = 120 (Half way along)
	
	ldi r8, player_level_ascii
	call draw_string
	ldm r1, current_level			;Load characted from memory
	call counter_under_hundred
	
	ldi ra, 240 ; X = 240 (most way along)
	
	ldi r8, player_lives_ascii
	call draw_string
	ldm r1, player_lives			;Load characted from memory
	call counter_under_hundred
	ret
	
:handle_controller
	ldm r0, #FFF0
	tsti r0, 4 ; AND check "left" bit
	jnz move_left
	tsti r0, 8 ; AND check "right" bit
	jnz move_right
	tsti r0, 64 ;AND check "A button" bit
	jnz fire_bullet
	ret
	
:handle_ship
	call draw_ship
	ret
	
:handle_aliens
	call spawn_alien
	call draw_alien
	call move_alien
	ret

:handle_bullet
	ldm r0, bulletfired
	cmpi r0, 1  ;check if a bullet was fired
	jnz returnsub ;it wasnt so escape
	spr #0804  ;width is half of real width 8x8
	ldm ra, bulletposx ;load in bullet x-axis
	ldm rb, bulletposy ;load in bullet y-axis
  subi rb, 8 ;move it up the screen 4 pixels
	stm rb, bulletposy ;store new y position
	ldi r8, spr_bullet ;point to bullet sprite
	drw ra, rb, r8 ; ra=x, rb=y, r8=sprite memory location
	;bullet collision handled on alien
	cmpi rb, 10 ;check if bullet went off the top of the screen
	ja returnsub
	ldi r0, 0
	stm r0, bulletfired ;it did so clear it and store status
	
	ret

:fire_bullet
	ldm r0, bulletfired
	cmpi r0, 1  ;check if a bullet was fired
	jz returnsub
	ldi r0, 1   ;it hasnt so lets set it as fired
	stm r0, bulletfired
	ldi r0, 216  ;set bullet to go from current ship height + 8px
	stm r0, bulletposy
	mov r0, rc  ;get current ship pos
	andi r0, #fff ;filter out the "crashed" part :P tho this shouldnt happen anyway
	addi r0, 7 ;make it look like its coming from the middle of the ship
	stm r0, bulletposx
	ret
		
:move_left	
	mov r0, rc
	andi r0, #fff
	cmpi r0, 0 ;make sure were not at far left as we can go.
	jz returnsub ;if we are, dont move!
	subi rc, 4 ; move left 1 pixel
	ret
	
:move_right
	mov r0, rc
	andi r0, #fff
	cmpi r0, 304 ;make sure were not at far right as we can go.
	jz returnsub ;if we are, dont move!
	addi rc, 4 ; move right 1 pixel
 	ret

 	
:get_status
	ldi r8 alieninfo  ;grab current alien status mem location
  ldi r4, 6        ;alien multiplier
  mul r9, r4, r4   ;multiply "current alien"
  add r8, r4			;add the offset to alien address
  ldm r4, r8			;read it in
  ret
 
:get_coords
	ldi r8 alieninfo  ;grab current alien status mem location
  ldi r4, 6        ;alien multiplier
  mul r9, r4, r4   ;multiply "current alien"
  addi r4, 2			;point to xy location
  add r8, r4			;add the offset to alien address
  ldm r4, r8			;read it in
  ret

:randomize_sprite
	call get_status
	cmpi r4, 3
	jae alien_explode
	subi r4, 1    ;take 1 off the status (incase its 2)
	muli r4, 128  ;create the offset
	mov r1, r8    ;save the alieninfo address
	ldi r8, spr_alien ;point to alien sprite
	add r8, r4    ;point to the correct animation frame
	mov r0, r6    ;grab the current frame counter;
	andi r0, #f   ;check every 15 frames
	cmpi r0, 0 		;see if our masked value is 0
	jnz returnsub
	rnd r5, #0001 ;generate random number (which part of animation)
	addi r5, 1    ;add 1 as we never want zero
	stm r5, r1
	ret

:alien_explode
	mov r0, r4   ;save status
	addi r4, 1	 ;increase to next frame value
	subi r0, 3   ;bring current frame down from offset
	muli r0, 128 ;multiply for offset
	ldi r8, spr_aliendie  ;point to alien death sprite
	add r8, r0
	ldi r2 alieninfo  ;grab current alien status mem location
  ldi r1, 6        ;alien multiplier
  mul r9, r1, r1   ;multiply "current alien"
  add r2, r1			;add the offset to alien address
  cmpi r4, 6
  jz alien_dead
  stm r4, r2		;store the new status
  ret 

:alien_dead
	ldi r4, 0
	stm r4, r2		;store the new status
	
	ret
		
:draw_alien
	spr #1008  ;width is half of real width
	ldi r9, 0   ;set the current alien to beginning
	
:drawalienloop
	call get_status
	cmpi r4, 0    ;test for a status other than 0, if one exists, this alien is here
  jz drawaliennext  ;alien is not active, try the next
  addi r8, 2       ;point it to the y after the status
  ldm rb, r8			;read in y to the register 
  addi r8, 2       ;point it to the x axis
  ldm ra, r8			;read in y to the register 
  call randomize_sprite ;choose an alien sprite
	drw ra, rb, r8 ; ra=x, rb=y, r8=sprite memory location
	jae drawaliennext
	cmpi rb, 208
	jbe alien_killed
	ori rc, #1000
  
:drawaliennext
	addi r9, 1				 ;Increment which alien we're dealing with
	cmpi r9, 15        ;All possible aliens delt with?
	jnz drawalienloop     ;they arent so lets try the next one
	ret

:alien_killed
	ldi r8 alieninfo  ;grab current alien status mem location
  ldi r4, 6        ;alien multiplier
  mul r9, r4, r4   ;multiply "current alien"
  add r8, r4			;add the offset to alien address
  ldi r0, 3       ;alien is killed
  stm r0, r8			;save new status
  ldm r0 bulletfired
  cmpi r0, 0  ;check if there was actually a bullet
  jz drawaliennext ;there wasnt so dont score the player
  addi rd, 1    ;increase score
	stm rd, current_score
  ldi r0, 0
	stm r0, bulletfired ;get rid of the bullet
	jmp drawaliennext
	
:move_alien
	ldi r9, 0   ;set the current alien to beginning
	
:moveloop
  call get_status
  cmpi r4, 0    ;test for a status other than 0, if one exists, this alien is here
  jz movenext  ;alien is not active, try the next
  cmpi r4, 3    ;test for a status other than 0, if one exists, this alien is here
  jae movenext	;alien is dead, try the next
  addi r8, 2       ;point it to the y after the status
  ldm r4, r8			;read it in
  addi r4, 1      ;move our alien down 1 pixel
  stm, r4, r8     ;put it back in memory
  cmpi r4, 240    ;has it gone of the screen?
  jnz movenext		;hasnt gone off screen so dont disable
  subi r8, 2       ;point it to the status for current alien
  ldi r4, 0				;put 0 in temp register
  stm r4, r8       ;set the active flag for current alien to 0
  
:movenext
	addi r9, 1				 ;Increment which alien we're dealing with
	cmpi r9, 15        ;All possible aliens delt with?
	jnz moveloop     ;they arent so lets try the next one
  ret        ;we have our aliens moved!
	
:spawnnext
  addi r9, 1				 ;Increment which alien we're dealing with
	cmpi r9, 15        ;All possible aliens delt with?
	jnz spawn_loop     ;they arent so lets try the next one
  ret        ;we have our aliens!
  
:spawn_alien
	cmp r6, r7 ;see if we've hit the spawn time
	jnz returnsub ;we havent yet so don't spawn
	
	ldi r9, 0   ;set the current alien to beginning
	ldi r6, 0   ;reset spawn timer
	
:spawn_loop	
  call get_status
  cmpi r4, 0    ;test for a status other than 0, if one exists, this alien is here
  jnz spawnnext  ;alien is active, don't spawn one, try the next
  ldi r4, 1      ;set as active
  stm r4, r8 ;store the active status in memory
  addi r8, 2       ;point it to the y after the status
  ldi r4, 10   ;clear reg to 10 pixels down
  stm r4, r8  ;store y axis memory for this alien
  rnd r5, 19 ;generate random number up to 14! (max 19x16=306 x-axis)
  muli r5, 16 ;multiply it to get our x value
  addi r8, 2  ;look at x axis
  stm r5, r8  ;store x axis memory for this alien
  
  jmp returnsub  ;we spawned an alien, so come out
 
:draw_ship
	spr #1008  ;width is half of real width
	ldi rb, 224 ;Ship always drawn at the bottom
	ldi r8, spr_ship ;load mem address of ship sprite in to memory
	mov ra, rc ;Ship x position	
	cmpi ra, #1000
	jb draw_ship_finish
	;we've exploded so change the picture
	mov r0, rc  ;grab ship register again
	shr r0, 12   ;move the bit down
	subi r0, 1  ;align offset
	muli r0, 128 ;multiply the offset
	ldi r8, spr_shipdie ;change to the ship death animation
	add r8, r0   ;add it on to sprite address
	addi rc, #1000 ;increment frame
	
:draw_ship_finish
	andi ra, #fff		
	drw ra, rb, r8 ; ra=x, rb=y, r8=sprite memory location
	ret
	
:returnsub
	ret
	
;Making aliens explode.
;	cmpi rb, 210
;	jnz drawaliennext
;	ldi r8 alieninfo  ;grab current alien status mem location
;  ldi r4, 6        ;alien multiplier
;  mul r9, r4, r4   ;multiply "current alien"
;  add r8, r4			;add the offset to alien address
;  ldi r4, 3
;  stm r4, r8
  
;MADE UP MEMORY BLOCKS

:player_lives
  db #00
  db 255
	
:current_level
  db #00
  db 255
	
:current_score
  db #00
  db 255
  
:bulletfired
	db #00
	db #00

:bulletposx
  db #00
  db #00
  
:bulletposy
  db #00
  db #00
 
:you_scored_ascii
	db "You Scored:"
	db 255
	
:game_over_ascii
	db "Game Over!"
	db 255

:press_start_ascii
	db "PRESS START"
	db 255
	
:player_score_ascii
	db "Score:"
	db 255
:player_lives_ascii
	db "Lives:"
	db 255
	
:player_level_ascii
	db "Level:"
	db 255
	
:spawn_timer
  db 120
 
:zero_amount
	db #00
	db #00
	
:alieninfo
  db #00 ;Alien 1
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 2
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 3
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 4
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 5
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 6
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 7
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 8
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 9
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 10
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 11
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 12
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 13
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 14
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 15
  db #00
  db #00
  db #00
  db #00
  db #00
  db #00 ;Alien 16
  db #00
  db #00
  db #00