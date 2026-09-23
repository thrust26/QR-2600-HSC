; *** Data overlapping optimized by DOOD - v0.91 ***
; (C) 2013 Thomas Jentzsch
; Parameters: d:\Users\Thomas\Documents\Atari\Dev\DASM\Projects\Top_Tom\DOOD v0.91.exe Font.inc -s -c

Four  ; @$0 = 0
  .byte %00000110
  .byte %00000110
  .byte %01111111
  .byte %01111111
  .byte %01100110
  .byte %01110110
  .byte %00110110
  .byte %00111110
  .byte %00011110
  .byte %00011110
DIGIT_H = . - Four

Shield  ; @$a = 10
  .byte %00001000
  .byte %00011100
  .byte %00111110
  .byte %00100110
  .byte %01111011
  .byte %01100011
  .byte %01101111
  .byte %01110011
  .byte %01111111
  .byte %01001001

PowerUp  ; @$14 = 20
  .byte %00100000
  .byte %00010000
  .byte %00011000
  .byte %00001100
  .byte %01111100
  .byte %01110000
  .byte %00111000
  .byte %00111000
  .byte %00011100
;  .byte %00011100
 IF TITLE
  IF BOT_TOM
Letter_C
  .byte %00011100
  .byte %00111110
  .byte %00110110
  .byte %01101011
  .byte %01101111
  .byte %01101111
  .byte %01101011
  .byte %00110110
  .byte %00111110
  .byte %00011100
  ENDIF
 ENDIF

 IF TITLE
Letter_And ; @$77 = 119
  .byte %00111011
  .byte %01111111
  .byte %01101110
  .byte %01101110
  .byte %00111011
  .byte %00111011
  .byte %01101101
  .byte %01101100
  .byte %01111100
;  .byte %01110000
 ENDIF
Seven  ; @$80 = 128
  .byte %00111000
  .byte %00111000
  .byte %00111000
  .byte %00111000
  .byte %00111100
  .byte %00011110
  .byte %00001111
  .byte %00000111
 IF !TITLE
  .byte %01111111
  .byte %01111111
 ELSE
 IF BOT_TOM
Letter_T2
  .byte %01111111
  .byte %01111111
  .byte %01111111
;  .byte %00011100
;  .byte %00011100
;  .byte %00011100
;  .byte %00011100
;  .byte %00011100
;  .byte %00011100
;  .byte %00011100
 ENDIF
Letter_T ; @$1c = 28
  .byte %00011100
  .byte %00011100
  .byte %00011100
  .byte %00011100
  .byte %00011100
  .byte %00011100
  .byte %00011100
  .byte %01111111
;  .byte %01111111
;  .byte %01111111
 ENDIF
Two ; @$24 = 36
  .byte %01111111
  .byte %01111111
  .byte %01111000
  .byte %00111100
  .byte %00011110
  .byte %00001111
  .byte %00000111
  .byte %01100111
  .byte %01111111
;  .byte %00111110
Six ; @$2d = 45
  .byte %00111110
  .byte %01111111
  .byte %01110011
  .byte %01110011
  .byte %01111111
  .byte %01111110
  .byte %01110000
  .byte %01110011
  .byte %01111111
;  .byte %00111110
Three ; @$36 = 54
  .byte %00111110
  .byte %01111111
  .byte %01100111
  .byte %00000111
  .byte %00011110
  .byte %00011110
  .byte %00000111
  .byte %01100111
  .byte %01111111
;  .byte %00111110
Nine  ; @$3f = 63
  .byte %00111110
  .byte %01111111
  .byte %01100111
  .byte %00000111
  .byte %00111111
  .byte %01111111
  .byte %01100111
  .byte %01100111
  .byte %01111111
;  .byte %00111110
Eight ; @$48 = 72
  .byte %00111110
  .byte %01111111
  .byte %01100111
  .byte %01100111
  .byte %00111110
  .byte %00111110
  .byte %01100111
  .byte %01100111
  .byte %01111111
 IF !TITLE
  .byte %00111110
 ELSE
Letter_O ; @$51 = 81
  .byte %00111110
  .byte %01111111
  .byte %01110011
  .byte %01110011
  .byte %01110011
  .byte %01110011
  .byte %01110011
  .byte %01110011
  .byte %01111111
;  .byte %00111110
 ENDIF
Zero  ; @$5a = 90
  .byte %00111110
  .byte %01111111
  .byte %01100011
  .byte %01100011
  .byte %01101011
  .byte %01101011
  .byte %01100011
  .byte %01100011
  .byte %01111111
;  .byte %00111110
Five ; @$63 = 99
  .byte %00111110
  .byte %01111111
  .byte %00000111
  .byte %00000111
  .byte %01111110
  .byte %01111110
  .byte %01100000
  .byte %01100000
  .byte %01111111
  .byte %01111111

 IF TITLE
Letter_M ; @$6d = 109
  IF TOP_TOM
  .byte %11011011
  .byte %11011011
  .byte %11111111
  .byte %11111111
  .byte %11111111
  .byte %11111111
  .byte %11100111
  .byte %11100111
  .byte %11000011
  .byte %11000011
Letter_P ; @$8a = 138
  .byte %01110000
  .byte %01110000
  .byte %01110000
  .byte %01111110
  .byte %01111111
  .byte %01110011
  .byte %01110011
  .byte %01110011
  .byte %01111111
  .byte %01111110
  ENDIF

  IF BOT_TOM
Letter_M
  .byte %11000011
  .byte %11000011
  .byte %11100111
  .byte %11100111
  .byte %11111111
  .byte %11111111
  .byte %11111111
  .byte %11111111
  .byte %11011011
  .byte %11011011

Letter_B
  .byte %01111110
  .byte %01111111
  .byte %01110011
  .byte %01110011
  .byte %01111110
  .byte %01111110
  .byte %01110011
  .byte %01110011
  .byte %01111111
  .byte %01111110
  ENDIF
 ENDIF

One ; @$94 = 148
  .byte %00001100
  .byte %00001100
  .byte %00001100
  .byte %00001100
  .byte %00001100
  .byte %00001100
  .byte %00001100
  .byte %00111100
  .byte %00111100
;  .byte %00011100
RandomSym ; @$9d = 157
  .byte %00011100
  .byte %00011100
  .byte %00000000
  .byte %00011100
  .byte %00011110
  .byte %00001111
  .byte %00000111
  .byte %01100111
  .byte %01111111
  .byte %00111110
                       ; @$a7 = 167
; 167/180 (92.8%)
 IF COMBO_BONUS
Letter_x
  .byte %01100110
  .byte %01100110
  .byte %01111110
  .byte %00011000
  .byte %00011000
  .byte %01111110
  .byte %01100110
  .byte %01100110
  .byte %00000000
  .byte %00000000
 ENDIF

