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

; QR code modes:
;QR_NUMERIC      = %0001     ; unsupported
QR_ALPHA        = %0010
QR_BYTE         = %0100
;QR_KANJI        = %0111     ; unsupported

; QR code error correction levels:
QR_LVL_L        = 0
QR_LVL_M        = 1
QR_LVL_Q        = 2
QR_LVL_H        = 3

  IFNCONST QR_MODE
QR_MODE         = QR_ALPHA
  ENDIF
  IFNCONST QR_VERSION
QR_VERSION      = 2         ; 1..3, QR code size (21, 25, 29)
  ENDIF
  IFNCONST QR_LEVEL
QR_LEVEL        = QR_LVL_M  ; 0..3, error correction levels L, M, Q, H
  ENDIF
  IFNCONST QR_SINGLE_MASK
QR_SINGLE_MASK  = 1         ; 0|1, use only 1 of the 8 mask pattern
  ENDIF
  IFNCONST QR_PADDING
QR_PADDING      = 1         ; 0|1, (+31 bytes) add padding bytes add the end of test message text
  ENDIF
  IFNCONST QR_GENERATE
QR_GENERATE     = 0         ; 0|1, generate Reed-Solomon ECC generator polynomial on-the-fly
  ENDIF
  IFNCONST QR_FLEX_URL
QR_FLEX_URL     = 0         ; 0|1, use url different from default "H.FIRMAPLUS.DE/Q"
  ENDIF


;===============================================================================
; C A L C U L A T E D   Q R   C O D E   C O N S T A N T S
;===============================================================================

; Do NOT change the following constants!

  IFNCONST QR_OVERLAP
QR_OVERLAP      = 1 ; overlaps input and output data to save RAM (defined for version 2 only!)
; Note: 0 has not enough ZP-RAM
  ENDIF
  IFNCONST QR_DIRECT_DRAW
QR_DIRECT_DRAW  = 1 ; (-66 bytes ROM, -9 bytes RAM) draw byte columns instead of individual pixel
; TODO: 0 not working
 ENDIF

  IF QR_VERSION = 2 ; 25x25 QR code
QR_DEGREE   = 10 + QR_LEVEL * 6
  ELSE
    ECHO    ""
    ECHO    "ERROR: Unsupported QR code version", QR_VERSION, "!"
    ERR
  ENDIF ; /QR_VERSION = 2

QR_SIZE = 17 + QR_VERSION * 4

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

  IF QR_MODE = QR_BYTE
_QR_MAX_MSG = QR_MAX_DATA - 2
  ENDIF
  IF QR_MODE = QR_ALPHA
_QR_MAX_MSG = (QR_MAX_DATA * 8 - 13) * 2 / 11
  ENDIF
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
  IF QR_MODE = QR_BYTE ;{
QR_NON_OVER = 6
  ENDIF ;}
  IF QR_MODE = QR_ALPHA
