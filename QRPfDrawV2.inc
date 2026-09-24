  MAC QR_DRAW_CODE
QR_BLOCK_H  = 7
.tmpFirst   = qrTmpVars     ; leftmost pixel column (-> M1), ZP-RAM!
.tmpFirst1  = qrTmpVars+1   ; ZP-RAM!
.tmpFirst2  = qrTmpVars+2   ; ZP-RAM!

_qrDrawCode
; |PF0 |  PF1   |  PF2   |  PF2   |  PF1   |PF0 |
; |....|...xxxxx|xxxxxxxx|xxxxxxxx|xxxx....|....|
.pf0R1LLst  = grp0LLst
.pf2LLst    = grp1Lst
.pf1RLst    = grp0RLst

    lda     #0              ; black QR code...
    sta     COLUPF
    lda     #$0e            ; ...on white background
    sta     COLUBK

; some vertical centering
    ldx     #{1}    ;(200-QR_SIZE*QR_BLOCK_H)/2
.waitTop
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitTop
    stx     CTRLPF

    lda     #%01111111      ;           = $7f
    sta     .tmpFirst
    lda     firstMsl
   IF QR_LEVEL = 0 || QR_LEVEL = 1
    sec                     ;           top eye, 1st format bit is 1
    rol
   ELSE
    asl                     ;           top eye, 1st format bit is 0
   ENDIF
    sta     .tmpFirst1
    lda     #%01111110      ;           = $7e
    rol                     ;           = %1111110x
    sta     .tmpFirst2
; QR code display kernel:
    ldx     #QR_SIZE-1
    bne     .loopQrKernel

; QR code display kernel:
.loopBlock
    SLEEP   2
    lda     #0              ; 2
    sta     PF2             ; 3         @43/44
    sta     PF0             ; 3         @46/47
    NOP_W                   ; 2 = 10
.loopQrKernel               ;           @68/69
    ldy     #QR_BLOCK_H     ; 2
    sta     WSYNC           ; 3 =  5
;---------------------------------------
; |PF0 |  PF1   |  PF2   |PF0 |  PF1   |  PF2   |
; |    |7......0|0......7|4..7|7......0|        |
; |....|...XXXXX|XXXXXXXX|XXXX|XXXXXXXX|........|
    lda     .tmpFirst       ; 3
    lsr                     ; 2
    lda     .pf0R1LLst,x    ; 4
    and     #%1111          ; 2
    bcc     .clear          ; 2/3
    ora     #%10000         ; 2         CF needed for 1st ror
.clear                      ;   = 14/15
    sta     PF1             ; 3         @17/18
    lda     .pf2LLst,x      ; 4
    sta     PF2             ; 3 = 10    @24/25
    lda     .pf0R1LLst,x    ; 4
    sta     PF0             ; 3         @31/32  >=27
    lda     .pf1RLst,x      ; 4
    sta     PF1             ; 3 = 14    @38/39  >=38
    dey                     ; 2
    bne     .loopBlock      ; 3/2= 5/4  @43/44
    ror     .tmpFirst2      ; 5         shift bits into .tmpFirst
    ror     .tmpFirst1      ; 5
    ror     .tmpFirst       ; 5
    sty     PF2             ; 3
    sty     PF0             ; 3 = 21    @63/64
    dex                     ; 2
    bpl     .loopQrKernel   ; 3/2= 5/4  @68/69
    sty     PF1             ;           @71

    ldx     #{2}
.waitBtm
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitBtm
    ECHO    "    QR Code PF kernel:", [. - _qrDrawCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrDrawCode
  ENDM ; /QR_DRAW_CODE

  MAC _QR_ARRANGE_DRAW_DATA
TIM_AS_S
   IF !QR_SPRITE_GFX
; rearrange bitmap data for PF display
; |PF0 |  PF1   |  PF2   |PF0 |  PF1   |  PF2   |
; |    |7......0|0......7|4..7|7......0|        |
; |....|...XXXXX|XXXXXXXX|XXXX|XXXXXXXX|........|
;           |  P0L   |   P1   |  P0R   |
;          0|abcdefgh|ijklmnop|qrstuvwx| ->
;       -> 0|ponmabcd|lkjihgfe|qrstuvwx|
.tmpLeft    = qrTmpVars

    ldx     #QR_SIZE-1
.loopRows
; rearrange grp0LLst & grp1Lst into pf0R1LLst
    lda     grp0LLst,x
    sta     .tmpLeft
    lda     grp1Lst,x
    ldy     #4
.loopShift01a
    lsr                     ; 3..0 -> 0..3
    rol     grp0LLst,x
    dey
    bne     .loopShift01a
    lda     .tmpLeft
    ldy     #4
.loopShift01b
    asl
    rol     grp0LLst,x
    dey
    bne     .loopShift01b
; rearrange grp0LLst & grp1Lst into pf2LLst
    lda     grp1Lst,x
    lsr
    lsr
    lsr
    lsr
    ldy     #4
.loopShift2a
    lsr
    rol     grp1Lst,x
    dey
    bne     .loopShift2a
    lda     .tmpLeft
    ldy     #4
.loopShift2b
    lsr
    rol     grp1Lst,x
    dey
    bne     .loopShift2b
; loop
    dex
    bpl     .loopRows
   ENDIF ; / !QR_SPRITE_GFX
TIM_AS_E
  ENDM

  MAC QR_DRAW_DATA
_qrFuncData
    include QRFuncDataV2S0.inc ; special pattern

    ECHO    "QR Code function modules data:", [. - _qrFuncData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrFuncData
  ENDM

