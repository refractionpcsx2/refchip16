; Bruised - Fighting Game by Refraction
;
; r0 = General Purpose
; r1 = General Purpose 
; r2 = General Purpose
; r3 = General Purpose
; r4 = General Purpose
; r5 = General Purpose (Random Number or Controller Destination)
; r6 = Current Sprite X-Axis / General Purpose
; r7 = Current Sprite Y-Axis / General Purpose
; r8 = Current Sprite Memory Location / General Purpose
; r9 = Player Status - 0 for nothing, 1 for walking left, 2 walking right, 4 punching, 8 kicking, 16 blocking
; ra = CPU Status    - 0 for nothing, 1 for walking left, 2 walking right, 4 punching, 8 kicking, 16 blocking
; rb = Player X Position
; rc = CPU X Position
; rd = Player Health
; re = CPU Health
; rf = Fight Status - 0 for fighting, 1 is Player Won, 2 is Cpu Won.

;Title Screen
importbin fist.bin 0 3420 spr_fist	           ;spr #721E
importbin logo.bin 0 4000 spr_logo   					 ;spr #3250
importbin font.bin 0 3072 font								 ;spr #0804

;General Ingame Sprites
importbin topbar.bin 0 2400 spr_topbar	       ;spr #0FA0
importbin healthblock.bin 0 8 spr_healthblock  ;spr #0801
importbin floor.bin 0 480 spr_floor	       		 ;spr #1E10

;Player and CPU Character Sprites
importbin body.bin 0 306 spr_body 	       		 ;spr #2209
importbin head.bin 0 270 spr_head	       		   ;spr #1E09
importbin legskick.bin 0 3822 spr_legskick     ;spr #3127 + offset
importbin legswalk.bin 0 5733 spr_legswalk     ;spr #3127 + offset
importbin stood.bin 0 1911 spr_stood     			 ;spr #3127
importbin arms.bin 0 714 spr_arms     			   ;spr #2215
importbin armspunch.bin 0 1428 spr_armspunch   ;spr #2215
importbin armsblock.bin 0 1428 spr_armsblock   ;spr #2215

:init
	ldi r9, 0    ;Player is doing nothing
	ldi ra, 0    ;CPU is doing nothing
	ldi rb, 20   ;put Player position on the left side of the screen
	ldi rc, 242  ;put CPU position on the right hand side of the screen
	ldi rd, 100  ;set player health to 100
	ldi re, 100  ;set cpu health to 100
	ldi rf, 0    ;fight isn't over, it hasn't started either!
	ldi r0, 0		 ;used for the flashing "PRESS START" on the menu
	ldi r4, 0		 ;used in the cpu "AI"

:title_screen
	cls
	spr #721E				 		;Set the size to that of our fist
	ldi r6, 20       		;X - first fist goes on the left side of the screen
	ldi r7, 63  		 		;Y - centre it vertically
	ldi r8, spr_fist 		;Mem - point to our fist sprite
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ldi r6, 240      		;X - second fist goes on the right side of the screen
	ldi r7, 63  		 		;Y - centre it vertically
	flip 1, 0						;flip the second fist so it looks like another arm ;p
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	flip 0, 0						;Set drawing back to normal
	spr #3250				 		;Set the size to that of our logo
	ldi r6, 80       		;X - Logo goes in the middle (this is next to the first fist)
	ldi r7, 70  		 		;Y - put it between the fists
	ldi r8, spr_logo 		;Mem - point to our logo sprite
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	cmpi r0, 30					;check if we should skip drawing it (every 1/2 a second)
	jae skipped_start_draw ;if it's above, don't print it
	ldi r6, 107      		;X - Start goes under the logo
	ldi r7, 150  		 		;Y - put it between the fists
	ldi r8, ascii_start ;Mem - point to our "Press Start" text
	call draw_string    ;draw the text!
	
:skipped_start_draw
	ldi r6, 130      		;X - Author goes in the bottom right
	ldi r7, 230  		 		;Y - put it down the bottom
	ldi r8, ascii_author ;Mem - point to our fist sprite
	call draw_string    ;draw the text!
	vblnk
	ldm r3, #FFF0				;Load the controller state
	tsti r3, #10				;Test the Start button
	jnz init_newgame		;Start the game off!!
	addi r0, 1					;Add a frame to our press start blinky counter
	cmpi r0, 60					;we want to reset it on 60, so we might as well jump to the init
	jz init							;if we've done 60 frames, start it again
	jmp title_screen 		;otherwise loop around
	