QR_NON_OVER = 1
  ENDIF
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

  IF QR_GENERATE ;{
; Computes a Reed-Solomon ECC generator polynomial for degree 16, storing in result[0 : 16-1].
; This is implemented as a lookup table too (Generator)
;-----------------------------------------------------------
  MAC _RS_DIVISOR
;-----------------------------------------------------------
; E.g. g(x)=(x+1)(x+?)(x+?^2)(x+?^3)...(x+?^15)
;          = x^16+3Bx^15+0Dx^14+68x^13+BDx^12+44x^11+d1x^10+1e^x9+08x^8
;            +A3x^7+41x^6+29x^5+E5x^4+62x^3+32x^2+24x+3B
.root   = qrTmpVars+2
.i      = qrTmpVars+3       ; ZP-RAM!

; memset(qrGenerator, 1, 16);
    ldx     #QR_DEGREE-1
    stx     .i
    ldy     #0
.loopClear
    sty     qrGenerator,x
    dex
    bne     .loopClear
; qrGenerator[0] = 1;  // Start off with the monomial x^0
    iny
    sty     qrGenerator
; uint8_t root = 1;
    sty     .root
; for (int i = 0; i < degree; i++) {
.loopI
; // Multiply the current product by (x - r^i)
; for (int j = degree-1; j >= 0; j--) {
    ldx     #QR_DEGREE-1
.loopJ
; qrGenerator[j] = reedSolomonMultiply(qrGenerator[j], root);
    lda     qrGenerator,x
    ldy     .root
    jsr     RSMult
; if (j)
    dex
    bmi     .skipEor
; qrGenerator[j] ^= qrGenerator[j - 1];
    eor     qrGenerator,x
.skipEor
    sta     qrGenerator+1,x
    txa
    bpl     .loopJ
; root = reedSolomonMultiply(root, 0x02);
    lda     .root
    ldy     #$02
    jsr     RSMult
    sta     .root
    dec     .i              ; ZP-RAM!
    bpl     .loopI
  ENDM

;-----------------------------------------------------------
RSMult SUBROUTINE
    _RS_MULT
    rts
;-----------------------------------------------------------
  ENDIF ;} /QR_GENERATE

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
  IF QR_GENERATE ;{
    lda     qrGenerator,x
    jsr     RSMult
  ELSE ;}
    lda     QR_Generator,x
    _RS_MULT
  ENDIF
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

 IF QR_DIRECT_DRAW
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
 ELSE ;{
;-----------------------------------------------------------
  MAC _BLACK_FUNC
;-----------------------------------------------------------
; Blacks all function/alignment and timing pattern areas

; assume bitmap is clear, only draw the pattern
    ldy     #4+2            ; pattern + timing lines
.loopPattern
    lda     #$ff            ; fill pattern
    jsr     DrawPattern
    bpl     .loopPattern
  ENDM

;-----------------------------------------------------------
BlackFunc SUBROUTINE
;-----------------------------------------------------------
    _BLACK_FUNC
    rts

;-----------------------------------------------------------
  MAC _DRAW_FUNC
;-----------------------------------------------------------
; Draws all function/alignment and timing pattern over existing codewords

; blacken functions pattern first
    jsr     BlackFunc

; draw function pattern
    ldy     #4
.loopPattern
    lda     #$00            ; draw pattern
    jsr     DrawPattern
    bne     .loopPattern

; draw horizontal timing pattern
    ldy     #QR_SIZE-1 - 9
    ldx     #QR_SIZE-1 - 6
.loopTimingH
    jsr     QR_InvertPixel
    dey
    dey
    cpy     #9
    bcs     .loopTimingH

; draw vertical timing pattern
    ldy     #6
    ldx     #QR_SIZE-1 - 9
.loopTimingV
    jsr     QR_InvertPixel
    dex
    dex
    cpx     #9
    bcs     .loopTimingV
  ENDM ;

;---------------------------------------------------------------
DrawPattern SUBROUTINE
;---------------------------------------------------------------
; Y = index, A = fill
.index      = qrTmpVars
.width      = qrTmpVars+1   ; ZP-RAM!
.height     = qrTmpVars+2   ; ZP-RAM!
.tmpPat     = qrTmpVars+3   ; ZP-RAM!
.offset     = qrTmpVars+4
.fill       = qrTmpVars+5

    sty     .index
    sta     .fill
    ldx     PatternY,y
    lda     PatternHeight,y
    sta     .height
    lda     PatternOffset,y
    tay
.loopRow
    sty     .offset
    lda     PatternStart,y
    ora     .fill
    sta     .tmpPat
    ldy     .index
    lda     PatternWidth,y
    sta     .width
    lda     PatternX,y
    tay
; draw one row
.loopBit
    sec                     ; allows for >8 black pixel
    ror     .tmpPat         ; ZP-RAM!
    bcc     .clear
;.set
    jsr     QR_CheckPixel
    bne     .skipInvert
.invert
    jsr     QR_InvertPixel
.skipInvert
    iny                     ; x++
    dec     .width          ; ZP-RAM!
    bne     .loopBit
    dex                     ; y--
    ldy     .offset
    iny
    dec     .height         ; ZP-RAM!
    bne     .loopRow
.exit
    ldy     .index
    dey
    rts

.clear
    jsr     QR_CheckPixel
    bne     .invert
    beq     .skipInvert
 ENDIF ;} /QR_DIRECT_DRAW = 0

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
  IF QR_DIRECT_DRAW
.iByte  = qrTmpVars+3       ; ZP-RAM!
.iBit   = qrTmpVars+4       ; ZP-RAM!
.right1 = qrTmpVars+5
  ELSE ;{
; must not overlap with DrawPattern space
.iBit   = qrTmpVars+6
.iByte  = qrTmpVars+7       ; ZP-RAM! (TODO)
.right1 = qrTmpVars+8
  ENDIF ;}

; blacken the (right) function modules in the bitmap
  IF QR_OVERLAP
    _CLEAR_RIGHT        ; returns with X = -1
  ELSE ;{
   IF QR_DIRECT_DRAW
    _BLACK_FUNC         ; returns with X = 0
   ELSE
    jsr     BlackFunc
    ldx     #0
   ENDIF
  ENDIF ;}
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
  IF QR_OVERLAP
; overwrite shared data
    cpy     #8*2+1
    bne     .skipBlackMiddle
; blacken the middle function modules in the bitmap
    _CLEAR_MIDDLE
   IF !QR_DIRECT_DRAW ;{
    ldy     .right1     ; DrawPattern overwrites Y
   ENDIF ;}
.skipBlackMiddle
    cpy     #8+1
    bne     .skipBlackLeft
; blacken the left function modules in the bitmap
    _CLEAR_LEFT
.skipBlackLeft
  ENDIF ;} /QR_OVERLAP
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
;    bne     .skipPixel
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
  IF QR_OVERLAP
    jmp     .loopRight
  ELSE ;{
    bpl     .loopRight      ; unconditional!
  ENDIF ;}
; } // for right
.exitDraw
TIM_DC_E
  ENDM ; /_DRAW_CODEWORDS

