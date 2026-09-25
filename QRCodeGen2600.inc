; *** HSC QR code generator ***
; (C)2021/2026 Thomas Jentzsch
; specialized for creating URLs for the PlusCart HSC

; TODOs:
; x optimize SpecialTbl (only last two entries required for URL)
; - support non-ZP RAM
;   o define exceptions which still need ZP-RAM for RMW operations
;   - define and apply read and write offsets
; + put fixed mask into EorGfx
; + eliminate BlackGfx
; - allow SaveKey with QR-code


;===============================================================================
; U S E R - D E F I N E D   Q R   C O D E   C O N S T A N T S
;===============================================================================

; QR code error correction levels:
QR_LVL_L        = 0
QR_LVL_M        = 1
QR_LVL_Q        = 2         ; unsupported
QR_LVL_H        = 3         ; unsupported

  IFNCONST QR_LEVEL
QR_LEVEL        = QR_LVL_M  ; 0..3, error correction levels L, M, Q, H
  ENDIF
  IFNCONST QR_PADDING
QR_PADDING      = 1         ; 0|1, (+31 bytes) add padding bytes add the end of test message text
  ENDIF
  IFNCONST QR_FLEX_URL
QR_FLEX_URL     = 0         ; 0|1, use url different from default "H.FIRMAPLUS.DE/Q"
  ENDIF


;===============================================================================
; C A L C U L A T E D   Q R   C O D E   C O N S T A N T S
;===============================================================================

; QR code mode:
QR_ALPHA        = %0010

; Do NOT change the following constants!
QR_VERSION      = 2         ; 2, QR code size (25)
QR_MODE         = QR_ALPHA

QR_DEGREE       = 10 + QR_LEVEL * 6     ; 10 or 16
QR_SIZE         = 17 + QR_VERSION * 4

; Calculate capacity based on version
_QR_VAL SET (QR_VERSION * 16 + 128) * QR_VERSION + 64
  IF QR_VERSION >= 2
_QR_NUM_ALIGN = QR_VERSION / 7 + 2
_QR_VAL SET _QR_VAL - ((25 * _QR_NUM_ALIGN - 10) * _QR_NUM_ALIGN - 55)
  ENDIF
QR_CAPACITY_BITS = _QR_VAL / 8 * 8
;QR_CAPACITY = _QR_VAL / 8
QR_TERM     = %0000; terminator

QR_MAX_DATA = QR_CAPACITY_BITS / 8 - QR_DEGREE

QR_POLY     = $11d  ; GF(2^8) is based on 9 bit polynomial
                    ; x^8 + x^4 + x^3 + x^2 + 1 = 0x11d
QR_FORMATS  = 15    ; 15 type information bits

_QR_MAX_MSG = (QR_MAX_DATA * 8 - 13) * 2 / 11
QR_MAX_MSG  = (_QR_MAX_MSG - (QR_URL_LEN + 2)) / 2  ; URL + (CRC8 * 2)
QR_TOTAL    = QR_MAX_DATA + QR_DEGREE ; e.g. 44

NUM_FIRST   = 1     ; left top 9 and bottom 8 bits are fixed!

_QR_TOTAL   SET 0


;===============================================================================
; V A R I A B L E S
;===============================================================================

; These two variables define start and end of the RAM area which can be used by
; the QR code.
; Note: Currently only ZP-RAM supported (support for non-ZP RAM shouldn't be
; a major problem)

  IFNCONST qrRamStart
    ECHO    ""
    ECHO    "!!! ERROR: qrRamStart not defined !!!"
    ERR
  ENDIF
  IFNCONST qrRamEnd
    ECHO    ""
    ECHO    "!!! ERROR: qrRamEnd not defined !!!"
    ERR
  ENDIF


;===============================================================================
; Q R   Z P - V A R I A B L E S
;===============================================================================

    SEG.U   variables
    ORG     qrRamStart

;---------------------------------------
; QR code variables
; all byte counts based on version 2, level M QR code
  IF qrRamStart < $100
qrTmpVars   ds 6
  ELSE