:init_newgame
	cls
	call draw_elements
	ldi r0, 0								;prep a temp reg to count frames
	ldi r6, 107      			 ;X - Start goes under the logo
	ldi r7, 100  		 			 ;Y - put it between the fists
	ldi r8, ascii_getready ;Mem - point to our get ready text	
	call draw_string       ;draw the text!
	;call wait_two_seconds	 ;wait the 2 seconds
	cls
	call draw_elements
	ldi r0, 0								;prep a temp reg to count frames
	ldi r6, 130      			 ;X - Start goes under the logo
	ldi r7, 100  		 			 ;Y - put it between the fists
	ldi r8, ascii_fight    ;Mem - point to our get ready text	
	call draw_string       ;draw the text!
	;call wait_two_seconds	 ;wait the 2 seconds	

:game_loop
	cls
	call cpu_check_elements
	call check_controls
	call check_punch
	call check_kick	
	call draw_elements
	call check_health
	vblnk
	cmpi rf, 0
	jnz game_finished
	jmp game_loop

:game_finished
	cmpi rf, 1
	jnz cpu_win
	jmp player_win
	
:cpu_win
	cls
	call draw_elements
	ldi r0, 0								;prep a temp reg to count frames
	ldi r6, 107      			 ;X - Start goes under the logo
	ldi r7, 70  		 			 ;Y - put it between the fists
	ldi r8, ascii_cpuwin	 ;Mem - point to our get ready text	
	call draw_string       ;draw the text!
	call wait_two_seconds	 ;wait the 2 seconds
	jmp init
	
:player_win
	cls
	call draw_elements
	ldi r0, 0								;prep a temp reg to count frames
	ldi r6, 107      			 ;X - Start goes under the logo
	ldi r7, 70  		 			 ;Y - put it between the fists
	ldi r8, ascii_playerwin ;Mem - point to our get ready text	
	call draw_string       ;draw the text!
	call wait_two_seconds	 ;wait the 2 seconds
	jmp init

:check_health
	cmpi re, 0
	jnz check_player
	ldi rf, 1
:check_player
	cmpi rd, 0
	jnz returnsub
	ldi rf, 2
	ret
	
:draw_elements
	call draw_topbar
	call draw_player_health
	call draw_cpu_health
	call draw_floor
	call draw_player
	call draw_computer
	ret

;*****************************CONTROLLER STUFF**********************
:check_controls
	andi r9, #1c						;set ourselves as not walking
	ldm r3, #FFF0				;Load the controller state
	cmpi r3, #C0
	jz player_blocking
	tsti r3, #8					;Test the Right button
	jnz move_right
	tsti r3, #8					;Test the Right button
	jnz move_right
	tsti r3, #4					;Test the Left button
	jnz move_left
	andi r9, #c
	ret
	
:move_right
	cmpi r9, 4
	jae returnsub
	ldi r9, 2  		;set as walking right
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 35	
	cmp r1, r0
	jae returnsub
	addi rb, 1
	ret
:move_left
	cmpi r9, 4
	jae returnsub
	ldi r9, 1  		;set as walking left
	cmpi rb, 20
	jz returnsub
	subi rb, 1
	ret
	
:player_blocking
	cmpi r9, 4
	jnz returnsub
	ldi r0, 1
	stm r0, player_frame
	ldi r9, 16  		;set as blocking
	ret 
	
:check_punch
	tsti r3, #40					;Test the A button for punching
	jnz player_punch
	call clear_player_punch
	ret
:check_kick
	tsti r3, #80					;Test the B button for punching
	jnz player_kick
	call clear_player_kick
	ret
	
:clear_player_punch
	cmpi r9, 4
	jnz returnsub
	ldm r0, player_frame
	cmpi r0, 9
	jnz returnsub
	ldi r0, 0
	ldi r9, 0
	stm r0, player_frame
	ret

:player_punch
	cmpi r9, 4
	jae returnsub
	ldi r9, 4  		;set as punching
	ldi r0, 1
	stm r0, player_frame
	cmpi ra, 16   ;see if cpu is blocking
	jz returnsub
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 45
	cmp r0, r1
	jae returnsub	
	subi re, 2   ;reduce cpu health
	ret