;-----------------------------------------------------------
  MAC _APPLY_MASK
;-----------------------------------------------------------
TIM_AM_S
   IF QR_SINGLE_MASK
;; use only mask 0:
;.y          = qrTmpVars
;
;    ldx     #QR_SIZE - 1
;.loopY
;    stx     .y
;    ldy     #QR_SIZE - 1
;.loopX
;; 0: (x + y) % 2 == 0
;    tya
;    eor     .y
;    lsr
;    bcs     .skipInvert
;    jsr     QR_InvertPixel
;.skipInvert
;    dey
;    bpl     .loopX
;    dex
;    bpl     .loopY
   ELSE ;{
.xMod3      = qrTmpVars     ; ZP-RAM!
.yMod3      = qrTmpVars+1   ; ZP-RAM!
.xDiv3      = qrTmpVars+2   ; ZP-RAM!
.tmp        = qrTmpVars+3   ; ZP-RAM!
.y          = qrTmpVars+4
;.x          = qrTmpVars+5
    lda     #0
    sta     .yMod3
    ldx     #QR_SIZE - 1
.loopY
    stx     .y
    lda     #0
    sta     .xMod3
    sta     .xDiv3
    ldy     #QR_SIZE - 1
.loopX
;    sty     .x
    lda     qrPattern
    bne     .not0
; 0: (x + y) % 2 == 0
    tya
    eor     .y
    bpl     .checkMod2

.not0
    cmp     #4
    bne     .not4
; 4: (x / 3 + y / 2) % 2 == 0
    lda     .y
    lsr
    adc     .xDiv3
    bpl     .checkMod2

.not4
    bcs     .above4

    lsr
    bcs     .not2
; 2: x % 3 == 0
    lda     .xMod3
    beq     .invert
    bne     .skipInvert

.not2
    bne     .is3
; 1: y % 2 == 0
    txa
    bpl     .checkMod2

.is3
; 3: (x + y) % 3 == 0
    lda     .xMod3
;    sec
    sbc     .yMod3
    beq     .invert
    bne     .skipInvert

.above4
    cmp     #6
    beq     .is6
    bcs     .is7
.is6
    php
; 5: x * y % 2 + x * y % 3 == 0
; 6: (x * y % 2 + x * y % 3) % 2 == 0
    lda     .xMod3
    beq     .modEven56
    lda     .yMod3
    beq     .modEven56
    clc
    adc     .xMod3
    BIT_W
.modEven56
    lda     #0
    sta     .tmp
    tya
    lsr
    bcc     .even56
    txa
    lsr
    bcc     .even56
    inc     .tmp            ; ZP-RAM!
.even56
    plp
    lda     .tmp
    bcs     .checkMod2
    beq     .invert
    bne     .skipInvert

.is7
; 7: ((x + y) % 2 + x * y % 3) % 2 == 0
    tya
    eor     .y
    sta     .tmp
    lda     .xMod3
    beq     .modEven7
    lda     .yMod3
    beq     .modEven7
    clc
    adc     .xMod3
    adc     .tmp
    sta     .tmp
.modEven7
    lda     .tmp
.checkMod2
    lsr
    bcs     .skipInvert
.invert
    jsr     QR_InvertPixel
.skipInvert
; next X
    dec     .xMod3          ; ZP-RAM!
    bpl     .xMod3OK
    lda     #2
    sta     .xMod3
    inc     .xDiv3          ; ZP-RAM!
.xMod3OK
    dey
    bpl     .loopX
; next Y
    dec     .yMod3          ; ZP-RAM!
    bpl     .yMod3OK
    lda     #2
    sta     .yMod3
.yMod3OK
    dex
    bmi     .exitLoopY
    jmp     .loopY

.exitLoopY
   ENDIF ;} /!QR_SINGLE_MASK
