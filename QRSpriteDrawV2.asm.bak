
  MAC QR_DRAW_CODE
; Display: M1, P0a, P1, P0b (25 pixel)
QR_BLOCK_H  = 2     ; QR code pixel height
.tmpFirst   = qrTmpVars     ; leftmost pixel column (-> M1), ZP-RAM!
.tmpFirst1  = qrTmpVars+1   ; ZP-RAM!
.tmpFirst2  = qrTmpVars+2   ; ZP-RAM!

_qrDrawCode
; reset some major TIA registers if required:
;    lda     #0
;    sta     NUSIZ1
;    sta     VDELP0

; Note: other color combinations work too, as long as the contrast is high enough
    ldx     #$00        ; black QR code...
    sta     WSYNC
;---------------------------------------
    lda     #$0e        ; ...on white background
    sta     COLUBK
    stx     COLUP0
    stx     COLUP1
    lda     #%001|$80
    sta     NUSIZ0
    sta     HMM1
    ldx     #$1f
    stx     HMP0
    inx
    stx     HMP1
    php                 ; waste 7 cycles
    plp
    ldx     #{1}
    sta     RESM1
    sta     RESP0
    sta     RESP1

    sta     WSYNC
;---------------------------------------
    sta     HMOVE

.loopWaitTop
    dex
    sta     WSYNC
;---------------------------------------
    bne     .loopWaitTop

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
.loopQrKernel               ;           @70*
    ldy     #QR_BLOCK_H     ; 2 = 2
.loopBlock
    sta     WSYNC           ; 3 = 3     @75*
;---------------------------------------
;M1-P0-P1-P0
    lda     .tmpFirst       ; 3
    asl                     ; 2
    sta     ENAM1           ; 3 =  8
    lda     grp1Lst,x       ; 4
    sta     GRP1            ; 3
    lda     grp0LLst,x      ; 4
    sta     GRP0            ; 3 = 14
    php                     ; 3         waste 14 cycles
    plp                     ; 4
    php                     ; 3
    plp                     ; 4
    sec                     ; 2 = 16    needed for 1st ror (25th bit)
    lda     grp0RLst,x      ; 4
    dey                     ; 2
    sta.w   GRP0            ; 4 = 10    @48
    bne     .loopBlock      ; 2/3
    ror     .tmpFirst2      ; 5         shift bits into .tmpFirst
    ror     .tmpFirst1      ; 5
    ror     .tmpFirst       ; 5 = 15
    dex                     ; 2
    bpl     .loopQrKernel   ; 3/2=7/6   @70/69*
    sty     ENAM1
    sty     GRP1
;---------------------------------------
    sty     GRP0

    ldx     #{2}
.loopWaitBtm
    sta     WSYNC
;---------------------------------------
    dex
    bne     .loopWaitBtm
    ECHO    "    QR Code sprite kernel:", [. - _qrDrawCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrDrawCode
  ENDM ; /QR_DRAW_CODE

  MAC _QR_ARRANGE_DRAW_DATA
TIM_AS_S
TIM_AS_E
  ENDM

  MAC QR_DRAW_DATA
_qrFuncData ; for 25 pixel

    include QRFuncDataV2S0.inc ; special pattern

    ECHO    "    QR Code function modules data:", [. - _qrFuncData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrFuncData
  ENDM