:clear_player_kick
	cmpi r9, 8
	jnz returnsub
	ldm r0, player_frame
	cmpi r0, 9
	jnz returnsub
	ldi r0, 0
	ldi r9, 0
	stm r0, player_frame
	ret
	
:player_kick
	cmpi r9, 4
	jae returnsub
	ldi r9, 8  		;set as kicking
	ldi r0, 1
	stm r0, player_frame
	cmpi ra, 16   ;see if cpu is blocking
	jz returnsub
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 45
	cmp r0, r1
	jae returnsub	
	subi re, 2   ;reduce cpu health
	ret

;********************PLAYER STUFF***************************************
:draw_player
	call draw_playerlegs
	call draw_playerheadbody
	call draw_playerarms
	ret
	
:draw_playerlegs
	spr #3127
	
	cmpi r9, 8
	jnz draw_normallegs
	jmp draw_player_kick
:draw_normallegs
	tsti r9, 3	
	jz playerstood
:playerwalking
	ldi r8, spr_legswalk  ;start with our legs
	ldm r0, player_frame
	cmpi r9, 2
	jnz playerwalkingleft
:playerwalkingright
	addi r0, 1
	andi r0, #1f
	cmpi r0, #1b
	jnz store_legs_frame
	ldi r0, 0
	jmp store_legs_frame
:playerwalkingleft
	cmpi r9, 1
	jnz playerstood
	subi r0, 1
	andi r0, #1f
	cmpi r0, #1f
	jnz store_legs_frame
	ldi r0, #1a
	jmp store_legs_frame
:playerstood
	ldi r8, spr_stood  ;start with our legs
	ldi r0, 0
	jmp draw_playerlegs_end
:store_legs_frame
	stm r0, player_frame
:draw_playerlegs_end
			
	divi r0, 9
	muli r0, 	1911			;point to correct frame
	add r8, r0
	mov r6, rb						;copy our player x position
	ldi r7, 161						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret

:draw_player_kick
	ldi r8, spr_legskick
	ldm r0, player_frame
	cmpi r0, 9
	jz playerstood
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 1911			;point to correct frame
	add r8, r1
	mov r6, rb						;copy our player x position
	ldi r7, 161						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, player_frame
	ret
	
:draw_playerheadbody
	spr #2209						;body
	ldi r8, spr_body  ;start with our legs
	mov r6, rb						;copy our player x position
	addi r6, 30
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	spr #1E09						;head
	ldi r8, spr_head  ;start with our legs
	mov r6, rb						;copy our player x position
	addi r6, 32
	ldi r7, 97						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret
	
:draw_playerarms	
	cmpi r9, 4
	jnz draw_playertestblockarms
	jmp draw_player_punch	
:draw_playertestblockarms
	cmpi r9, 16
	jnz draw_normalarms
	jmp draw_player_block
:draw_normalarms
	spr #2215
	ldi r8, spr_arms
:draw_arms_routine
	mov r6, rb						;copy our player x position
	addi r6, 32
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret

:draw_player_block
	spr #2215
	ldi r8, spr_armsblock
	ldm r0, player_frame
	cmpi r0, 6
	jz draw_arms_routine
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 714
	add r8, r1
	cmpi r0, 6
	jz draw_arms_routine	
	mov r6, rb						;copy our player x position
	addi r6, 32
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, player_frame
	ret
	
:draw_player_punch
	spr #2215
	ldi r8, spr_armspunch
	ldm r0, player_frame
	cmpi r0, 9
	jz draw_normalarms
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 714
	add r8, r1
	mov r6, rb						;copy our player x position
	addi r6, 34
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, player_frame
	ret

;*****************************CPU AI STUFF**************************
:cpu_check_elements
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 45
	cmp r0, r1
	jc cpu_check_elements_nexttoplayer
	cmpi r4, 0
	ja cpu_process
	andi ra, #c
	rnd r4, 30			;pick a random number of frames to do something for.
	rnd r5, 4     ;generate a number between 0-4
	jmp cpu_check_elements
cpu_process:	
	subi r4, 1	
	cmpi r5, 2
	jae cpu_move_left
	cmpi r5, 0
	jz cpu_move_right
	ret

:cpu_check_elements_nexttoplayer
	cmpi r4, 0
	ja cpu_process_attacks
	andi ra, #1c
	rnd r4, 10			;pick a random number of frames to do something for.
	rnd r5, 4     ;generate a number between 0-4
	jmp cpu_check_elements_nexttoplayer