qrTmpVars   = qrRamStartZp          ; handle SC-RAM (TODO) (4 bytes needed)
  ENDIF
;---------------------------------------
; input data (remainder and message):
qrData      ds QR_TOTAL             ; 48 bytes
;- - - - - - - - - - - - - - - - - - - -
; The QR draw data overlaps the QR code data! It overwrites the QR code data while being drawn.
QR_NON_OVER = 1
; generated QR code data, used for drawing:
qrCodeLst   = qrData + QR_NON_OVER  ; all but 6/1 bytes overlap (version 2 only!)
            ds NUM_FIRST + QR_SIZE*3 - QR_TOTAL + QR_NON_OVER   ; 28/34 bytes
CODE_LST_SIZE   = . - qrCodeLst
_QR_RAM         = . - qrRamStart

  IF . > qrRamEnd
    ECHO    ""
    ECHO    "!!! ERROR: QR code RAM data overwrites game RAM data! (", qrRamEnd, ">", ., ") !!!"
    ECHO    "   ", [. - qrRamStart]d, "bytes ZP RAM required!"
    ERR
  ENDIF

; drawing variables (named for sprite display):
grp0LLst    = qrCodeLst + QR_SIZE * 0
firstMsl    = qrCodeLst + QR_SIZE * 1
grp1Lst     = qrCodeLst + NUM_FIRST + QR_SIZE * 1
grp0RLst    = qrCodeLst + NUM_FIRST + QR_SIZE * 2

qrRemainder = qrData                ; (QR_DEGREE = e.g. 16 bytes)
qrMsgData   = qrData + QR_DEGREE    ; (QR_MAX_DATA = e.g. 28 bytes)

qrInputIdx  = qrTmpVars             ; ZP-RAM!
qrMsgIdx    = qrTmpVars + 1         ; ZP-RAM!
qrNewByte   = qrTmpVars + 2         ; ZP-RAM!
qrTmp       = qrRemainder           ; 6 bytes
qrCrc8      = qrTmp+4
qrUrlPos    = qrTmp+5


;===============================================================================
; Q R   C O D E   M A C R O S
;===============================================================================

  MAC BIT_B
    .byte   $24
  ENDM

; The following code has been partially converted from the C code of the
; QR code generator found at https://github.com/nayuki/QR-Code-generator

;-----------------------------------------------------------
; Returns the product of the two given field elements modulo GF(2^8/0x11D).
; All inputs are valid.
  MAC _RS_MULT
;-----------------------------------------------------------
; Russian peasant multiplication (.factor * b)
; Input: .factor, A = b
; Result: A
.b      = qrTmpVars             ; ZP-RAM!
.factor = qrTmpVars+1

    sta     .b
; uint8_t z = 0;
    lda     #0
; for (int i = 7; i >= 0; i--) {
    ldy     #7
.loopI
;   z = (uint8_t)((z << 1) ^ ((z >> 7) * 0x11D));
    asl
    bcc     .skipEorPoly
    eor     #<QR_POLY
.skipEorPoly
;   z ^= ((b >> i) & 1) * .factor;
    asl     .b                  ; ZP-RAM!
    bcc     .skipEorA
    eor     .factor
.skipEorA
; }
    dey
    bpl     .loopI
  ENDM

;-----------------------------------------------------------
  MAC _RS_REMAINDER
;-----------------------------------------------------------
TIM_RM_S
.factor = qrTmpVars+1
.i      = qrTmpVars+2

; memset(result, 0, 16); // (was done in QR_START_MSG)
    lda     #0
    ldx     #QR_DEGREE-1
.clearRemainder
    sta     qrRemainder,x
    dex
    bpl     .clearRemainder

