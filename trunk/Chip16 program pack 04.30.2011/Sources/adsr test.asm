start:
sng 168, 61688  ;triangle a = 10 d = 8 v = 15 s = 15 r = 8 800ms + release = 1550 ms
ldi r1, 1000 ;1000hz
call playnote
sng 168, 61944	;sawtooth a = 10 d = 8 v = 15 s = 15 r = 8 800ms + release = 1550 ms
call playnote
sng 168, 62200	;pulse    a = 10 d = 8 v = 15 s = 15 r = 8 800ms + release = 1550 ms
call playnote
sng 168, 62456	;noise    a = 10 d = 8 v = 15 s = 15 r = 8 800ms + release = 1550 ms
call playnote
jmp start

:playnote
snp r1, 800 ;play 1000hz for 800ms (plus release)
ldi r2, 200
call waiting
ret

:waiting
vblnk
subi r2, 1
jnz waiting
ret