cpu_process_attacks:	
	subi r4, 1	
	cmpi r5, 2
	jz cpu_kick
	call clear_cpu_kick
	cmpi r5, 3
	jz cpu_punch
	call clear_cpu_punch
	cmpi r5, 4
	jae cpu_block
	andi ra, #c
	ret
	
:cpu_block
	cmpi ra, 4
	jae returnsub
	cmpi ra, 8
	jz returnsub
	ldi ra, 16  		;set as blocking
	ret
	
:clear_cpu_punch
	cmpi ra, 4
	jnz returnsub
	ldm r0, cpu_frame
	cmpi r0, 9
	jnz returnsub
	ldi r0, 0
	ldi ra, 0
	stm r0, cpu_frame
	ret

:cpu_punch
	cmpi ra, 4
	jae returnsub
	ldi r4, 9
	ldi ra, 4  		;set as punching
	ldi r0, 1
	stm r0, cpu_frame	
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 45
	cmp r0, r1
	jae returnsub	
	cmpi r9, 16   ;see if player is blocking
	jnz reduce_p_health
	addi r4, 20
	ldi r5, 3			;stop the cpu attacking a moment
	ret
	
:reduce_p_health
	subi rd, 2   ;reduce player health
	ret
	
:cpu_move_right
	cmpi ra, 4
	jae returnsub
	ldi ra, 2  		;set as walking right
	cmpi rc, 242
	jz returnsub
	addi rc, 1
	ret
	
:cpu_move_left
	cmpi ra, 4
	jae returnsub
	ldi ra, 1  		;set as walking left
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 35	
	cmp r1, r0
	jae returnsub
	subi rc, 1
	ret
	
:clear_cpu_kick
	cmpi ra, 8
	jnz returnsub
	ldm r0, cpu_frame
	cmpi r0, 9
	jnz returnsub
	ldi r0, 0
	ldi ra, 0
	stm r0, cpu_frame
	ret
	
:cpu_kick
	cmpi ra, 4
	jae returnsub
	ldi r4, 9
	ldi ra, 8  		;set as kicking
	ldi r0, 1
	stm r0, cpu_frame
	mov r0, rc		;find the cpu position
	mov r1, rb		;grab our current position
	subi r0, 45
	cmp r0, r1
	jae returnsub	
	cmpi r9, 16   ;see if player is blocking
	jnz reduce_p_health
	addi r4, 20
	ldi r5, 3			;stop the cpu attacking a moment
	ret
	
;********************CPU STUFF***************************************
:draw_computer
	flip 1, 0
	call draw_cpulegs
	call draw_cpuheadbody
	call draw_cpuarms
	flip 0, 0
	ret
	
:draw_cpulegs
	spr #3127
	
	cmpi ra, 8
	jnz draw_normalcpulegs
	jmp draw_cpu_kick
:draw_normalcpulegs
	tsti ra, 3	
	jz cpustood
:cpuwalking
	ldi r8, spr_legswalk  ;start with our legs
	ldm r0, cpu_frame
	cmpi ra, 2
	jnz cpuwalkingleft
:cpuwalkingright
	subi r0, 1
	andi r0, #1f
	cmpi r0, #1f
	jnz store_cpulegs_frame
	ldi r0, #1a
	jmp store_cpulegs_frame
:cpuwalkingleft
	cmpi ra, 1
	jnz cpustood
	addi r0, 1
	andi r0, #1f
	cmpi r0, #1b
	jnz store_cpulegs_frame
	ldi r0, 0
	jmp store_cpulegs_frame
:cpustood
	ldi r8, spr_stood  ;start with our legs
	ldi r0, 0
	jmp draw_cpulegs_end
:store_cpulegs_frame
	stm r0, cpu_frame
:draw_cpulegs_end
			
	divi r0, 9
	muli r0, 	1911			;point to correct frame
	add r8, r0
	mov r6, rc						;copy our CPU x position
	ldi r7, 161						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret

:draw_cpu_kick
	ldi r8, spr_legskick
	ldm r0, cpu_frame
	cmpi r0, 9
	jz cpustood
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 1911			;point to correct frame
	add r8, r1
	mov r6, rc						;copy our player x position
	ldi r7, 161						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, cpu_frame
	ret
	
