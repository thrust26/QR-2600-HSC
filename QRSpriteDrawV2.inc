
  MAC QR_DRAW_CODE
; Display: M1, P0a, P1, P0b (25 pixel)
_qrDrawCode
QR_BLOCK_H  = 2     ; QR code pixel height
.tmpFirst   = qrTmpVars     ; left pixel column (-> M1), ZP-RAM!
.tmpFirst1  = qrTmpVars+1   ; ZP-RAM!
.tmpFirst2  = qrTmpVars+2   ; ZP-RAM!


; reset some major TIA registers if required:
;    lda     #0
;    sta     NUSIZ1
;    sta     VDELP0

; Note: other color combinations work too, as long as the contrast is high enough
    ldx     #$00        ; black QR code
    sta     WSYNC
;---------------------------------------
    lda     #$0e        ; on white background
    sta     COLUBK
    stx     COLUP0
    stx     COLUP1
    lda     #%001
    sta     NUSIZ0

    ldx     #$1f
    stx     HMP0
    inx
    stx     HMP1
    lda     #$80
    sta     HMM1

    php
    plp

    sta     RESM1
    sta     RESP0
    sta     RESP1

    sta     WSYNC
;---------------------------------------
    sta     HMOVE

    ldx     #{1}
.loopWaitTop
    dex
    sta     WSYNC
;---------------------------------------
    bne     .loopWaitTop

    lda     #$7f
    sta     .tmpFirst
    lda     firstMsl        ; 3
   IF QR_LEVEL = 0 || QR_LEVEL = 1
    sec             ; top eye, 1st format bit is 1
    rol
   ELSE
    asl             ; top eye, 1st format bit is 0
   ENDIF
    sta     .tmpFirst1
    lda     #$fe
    rol
    sta     .tmpFirst2

    ldx     #QR_SIZE
    bne     .enterLoop      ; 3         1st top left fixed pixel

; the QR code kernel
LoopQRKernel                ;           @55
    ror     .tmpFirst2      ; 5
    ror     .tmpFirst1      ; 5
    ror     .tmpFirst       ; 5 = 15
.enterLoop
    ldy     #QR_BLOCK_H     ; 2 = 2
.loopBlock
    sta     WSYNC           ; 3 = 3     @75
;---------------------------------------
;M1-P0-P1-P0
    lda     .tmpFirst       ; 3
    asl                     ; 2
    sta     ENAM1           ; 3 =  8
    lda     grp1Lst-1,x     ; 4
    sta     GRP1            ; 3
    lda     grp0LLst-1,x    ; 4
    sta     GRP0            ; 3 = 14
    php                     ; 3
    plp                     ; 4
    php                     ; 3
    plp                     ; 4
    sec                     ; 2 = 16    needed for 1st ror
    lda     grp0RLst-1,x    ; 4
    dey                     ; 2
    sta.w   GRP0            ; 4 = 10    @48
    bne     .loopBlock      ; 2/3
    dex                     ; 2
    bne     LoopQRKernel    ; 3/2=7/6
    CHECKPAGE LoopQRKernel
    sta     WSYNC
;---------------------------------------------------------------
    stx     ENAM1
    stx     GRP1
    stx     GRP0

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
QrFuncData ; for 25 pixel

    include QRFuncDataV2S0.inc ; special pattern

    ECHO    "    QR Code function modules data:", [. -QrFuncData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - QrFuncData
  ENDM

