  MAC QR_DRAW_CODE
QR_BLOCK_H  = 7
.tmpFirst   = qrTmpVars     ; ZP-RAM!

; |PF0 |  PF1   |  PF2   |  PF2   |  PF1   |PF0 |
; |....|...xxxxx|xxxxxxxx|xxxxxxxx|xxxx....|....|
.pf0R1LLst  = grp0LLst
.pf2LLst    = grp1Lst
.pf1RLst    = grp0RLst

    lda     #0
    sta     CTRLPF
    sta     COLUPF
    lda     #$0e
    sta     COLUBK

; some vertical centering
    ldx     #{1}    ;(200-QR_SIZE*QR_BLOCK_H)/2
.waitTop
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitTop

    ldx     #QR_SIZE-1
    lda     #%1             ;           1st top left fixed pixel
    bne     .enterLoop

.loopBlock
    SLEEP   2
    lda     #0              ; 2
    sta     PF2             ; 3         @43/44
    sta     PF0             ; 3 =  8    @46/47
    beq     .contBlock      ; 3

; the QR code kernel
LoopKernel                  ;           @47/48
    lda     QrFirstIdxTbl,x ; 4
    sty     PF0             ; 3
    sty     PF2             ; 3
    cmp     #1              ; 2
    bcs     .newFirst       ; 2/3
    lsr     .tmpFirst       ; 5         ZP-RAM!
    bpl     .endFirst       ; 3 = 22    unconditional

.newFirst                   ;           @62/63
; $bf/$3f | $01 | $fe
    bne     .enterLoop      ; 2/3
    lda     firstMsl        ; 3
.enterLoop
    sta     .tmpFirst       ; 3 =  8
.endFirst                   ;           @69..71
    ldy     #QR_BLOCK_H     ; 2
.contBlock
    sta     WSYNC           ; 3
;---------------------------------------
; |PF0 |  PF1   |  PF2   |PF0 |  PF1   |  PF2   |
; |    |7......0|0......7|4..7|7......0|        |
; |....|...XXXXX|XXXXXXXX|XXXX|XXXXXXXX|........|
    lda     .tmpFirst       ; 3
    lsr                     ; 2
    lda     .pf0R1LLst,x    ; 4
    and     #%1111          ; 2
    bcc     .clear          ; 2/3
    ora     #%10000         ; 2
.clear                      ;   = 14/15
    sta     PF1             ; 3         @17/18
    lda     .pf2LLst,x      ; 4
    sta     PF2             ; 3 = 10    @24/25
    lda     .pf0R1LLst,x    ; 4
    sta     PF0             ; 3         @31/32  >=27
    lda     .pf1RLst,x      ; 4
    sta     PF1             ; 3 = 14    @38/39  >=38
    dey                     ; 2
    bne     .loopBlock      ; 3/2
    dex                     ; 2
    bpl     LoopKernel      ; 3/2=9/8   @47/48
    CHECKPAGE LoopKernel
    SLEEP   2               ; 2
    sty     PF2             ; 3         @51/52
    sty     PF0             ; 3         @54/55
    sta     WSYNC           ; 3
;---------------------------------------
    sty     PF1

    ldx     #{2}
.waitBtm
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitBtm
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
QrFirstIdxTbl ; for 25 pixel
    ds 7, 0
    .byte   $fe
    ds 7, 0
    .byte   $01
    ds 7, 0
   IF QR_LEVEL = 0 || QR_LEVEL = 1
    .byte   $bf     ; 1st format bit is 1
   ELSE
    .byte   $3f     ; 1st format bit is 0
   ENDIF
    CHECKPAGE QrFirstIdxTbl ; make sure this table does not cross a page!

    include QRFuncDataV2S.inc ; special pattern

    ECHO    "QR Code function modules data:", [. - QrFirstIdxTbl]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - QrFirstIdxTbl
  ENDM