; for (int i = dataLen-1; i >= 0; i--) {  // Polynomial division
    ldx     #QR_MAX_DATA-1
.loopI
    stx     .i
;   uint8_t factor = qrMsgData[i] ^ qrRemainder[degree - 1];
    lda     qrMsgData,x
    eor     qrRemainder + QR_DEGREE - 1
    sta     .factor
;   memmove(&qrRemainder[1], &qrRemainder[0], (size_t)(16 - 1) * sizeof(qrRemainder[0]));
    ldx     #QR_DEGREE-1
.loopMove
    lda     qrRemainder-1,x
    sta     qrRemainder,x
    dex
    bne     .loopMove
;   qrRemainder[0] = 0;
    stx     qrRemainder
;   for (int j = 16-1; j >= 0; j--)
    ldx     #QR_DEGREE-1
.loopJ
;     qrRemainder[j] ^= reedSolomonMultiply(generator[j], factor);
    lda     QR_Generator,x
    _RS_MULT
    eor     qrRemainder,x
    sta     qrRemainder,x
;   }
    dex
    bpl     .loopJ
; }
    ldx     .i
    dex
    bpl     .loopI
TIM_RM_E
  ENDM ; /_RS_REMAINDER

;-----------------------------------------------------------
  MAC _DRAW_FUNC
;-----------------------------------------------------------
TIM_DF_S
; Draws all function, alignment, timing and mask pattern over existing codewords
    ldx     #CODE_LST_SIZE-1
.loopEor
    lda     qrCodeLst,x
    cpx     #8
    bcs     .skipOra
    lda     #$00            ; clear top, left "eye" (used for overlapping)
.skipOra
    eor     FuncGfx,x       ; apply function, alignment, timing and mask pattern
    sta     qrCodeLst,x
    dex
    bpl     .loopEor
TIM_DF_E
  ENDM ; /_DRAW_FUNC

;-----------------------------------------------------------
; Draws the raw codewords (including data and ECC) onto the given QR Code. This requires the initial state of
; the QR Code to be black at function modules and white at codeword modules (including unused remainder bits).
  MAC _DRAW_CODEWORDS
;-----------------------------------------------------------
TIM_DC_S
; Note: This part has the maximum RAM usage
.vert   = qrTmpVars+0
.j      = qrTmpVars+1
.y      = qrTmpVars+2
.iByte  = qrTmpVars+3       ; ZP-RAM!
.iBit   = qrTmpVars+4       ; ZP-RAM!
.right1 = qrTmpVars+5

; blacken the (right) function modules in the bitmap
    _CLEAR_RIGHT        ; returns with X = -1
; int i = 0;  // Bit index into the data
; 2600 code has data in reversed order
    stx     .iBit           ; X = $ff
    lda     #QR_TOTAL-1
    sta     .iByte
; // Do the funny zigzag scan
; Note: 2600 code has .right1 increased by 1
; for (int right = qrsize - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
    ldy     #QR_SIZE-1+1
.loopRight
;  if (right == 6)
    cpy     #6+1
    bne     .not6
;    right = 5;
    dey                 ; skip the timing column
.not6
    sty     .right1
; overwrite shared data
    cpy     #8*2+1
    bne     .skipBlackMiddle
; blacken the middle function modules in the bitmap
    _CLEAR_MIDDLE
.skipBlackMiddle
    cpy     #8+1
    bne     .skipBlackLeft
; blacken the left function modules in the bitmap
    _CLEAR_LEFT
.skipBlackLeft
;   for (int vert = 0; vert < qrsize; vert++) {  // Vertical counter
    ldy     #QR_SIZE-1
.loopVert
    sty     .vert
;       bool upward = ((right + 1) & 2) != 0; // 2600 code works in reverse
    lda     .right1
    and     #$02
    bne     .notUp
;       int y = upward ? qrsize - 1 - vert : vert;  // Actual y coordinate
    lda     #QR_SIZE-1;+1
    sec
    sbc     .vert
    tay
.notUp
    sty     .y
;     for (int j = 0; j < 2; j++) {
; some tricky code with .j here
    ldy     .right1
    BIT_B
.loopJ
    dey
    sty     .j
;       int x = right - j;  // Actual x coordinate
    dey
;       if (!getModule(qrcode, x, y) && i < dataLen * 8) {
;    ldy     .x
    ldx     .y
    jsr     QR_CheckPixel
    bcs     .skipPixel
;         bool black = getBit(qrData[i >> 3], 7 - (i & 7));
    ldx     .iByte
    asl     qrData,x
    bcc     .skipInv
;         setModule(qrcode, x, y, black);
;    ldy     .x
    ldx     .y
    jsr     QR_InvertPixel
.skipInv
;         i++;
    lsr     .iBit
    bne     .skipByte
    dec     .iBit
    dec     .iByte          ; ZP-RAM!
    bmi     .exitDraw       ; 2600 code exits here!
.skipByte
;       }
.skipPixel
    ldy     .j
    cpy     .right1
    beq     .loopJ
;     } // for j
    ldy     .vert
    dey
    bpl     .loopVert
;   } // for vert
    ldy     .right1
    dey
    dey
    bpl     .loopRight      ; unconditional!
; } // for right

