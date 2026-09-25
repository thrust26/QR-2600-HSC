_MASK_IDX SET 0

  MAC _AM   ; pattern mask, value
; apply mask 0
   LIST OFF
   IF _MASK_IDX
    LIST ON
    .byte   ({2}) ^ ($aa & {1})
    LIST OFF
   ELSE
    LIST ON
    .byte   ({2}) ^ ($55 & {1})
    LIST OFF
   ENDIF
_MASK_IDX SET _MASK_IDX ^ 1
   LIST ON
  ENDM

  MAC FUNC_GFX
FuncGfx
;GRP0LFunc
    _AM     %00000000, %11111100 | (({1} >> 7) & %1) ; constant, bit 0 of 2nd format copy, level
    _AM     %00000000, %00000100 | (({1} >> 6) & %1) ; constant, bit 1 of 2nd format copy, level
    _AM     %00000000, %01110100 | (({1} >> 5) & %1) ; constant, bit 2 of 2nd format copy, pattern
    _AM     %00000000, %01110100 | (({1} >> 4) & %1) ; constant, bit 3 of 2nd format copy, pattern
    _AM     %00000000, %01110100 | (({1} >> 3) & %1) ; constant, bit 4 of 2nd format copy, pattern
    _AM     %00000000, %00000100 | (({1} >> 2) & %1) ; constant, bit 5 of 2nd format copy, ECC
    _AM     %00000000, %11111100 | (({1} >> 1) & %1) ; constant, bit 6 of 2nd format copy, ECC
    _AM     %00000000, %00000001    ;                  constant, 1 (dark module)
    _AM     %11111011, %00000100    ;  8
    _AM     %11111011, %00000000
    _AM     %11111011, %00000100    ; 10
    _AM     %11111011, %00000000
    _AM     %11111011, %00000100    ; 12
    _AM     %11111011, %00000000
    _AM     %11111011, %00000100    ; 14
    _AM     %11111011, %00000000
    _AM     %00000000, (({1} << 1) & %11111000) | (({1} & %11) | %100) ; constant, bits 1..7 of 1st format copy, 1 (timing bit)
    _AM     %00000000, %00000000 | (({2} >> 7) & %1) ; constant, bit  8 of 1st format copy, ECC
    _AM     %00000000, %11111101    ; 18               constant, 1 (timing bit)                                     ; 18
    _AM     %00000000, %00000100 | (({2} >> 6) & %1) ; constant, bit  9 of 1st format copy, ECC
    _AM     %00000000, %01110100 | (({2} >> 5) & %1) ; constant, bit 10 of 1st format copy, ECC ; 20
    _AM     %00000000, %01110100 | (({2} >> 4) & %1) ; constant, bit 11 of 1st format copy, ECC
    _AM     %00000000, %01110100 | (({2} >> 3) & %1) ; constant, bit 12 of 1st format copy, ECC ; 22
    _AM     %00000000, %00000100 | (({2} >> 2) & %1) ; constant, bit 13 of 1st format copy, ECC
    _AM     %00000000, %11111100 | (({2} >> 1) & %1) ; constant, bit 14 of 1st format copy, ECC ; 24
;FirstFunc
;_MASK_IDX SET _MASK_IDX ^ 1
    _AM     %11111111, %00000000
;_MASK_IDX SET _MASK_IDX ^ 1
;GRP1Func
    _AM     %11111111, %00000000    ;  0
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ;  2
    _AM     %11111111, %00000000
    _AM     %11111110, %00000001    ;  4    alignment pattern
    _AM     %11111110, %00000001
    _AM     %11111110, %00000001    ;  6
    _AM     %11111110, %00000001
    _AM     %11111110, %00000001    ;  8
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 10
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 12
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 14
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 16
    _AM     %11111111, %00000000
    _AM     %00000000, %01010101    ; 18    horizontal timing pattern
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 20
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 22
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 24
_MASK_IDX SET _MASK_IDX ^ 1
;GRP0RFunc
    _AM     %11111111, %00000000    ;  0
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ;  2
    _AM     %11111111, %00000000
    _AM     %00001111, %11110000    ;  4    alignment pattern
    _AM     %00001111, %00010000
    _AM     %00001111, %01010000    ;  6
    _AM     %00001111, %00010000
    _AM     %00001111, %11110000    ;  8
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 10
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 12
    _AM     %11111111, %00000000
    _AM     %11111111, %00000000    ; 14
    _AM     %11111111, %00000000
    _AM     %00000000, (({1} << 7) & $80) | ({2} >> 1) ; bits 7..14 of 2nd format copy
    _AM     %00000000, %00000000    ;       constant    top, right "eye"
    _AM     %00000000, %01111111    ; 18    constant
    _AM     %00000000, %01000001    ;       constant
    _AM     %00000000, %01011101    ; 20    constant
    _AM     %00000000, %01011101    ;       constant
    _AM     %00000000, %01011101    ; 22    constant
    _AM     %00000000, %01000001    ;       constant
    _AM     %00000000, %01111111    ; 24    constant
  ENDM

  IF QR_LEVEL = 0
    FUNC_GFX %11101111, %10001000
  ENDIF
  IF QR_LEVEL = 1
    FUNC_GFX %10101000, %00100100
  ENDIF