TIM_AM_E
  ENDM ; /_APPLY_MASK

;-----------------------------------------------------------
  MAC _DRAW_FORMAT
;-----------------------------------------------------------
   IF !QR_SINGLE_MASK || !QR_DIRECT_DRAW ;{
.idx    = qrTmpVars

    ldy     #QR_FORMATS-1
.loopFormat
    sty     .idx
    cpy     #8
  IF QR_SINGLE_MASK
    ldx     #0
  ELSE
    ldx     qrPattern
  ENDIF
    lda     FormatLo,x
    and     QR_BitMask,y
    bcc     .lowFormat
    lda     FormatHi,x
    and     QR_BitMask-8,y
.lowFormat
   IF QR_DIRECT_DRAW
    beq     .skipFormat
   ELSE
    bne     .skipFormat
   ENDIF
    ldx     FormatY1,y
    lda     FormatX1,y
    tay
    jsr     QR_InvertPixel
    ldy     .idx
    ldx     FormatY2,y
    lda     FormatX2,y
    tay
    jsr     QR_InvertPixel
    ldy     .idx
.skipFormat
    dey
    bpl     .loopFormat
   ENDIF ;}
  ENDM

; ********** The user macros start here: **********

;-----------------------------------------------------------
  MAC _QR_START_MSG
;-----------------------------------------------------------
; A = message length
   IF QR_MODE = QR_BYTE ;{
; A = message length
; add mode and length to message data
    tax
    lsr
    lsr
    lsr
    lsr
    ora     #(QR_MODE << 4)
; (QR_MODE << 4) | (MSG_LEN >> 4)
    sta     qrMsgData + QR_MAX_DATA - 1
    txa
    asl
    asl
    asl
    asl
; (MSG_LEN << 4)
    sta     qrMsgData + QR_MAX_DATA - 2
    lda     #QR_MAX_DATA - 3
    sta     qrMsgIdx
   ENDIF ;}

   IF QR_MODE = QR_ALPHA ;{
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
   ENDIF ;}

;; clear the remaining data buffer
;    ldx     #QR_TOTAL-3
;    lda     #0
;.loopClear
;    sta     qrData,x
;    dex
;    bpl     .loopClear
  ENDM  ; /_QR_START_MSG

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
  IF (QR_MODE = QR_BYTE) | QR_FLEX_URL ;{
    bpl     _QrAddMsgDirect
  ENDIF ;}
; /QrAddMsg