.exitDraw
TIM_DC_E
  ENDM ; /_DRAW_CODEWORDS

; ********** The user macros start here: **********

 ;{
;-----------------------------------------------------------
  MAC _QR_START_MSG
;-----------------------------------------------------------
; A = message length
    pha
    ldy     #0
    sty     qrCrc8          ; reset CRC8
    sty     qrInputIdx
    iny                     ; byte full marker
    sty     qrNewByte
    lda     #QR_MAX_DATA - 1
    sta     qrMsgIdx
    lda     #(QR_MODE << 4)
    jsr     Add4QrBits      ; 4 bits
; add 9 length bits:
    lda     #0
    ldy     #1
    jsr     _AddQrBits
    pla
    ldy     #8              ; 1+8 bits
    jsr     _AddQrBits
  ENDM  ;} /_QR_START_MSG

;---------------------------------------------------------------
  MAC QR_ADD_MSG_CODE
;---------------------------------------------------------------
_QrAddMsgCode
;-----------------------------------------------------------
QrStartMsg SUBROUTINE
;-----------------------------------------------------------

  IF QR_FLEX_URL ;{
    asl
    adc     #QR_URL_LEN + 2     ; URL + (CRC8 * 2)
    _QR_START_MSG
; add fixed URL part:
    ldy     #0
.loopUrl
    sty     qrUrlPos
    lda     QrUrlPrefix,y
    jsr     QrAddMsgChar
    ldy     qrUrlPos
    iny
    cpy     #QR_URL_LEN
    bcc     .loopUrl
  ELSE ;} !QR_FLEX_URL
    ldx     #QR_MSG_INIT_LEN
.loopInit
    lda     QrMsgInit-1,x
    sta     qrMsgData + QR_DEGREE-1,x
    dex
    bne     .loopInit
    stx     qrCrc8
    stx     qrInputIdx      ; only even or odd needed
    lda     #$0f
    sta     qrMsgIdx
    lda     #$29
    sta     qrNewByte

;qrInputIdx  = $8a = $10
;qrMsgIdx    = $8b = $0f
;qrNewByte   = $8c = $29
;qrTmp       = $90
;qrCrc8      = $94 = $00
;qrUrlPos    = $95 = $0f

  ENDIF ; /!QR_FLEX_URL
    rts

;---------------------------------------------------------------
QrAddCrc8 SUBROUTINE
;---------------------------------------------------------------
    lda     qrCrc8

    ; falls through

;---------------------------------------------------------------
QrAddMsg SUBROUTINE
;---------------------------------------------------------------
.hexVal     = qrTmp+3   ; saves 1 byte stack

    sta     .hexVal
;---------------------------------------
    tax                 ; remember value
    eor     qrCrc8      ; A contained the data
    sta     qrCrc8      ; XOR it with the byte
    asl                 ; current contents of A will become x^2 term
    bcc     .up1        ; if b7 = 1
    eor     #$07        ; then apply polynomial with feedback
.up1
    eor     qrCrc8      ; apply x^1
    asl                 ; C contains b7 ^ b6
    bcc     .up2
    eor     #$07
.up2
    eor     qrCrc8      ; apply unity term
    sta     qrCrc8      ; save result
    txa                 ; restore value
;---------------------------------------
    lsr
    lsr
    lsr
    lsr
    jsr     _QrAddMsgDirect
    lda     .hexVal
    and     #$0f
  IF  QR_FLEX_URL ;{
    bpl     _QrAddMsgDirect
  ENDIF ;}