:draw_cpuheadbody
	spr #2209						;body
	ldi r8, spr_body  ;start with our legs
	mov r6, rc						;copy our cpu x position
	addi r6, 30
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	spr #1E09						;head
	ldi r8, spr_head  ;start with our legs
	mov r6, rc						;copy our cpu x position
	addi r6, 28
	ldi r7, 97						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret
	
:draw_cpuarms	
	cmpi ra, 4
	jnz draw_cputestblockarms
	jmp draw_cpu_punch	
:draw_cputestblockarms
	cmpi ra, 16
	jnz draw_cpunormalarms
	jmp draw_cpu_block
:draw_cpunormalarms
	spr #2215
	ldi r8, spr_arms
:draw_cpuarms_routine
	mov r6, rc						;copy our cpu x position
	addi r6, 3
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret

:draw_cpu_block
	spr #2215
	ldi r8, spr_armsblock
	ldm r0, cpu_frame
	cmpi r0, 6
	jz draw_cpuarms_routine
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 714
	add r8, r1
	cmpi r0, 6
	jz draw_cpuarms_routine		
	mov r6, rc						;copy our cpu x position
	addi r6, 3
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, cpu_frame
	ret
	
:draw_cpu_punch
	spr #2215
	ldi r8, spr_armspunch
	ldm r0, cpu_frame
	cmpi r0, 9
	jz draw_cpunormalarms
	mov r1, r0
	addi r0, 1
	subi r1, 1
	divi r1, 3
	andi r1, 1
	muli r1, 714
	add r8, r1
	mov r6, rc						;copy our cpu x position
	addi r6, 2
	ldi r7, 127						;top of the legs plus the floor
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	stm r0, cpu_frame
	ret
	
;********************HUD STUFF***************************************
:wait_two_seconds	
	addi r0, 1		 ;add one frame to our counter
	vblnk
	cmpi r0, 120    ;wait for 120 frames
	jnz wait_two_seconds
	ret
	
:draw_topbar
	spr #0FA0						;size of the top bar
	ldi r6, 0      			;X - Top bar is the full length
	ldi r7, 0  		 			;Y - Stick it at the top
	ldi r8, spr_topbar  ;Mem - point to our topbar sprite
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	ret
	
:draw_floor
	spr #1E10						;size of the floor
	ldi r6, 0      			;X - Start from the far left
	ldi r7, 210  		 		;Y - Stick it at the bottom - sprite height (30)
	ldi r8, spr_floor   ;Mem - point to our floor sprite
:draw_floor_loop
	drw r6, r7, r8 	 		;r6=x, r7=y, r8=sprite memory location
	addi r6, 32					;move over the width of our sprite
	cmpi r6, 320				;see if we've hit the screen edge
	jz returnsub				;we have so exit
	jmp draw_floor_loop ;we still have space to draw so keep looping
	
:draw_player_health
	ldi r6, 9      						;X - Start from the far left
	ldi r7, 4   		 					;Y - Stick it at the bottom - sprite height (30)
	mov r0, rd								;Copy the players health to a temporary reg for looping
	jmp draw_health_blocks
	
:draw_cpu_health	
	ldi r6, 311      					;X - Start from the far left
	sub r6, re								;take off the health so it starts in the right place
	ldi r7, 4   		 					;Y - Stick it at the bottom - sprite height (30)
	mov r0, re								;Copy the CPUs health to a temporary reg for looping
	jmp draw_health_blocks
	
:draw_health_blocks
		spr #0801
		ldi r8, spr_healthblock   ;Mem - point to our floor sprite
:draw_health_blocks_loop
		cmpi r0, 0 							;see if its gone out of the bar
		jz returnsub 						;it hasn't so keep drawing!
		drw r6, r7, r8 	 						;r6=x, r7=y, r8=sprite memory location
		subi r0, 2									;move over the width of our health sprite (2)
		addi r6, 2									;move the X over to the next position
		jmp draw_health_blocks_loop


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
	
:ascii_start
	db "PRESS START"
	db 255

:ascii_author
	db "Written by Refraction"
	db 255

:ascii_getready
	db "Get Ready..."
	db 255
	
:ascii_fight
	db "Fight!"
	db 255

:ascii_playerwin
	db "Player wins!"
	db 255

:ascii_cpuwin
	db "CPU wins!"
	db 255
	
:player_frame
	db #00
	db #00
	
:cpu_frame
	db #00
	db #00