QrAddMsgChar SUBROUTINE
;---------------------------------------------------------------
   IF QR_MODE = QR_BYTE ;{
; A = byte to add, uses X
_QrAddMsgDirect
    ldx     qrMsgIdx
    pha
    lsr
    lsr
    lsr
    lsr
    ora     qrMsgData + 1,x
    sta     qrMsgData + 1,x
    pla
    asl
    asl
    asl
    asl
    sta     qrMsgData,x
    dec     qrMsgIdx        ; ZP-RAM!
   ENDIF ;}

   IF QR_MODE = QR_ALPHA
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
  ENDIF ; / QR_MODE = QR_ALPHA

    ECHO    "    QR Code message code #2:", [. - _QrAddMsgCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _QrAddMsgCode

  ENDM  ; /QR_ADD_MSG_CODE

;{-----------------------------------------------------------
  MAC _QR_CONVERT_INPUT
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

  IF QR_MODE = QR_BYTE ;{
   IF QR_PADDING
.msgLen     = qrTmpVars
; pad with optional filler bytes (QR code works without too)
    lda     #_QR_MAX_MSG - 1
    sec
    sbc     .msgLen
    bcc     .donePadding
    tax
.loopPadding
    lda     #$ec            ; defined by QR standard
    sta     qrMsgData,x
    dex
    bmi     .donePadding
    lda     #$11            ; defined by QR standard
    sta     qrMsgData,x
    dex
    bpl     .loopPadding
   ENDIF
  ENDIF ;}

  IF QR_MODE = QR_ALPHA
DEBUG2
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
  ENDIF ; /QR_ALPHA
.donePadding
  ENDM ; /QR_STOP_MSG

;-----------------------------------------------------------
  MAC QR_GEN_CODE
;-----------------------------------------------------------
; This is the main macro to use!
_QRCodeCode

  IF QR_GENERATE ;{
; generate the Reed-Solomon ECC generator polynomial
RSDevisor
;    _RS_DIVISOR
  ENDIF ;}
; calculate the ECC
RSRemainder
    _RS_REMAINDER
; draw the code words onto the bitmap using the BlackGfx as pixel mask
DrawCodes
    _DRAW_CODEWORDS
; apply the pattern mask
ApplyMask
    _APPLY_MASK
; blacken the function modules in the bitmap again
; and draw the function modules in the bitmap
DrawFunc
    _DRAW_FUNC
; draw the format bits
DrawFormat
    _DRAW_FORMAT

    _QR_ARRANGE_DRAW_DATA    ; required only for PF display

    ECHO    "    QR Code encoding code:", [. - _QRCodeCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _QRCodeCode
  ENDM ; /QR_GEN_CODE

;-----------------------------------------------------------
  MAC QR_CODE_DATA
;-----------------------------------------------------------
; Add this to your code's data area
QRCodeData

   IF !QR_DIRECT_DRAW ;{
; Function pattern parameters
PatternX ; left
    .byte   6                       ; left, vertical timing line
    .byte   0, 0, QR_SIZE-8         ; eye pattern
    .byte   QR_SIZE-8, QR_SIZE-9    ; alignment pattern (right/middle part)
    .byte   9                       ; top, horizontal timing line
PatternY ; top
    .byte   QR_SIZE - 10            ; left, vertical timing line
    .byte   QR_SIZE-1, 7, QR_SIZE-1 ; eye pattern
    .byte   8, 8                    ; alignment pattern (right/middle part)
    .byte   QR_SIZE - 7             ; top, horizontal timing line
PatternOffset = . - 1
    .byte   EyeLeftTop-PatternStart     ; 1
    .byte   EyeLeftBottom-PatternStart  ; 2
    .byte   EyeRightTop-PatternStart    ; 3
    .byte   AlignRightBottom-PatternStart
    .byte   AlignRightBottom-PatternStart
; timing lines need no pattern, always drawn filled here
PatternWidth
    .byte   1                       ; left, vertical timing line
    .byte   9, 9, 8                 ; eye pattern
    .byte   4, 1                    ; alignment pattern (right/middle part)
;    .byte   QR_SIZE - 9 - 8         ; top, horizontal timing line
PatternHeight
    .byte   QR_SIZE - 9 - 8         ; left, vertical timing line
    .byte   9, 8, 9                 ; eye pattern
    .byte   5, 5                    ; alignment pattern (right/middle part)
    .byte   1                       ; top, horizontal timing line

PatternStart
EyeRightTop
    .byte   %11111110
    .byte   %10000010
    .byte   %10111010
    .byte   %10111010
    .byte   %10111010
    .byte   %10000010
    .byte   %11111110
    .byte   %00000000
    .byte   %11111111
AlignRightBottom = . - 1
;    .byte   %1111
    .byte   %1000
    .byte   %1010
    .byte   %1000
    .byte   %1111

EyeLeftBottom
    .byte   %00000000
;    .byte   %01111111
;    .byte   %01000001
;    .byte   %01011101
;    .byte   %01011101
;    .byte   %01011101
;    .byte   %01000001
;    .byte   %01111111
EyeLeftTop
    .byte   %01111111
    .byte   %01000001
    .byte   %01011101
    .byte   %01011101
    .byte   %01011101
    .byte   %01000001
    .byte   %01111111
    .byte   %00000000
    .byte   %11111111
   ENDIF ;} /!QR_DIRECT_DRAW

; Format Information Strings
  IF !QR_SINGLE_MASK || !QR_DIRECT_DRAW ;{
   IF QR_LEVEL = 0 ; L
FormatLo
    .byte   %11101111
    .byte   %11100101
    .byte   %11111011
    .byte   %11110001
    .byte   %11001100
    .byte   %11000110
    .byte   %11011000
    .byte   %11010010
FormatHi
    .byte   %10001000
    .byte   %11100110
    .byte   %01010100
    .byte   %00111010
    .byte   %01011110
    .byte   %00110000
    .byte   %10000010
    .byte   %11101100
   ENDIF
   IF QR_LEVEL = 1 ; M
FormatLo
    .byte   %10101000
    .byte   %10100010
    .byte   %10111100
    .byte   %10110110
    .byte   %10001011
    .byte   %10000001
    .byte   %10011111
    .byte   %10010101
FormatHi
    .byte   %00100100
    .byte   %01001010
    .byte   %11111000
    .byte   %10010110
    .byte   %11110010
    .byte   %10011100
    .byte   %00101110
    .byte   %01000000
   ENDIF
   IF QR_LEVEL = 2 ; Q
FormatLo
    .byte   %01101010
    .byte   %01100000
    .byte   %01111110
    .byte   %01110100
    .byte   %01001001
    .byte   %01000011
    .byte   %01011101
    .byte   %01010111
FormatHi
    .byte   %10111110
    .byte   %11010000
    .byte   %01100010
    .byte   %00001100
    .byte   %01101000
    .byte   %00000110
    .byte   %10110100
    .byte   %11011010
   ENDIF
   IF QR_LEVEL = 3 ; H
FormatLo
    .byte   %00101101
    .byte   %00100111
    .byte   %00111001
    .byte   %00110011
    .byte   %00001110
    .byte   %00000100
    .byte   %00011010
    .byte   %00010000
FormatHi
    .byte   %00010010
    .byte   %01111100
    .byte   %11001110
    .byte   %10100000
    .byte   %11000100
    .byte   %10101010
    .byte   %00011000
    .byte   %01110110
   ENDIF

; positions of the 15 type information bits
FormatX1
    .byte   0, 1, 2, 3, 4, 5, 7, 8
;    ds      7, 8           ; shared
FormatX2
    ds      7, 8
    .byte   QR_SIZE-8, QR_SIZE-7, QR_SIZE-6, QR_SIZE-5
    .byte   QR_SIZE-4, QR_SIZE-3, QR_SIZE-2, QR_SIZE-1
FormatY2
    .byte   0, 1, 2, 3, 4, 5, 6
;    ds      8, QR_SIZE-9   ; shared
FormatY1
    ds      8, QR_SIZE-9
    .byte   QR_SIZE-8, QR_SIZE-6, QR_SIZE-5, QR_SIZE-4
    .byte   QR_SIZE-3, QR_SIZE-2, QR_SIZE-1
  ENDIF ;} /QR_SINGLE_MASK = 0 || QR_DIRECT_DRAW = 0

QR_BitMask
    .byte   $80, $40, $20, $10, $8, $4, $2, $1

 IF !QR_GENERATE
QR_Generator ; data in reversed order!
  IF QR_DEGREE = 7
    .byte   $75, $44, $0b, $a4, $9a, $7a, $7f
  ENDIF
  IF QR_DEGREE = 10
    .byte   $c1, $9d, $71, $5f, $5e, $c7, $6f, $9f
    .byte   $c2, $d8
  ENDIF
  IF QR_DEGREE = 13
    .byte   $87, $84, $53, $2b, $2e, $0d, $34, $11
    .byte   $b1, $11, $e3, $49, $89
  ENDIF
  IF QR_DEGREE = 15
    .byte   $1a, $86, $20, $97, $84, $8b, $69, $69
    .byte   $0a, $4a, $70, $a3, $6f, $c4, $1d
  ENDIF
  IF QR_DEGREE = 16
; Reed-Solomon ECC generator polynomial for degree 16
; g(x)=(x+1)(x+?)(x+?^2)(x+?^3)...(x+?^15)
; = x^16+3bx^15+0dx^14+68x^13+bdx^12+44x^11+d1x^10+1e^x9+08x^8
;   +a3x^7+41x^6+29x^5+e5x^4+62x^3+32x^2+24x+3b
    .byte   $3b, $24, $32, $62, $e5, $29, $41, $a3
    .byte   $08, $1e, $d1, $44, $bd, $68, $0d, $3b
  ENDIF
  IF QR_DEGREE = 17
    .byte   $4f, $63, $7d, $35, $55, $86, $8f, $29
    .byte   $f9, $53, $c5, $16, $77, $78, $53, $42
    .byte   $77
  ENDIF
  IF QR_DEGREE = 22
    .byte   $f5, $91, $1a, $e6, $da, $56, $fd, $43
    .byte   $7b, $1d, $89, $1c, $28, $45, $bd, $13
    .byte   $f4, $b6, $b0, $83, $b3, $59
  ENDIF
  IF QR_DEGREE = 26
    .byte   $5e, $2b, $4d, $92, $90, $46, $44, $87
    .byte   $2a, $e9, $75, $d1, $28, $91, $18, $ce
    .byte   $38, $4d, $98, $c7, $62, $88, $04, $b7
    .byte   $44, $f6
  ENDIF
  IF QR_DEGREE = 28
    .byte   $c5, $3a, $4a, $b0, $93, $79, $64, $b5
    .byte   $7f, $e9, $77, $75, $38, $f7, $0c, $a7
    .byte   $29, $64, $ae, $67, $96, $d0, $fb, $12
    .byte   $0d, $1c, $09, $fc
  ENDIF

QR_DEGREE = . - QR_Generator  ; verify data size
 ENDIF ; /!QR_GENERATE

  IF !QR_FLEX_URL
QrMsgInit
    .byte   $fd, $95, $2c, $fa, $bc, $ed, $54, $b3
    .byte   $56, $27, $f3, $20
QR_MSG_INIT_LEN = . - QrMsgInit
  ENDIF

  IF (QR_MODE = QR_ALPHA) & QR_FLEX_URL ;{
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
; Platform and version specific code. Must NOT change X and Y registers!
; X = y; Y = x
  IF 0 ;{
; determine 8 bit column (0..2) or missile columns
    tya
    bne     .notMissile
; check if single missile byte is affected
    cpx     #8
    bcc     .alwaysSet
    cpx     #8*2
    bcs     .alwaysSet
;    lda     firstMsl
;    and     QR_BitMask-8,x
    lda     #0
    rts

.alwaysSet
    lda     #1
    rts

.notMissile
    cpy     #1+8
    bcs     .notGRP0L
; specific code for data overlapping & direct drawing:
    cpx     #8              ; bottom left eye (partially) shared with data!
    bcc     .alwaysSet
;---------------------------------------
    lda     grp0LLst,x
    and     QR_BitMask-1,y
    rts

.notGRP0L
    cpy     #1+8*2
    bcs     .notGRP1
    lda     grp1Lst,x
    and     QR_BitMask-1-8,y
    rts

.notGRP1
; must be GRP0R then
    lda     grp0RLst,x
    and     QR_BitMask-1-8*2,y
  ELSE ;}
    tya
    bne     .notMissile
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
  ENDIF ;}
    rts

;---------------------------------------------------------------
QR_InvertPixel SUBROUTINE
;---------------------------------------------------------------
; Platform and version specific code. Must NOT change X and Y registers!
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
; $fd, $95, $2c, $fa, $bc, $ed, $54, $b3, $56, $27, $f3, $20
;qrInputIdx  = $8a = $10
;qrMsgIdx    = $8b = $0f
;qrNewByte   = $8c = $29
;qrTmp       = $90
;qrCrc8      = $94 = $00
;qrUrlPos    = $95 = $0f