; /QrAddMsg

QrAddMsgChar SUBROUTINE
;---------------------------------------------------------------
; must be inside a subroutine for QR_ALPHA!
    IF QR_FLEX_URL ;{
    _QR_CONVERT_INPUT
    ENDIF ;}
_QrAddMsgDirect
    tax
    lda     qrInputIdx
    inc     qrInputIdx      ; ZP-RAM!
    lsr                     ; 1st or 2nd byte?
    lda     qrTmp
    stx     qrTmp
; stored first byte will be handled
; A) by next ADD_MSG_BYTE or
; B) by STOP_MSG
    bcc     .doneFirstByte

; combine both bytes into 11 bits:
; multiply by 45 (%101101) (= 0..1980):
.prodLo     = qrTmp+1           ; TODO: SC-RAM (RMW!)
.factor2    = qrTmp+2

    ldx     #45-1
; A = multiplier, X = multiplicand
; Factor 1 is stored in the lower bits of .prodLo; the low byte of
; the product is stored in the upper bits.
    lsr                 ; prime the carry bit for the loop
    sta     .prodLo
    stx     .factor2
    lda     #0
    ldx     #8
.loopMult
; At the start of the loop, one bit of .prodLo has already been
; shifted out into the carry.
    bcc     .noAdd
;    clc
    adc     .factor2
.noAdd
    ror
    ror     .prodLo     ; pull another bit out for the next iteration
    dex                 ; inc/dec don't modify carry; only shifts and adds do
    bne     .loopMult
; A = high byte, .prodLo = low byte of product
; add converted 2nd byte:
    tax                     ; high byte
    lda     qrTmp           ; 2nd byte
;    clc
    adc     .prodLo
    sta     .prodLo
    txa                     ; high byte
    adc     #0
; store 11 bits:
    asl
    asl
    asl
    asl
    asl
    ldy     #3              ; 3 bits
    jsr     _AddQrBits
    lda     .prodLo         ; low byte
    ldy     #8              ; 8 bits
    jsr     _AddQrBits
.doneFirstByte
    rts
; /QrAddMsgChar

;---------------------------------------------------------------
Add4QrBits SUBROUTINE
;---------------------------------------------------------------
.tmpByte    = qrTmp

    ldy     #4
_AddQrBits
.loopBits
    asl
    rol     qrNewByte       ; ZP-RAM!
    bcc     .contByte
; byte full, store:
    sta     .tmpByte
    lda     qrNewByte
    ldx     qrMsgIdx
    sta     qrMsgData,x
    dec     qrMsgIdx        ; ZP-RAM!
    lda     #1              ; byte full marker
    sta     qrNewByte
    lda     .tmpByte
; loop:
.contByte
    dey
    bne     .loopBits
    rts
; /Add4QrBits

    ECHO    "    QR Code message code #2:", [. - _QrAddMsgCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _QrAddMsgCode

  ENDM  ; /QR_ADD_MSG_CODE

;-----------------------------------------------------------
  MAC _QR_CONVERT_INPUT ;{
;-----------------------------------------------------------
; A = DATA; uses X
; convert characters:
    cmp     #$3a
    bcs     .letterOrColon
    cmp     #$30
    bcc     .special
    sbc     #$30            ; 0..9
    bcs     .convDone
    DEBUG_BRK

.special
    sbc     #$20-1
;    bmi     .illegalChar
;    cmp     #SPECIAL_TBL_SIZE
;    bcs     .illegalChar
    tax
    lda     SpecialTbl,x
;    bmi     .illegalChar
    bne     .convDone
    DEBUG_BRK

.letterOrColon
    bne     .letter
    lda     #44             ; ":"
    bne     .convDone

.letter
    sbc     #$41-10         ; A..Z
;    bmi     .illegalChar
;    cmp     #26
;    bcs     .illegalChar
.convDone
  ENDM ;} /_QR_CONVERT_INPUT

;-----------------------------------------------------------
  MAC QR_STOP_MSG
;-----------------------------------------------------------
    jsr     QrAddCrc8       ; CRC8

    lda     qrInputIdx
    lsr
    bcc     .noSecondByte
    lda     qrTmp
    asl
    asl
    ldy     #6
    jsr     _AddQrBits
.noSecondByte
   IF !QR_PADDING
    lda     #0
    ldy     #8
    jsr     _AddQrBits       ; make sure last byte is written
   ELSE
; add terminator
    lda     #(QR_TERM << 4)
    ldy     #4
    jsr     Add4QrBits
; fill and store last byte:
    ldx     qrMsgIdx
    lda     qrNewByte
    cmp     #1              ; only byte full marker?
    beq     .emptyByte      ;  yes, byte empty
.loopBits
    asl
    bcc     .loopBits
    sta     qrMsgData,x
    dex
.emptyByte
    txa
    bmi     .donePadding
.loopPadding
    lda     #$ec
    sta     qrMsgData,x
    dex
    bmi     .donePadding
    lda     #$11
    sta     qrMsgData,x
    dex
    bpl     .loopPadding
   ENDIF ;/QR_PADDING
.donePadding
  ENDM ; /QR_STOP_MSG

;-----------------------------------------------------------
  MAC QR_GEN_CODE
;-----------------------------------------------------------
; This is the main macro to use!
_QRCodeCode

; calculate the ECC
RSRemainder
    _RS_REMAINDER
; draw the code words onto the bitmap
DrawCodes
    _DRAW_CODEWORDS
; draw the function modules and format bits in the bitmap
; also apply the pattern mask
DrawFunc
    _DRAW_FUNC

    _QR_ARRANGE_DRAW_DATA    ; required only for PF display

    ECHO    "    QR Code encoding code:", [. - _QRCodeCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _QRCodeCode
  ENDM ; /QR_GEN_CODE

;-----------------------------------------------------------
  MAC QR_CODE_DATA
;-----------------------------------------------------------
; Add this to your code's data area
QRCodeData

QR_BitMask
    .byte   $80, $40, $20, $10, $8, $4, $2, $1

QR_Generator ; data in reversed order!
  IF QR_DEGREE = 10
    .byte   $c1, $9d, $71, $5f, $5e, $c7, $6f, $9f
    .byte   $c2, $d8
  ENDIF
  IF QR_DEGREE = 16
; Reed-Solomon ECC generator polynomial for degree 16
; g(x)=(x+1)(x+?)(x+?^2)(x+?^3)...(x+?^15)
; = x^16+3bx^15+0dx^14+68x^13+bdx^12+44x^11+d1x^10+1e^x9+08x^8
;   +a3x^7+41x^6+29x^5+e5x^4+62x^3+32x^2+24x+3b
    .byte   $3b, $24, $32, $62, $e5, $29, $41, $a3
    .byte   $08, $1e, $d1, $44, $bd, $68, $0d, $3b
  ENDIF
QR_DEGREE = . - QR_Generator  ; verify data size

  IF !QR_FLEX_URL
QrMsgInit
    .byte   $fd, $95, $2c, $fa, $bc, $ed, $54, $b3
    .byte   $56, $27, $f3, $20
QR_MSG_INIT_LEN = . - QrMsgInit
  ENDIF

  IF QR_FLEX_URL ;{
SpecialTbl
    .byte   36          ; space
    ds      3, -1
    .byte   37          ; $
    .byte   38          ; %
    ds      4, -1
    .byte   39          ; *
    .byte   40          ; +
    ds      1, -1
    .byte   41          ; -
    .byte   42          ; .
    .byte   43          ; /
SPECIAL_TBL_SIZE = . - SpecialTbl
  ENDIF ;}

    ECHO    "    QR Code encoding data:", [. - QRCodeData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - QRCodeData
   IF QR_FLEX_URL ;{
QR_URL_PREFIX = "H.FIRMAPLUS.DE/Q" ; = 16 Chars

QrUrlPrefix
    .byte   QR_URL_PREFIX
QR_URL_LEN = . - QrUrlPrefix
   ELSE ;}
QR_URL_LEN = 16
   ENDIF

  ENDM ; /QR_CODE_DATA

; Atari 2600 specific macros
;-----------------------------------------------------------
  MAC _CLEAR_LEFT
;-----------------------------------------------------------
; Clears left sprite column (except for bottom "eye"); also clears firstMsl!
    ldx     #NUM_FIRST + QR_SIZE-1-8
    lda     #0
.loopClearLeft
    sta     qrCodeLst+8,x
;    sta     grp0LLst+8,x
    dex
    bpl     .loopClearLeft
  ENDM

;-----------------------------------------------------------
  MAC _CLEAR_MIDDLE
;-----------------------------------------------------------
; Clears middle sprite column
    ldx     #QR_SIZE-1
    lda     #0
.loopClearMiddle
    sta     grp1Lst,x
    dex
    bpl     .loopClearMiddle
  ENDM

;-----------------------------------------------------------
  MAC _CLEAR_RIGHT
;-----------------------------------------------------------
; Clears right sprite column
    ldx     #QR_SIZE-1
    lda     #0
.loopClearRight
    sta     grp0RLst,x
    dex
    bpl     .loopClearRight
  ENDM

  MAC QR_BITMAP_CODE
_QR_BitMapCode
;---------------------------------------------------------------
QR_CheckPixel SUBROUTINE
;---------------------------------------------------------------
; Must NOT change X and Y registers!
; X = y; Y = x
; determine 8 bit column (0..2) or missile columns
    tya
    bne     .notMissile
; check if single missile byte is affected
    cpx     #8
    bcc     .full
    cpx     #8*2
    rts

.notMissile
; check for timing pattern:
;    cpy     #6              ; X = 6?  (vertical timing) already checked in loop
;    beq     .full
    cpx     #QR_SIZE-1-6    ; Y = 18? (horizontal timing)
    beq     .full
; check for top finders pattern:
    cpx     #QR_SIZE-1-8    ; Y < 16?
    bcc     .notTopFinders
    cpy     #9              ; X < 9? (left top finder)
    bcc     .full
    cpy     #QR_SIZE-1-7    ; X >= 17? (right top finder)
    rts

.notTopFinders
; check for bottom finder pattern:
    cpx     #8              ; Y >= 8?
    bcs     .notBtmFinder
    cpy     #9              ; X < 9? (bottom finder)
    bcc     .full
.notBtmFinder
; check for alignment pattern:
    cpy     #16             ; X >= 16?
    bcc     .empty
    cpy     #21             ; X < 21?
    bcs     .empty
    cpx     #9              ; Y < 9?
    bcs     .empty
    cpx     #4              ; Y >= 4?
    rts

.full
    sec
    rts

.empty
    clc
    rts

;---------------------------------------------------------------
QR_InvertPixel SUBROUTINE
;---------------------------------------------------------------
; Must NOT change X and Y registers!
; X = y; Y = x
; determine 8 bit column (0..2) or missile column
    tya
    bne     .notMissile
; check if single missile byte is affected
    cpx     #8
    bcc     .ignore
    cpx     #8*2
    bcs     .ignore
    lda     QR_BitMask-8,x
    eor     firstMsl
    sta     firstMsl
.ignore
    rts

.notMissile
    cpy     #1+8
    bcs     .notGRP0L
    lda     grp0LLst,x
    eor     QR_BitMask-1,y
    sta     grp0LLst,x
    rts

.notGRP0L
    cpy     #1+8*2
    bcs     .notGRP1
    lda     grp1Lst,x
    eor     QR_BitMask-1-8,y
    sta     grp1Lst,x
    rts

.notGRP1
; must be GRP0R then
    lda     grp0RLst,x
    eor     QR_BitMask-1-8*2,y
    sta     grp0RLst,x
    rts

    ECHO    "    QR Code bitmap code:", [. - _QR_BitMapCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _QR_BitMapCode
  ENDM ; /QR_BITMAP_CODE

  IF QR_SPRITE_GFX
    include "QRSpriteDrawV2.inc"
  ELSE
    include "QRPfDrawV2.inc"
  ENDIF
