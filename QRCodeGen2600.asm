; *** HSC QR code generator ***
; (C)2021/2026 Thomas Jentzsch
; specialized for creating URLs for the PlusROM HSC

; TODOs:
; - support non-ZP RAM
;   o define exceptions which still need ZP-RAM for RMW operations
;   - define and apply read and write offsets
;
; DONEs:
; + BUG: message length fixed to 6 in init code!!!
; + BUG: clear horizontal timing byte
; + BUG: data overlapping in first
; + BUG: level low not working
; + reduce RAM
;   + improve overlapping
;   + clear data on demand


;===============================================================================
; U S E R - D E F I N E D   Q R   C O D E   C O N S T A N T S
;===============================================================================

; QR code error correction levels:
QR_LVL_L        = 0
QR_LVL_M        = 1
;QR_LVL_Q        = 2         ; unsupported
;QR_LVL_H        = 3         ; unsupported

  IFNCONST QR_LEVEL
QR_LEVEL        = QR_LVL_M  ; error correction levels L, M (, Q, H)
  ENDIF
  IFNCONST QR_PADDING
QR_PADDING      = 0         ; 0|1, (+29 bytes) add padding bytes add the end of
                            ; message text (usually works without)
  ENDIF
  IFNCONST QR_ECHO_ON
QR_ECHO_ON      = 0         ; 1 = echo some debug output to console
  ENDIF


;===============================================================================
; C A L C U L A T E D   Q R   C O D E   C O N S T A N T S
;===============================================================================

; Do NOT change the following constants!
QR_VERSION      = 2         ; QR code size (25)
QR_MODE         = %0010     ; alphanumerich mode

QR_DEGREE       = 10 + QR_LEVEL * 6     ; 10 or 16
QR_SIZE         = 17 + QR_VERSION * 4   ; 21 or 25

; Calculate capacity based on version
_QR_VAL SET (QR_VERSION * 16 + 128) * QR_VERSION + 64
  IF QR_VERSION >= 2
_QR_NUM_ALIGN   = QR_VERSION / 7 + 2
_QR_VAL SET _QR_VAL - ((25 * _QR_NUM_ALIGN - 10) * _QR_NUM_ALIGN - 55)
  ENDIF
QR_CAPACITY_BITS = _QR_VAL / 8 * 8
QR_TERM         = %0000; terminator

QR_MAX_DATA     = QR_CAPACITY_BITS / 8 - QR_DEGREE

QR_POLY         = $11d  ; GF(2^8) is based on 9 bit polynomial
                        ; x^8 + x^4 + x^3 + x^2 + 1 = 0x11d

_QR_MAX_MSG     = (QR_MAX_DATA * 8 - 13) * 2 / 11
QR_MAX_MSG      = (_QR_MAX_MSG - (_QR_URL_LEN + 2)) / 2  ; URL + (CRC8 * 2)
QR_TOTAL        = QR_MAX_DATA + QR_DEGREE ; e.g. 44

NUM_FIRST       = 1     ; left top 9 and bottom 8 bits are fixed!

_QR_TOTAL SET 0         ; ROM bytes used counter


;===============================================================================
; C H E C K S
;===============================================================================

; These two variables define start and end of the RAM area which can be used by
; the QR code.
; Note: Currently only ZP-RAM supported (support for non-ZP RAM shouldn't be
; a major problem)

  IFNCONST qrRamStart
    QR_ECHO ""
    QR_ECHO "!!! ERROR: qrRamStart not defined !!!"
    ERR
  ENDIF
  IFNCONST qrRamEnd
    QR_ECHO ""
    QR_ECHO "!!! ERROR: qrRamEnd not defined !!!"
    ERR
  ENDIF

  IFNCONST QR_MSG_LEN
    QR_ECHO  ""
    QR_ECHO "!!! ERROR: QR code message length not defined !!!"
    ERR
  ENDIF

  IF QR_MAX_MSG < QR_MSG_LEN
    QR_ECHO  ""
    QR_ECHO "!!! ERROR: QR code message length (", [QR_MSG_LEN]d, ") > maximum length (", [QR_MAX_MSG]d, ") !!!"
    ERR
  ENDIF


;===============================================================================
; Q R   Z P - V A R I A B L E S
;===============================================================================

    SEG.U   variables
    ORG     qrRamStart

; Memory Layout (data overlapping):
;           1         2         3         4         5         6         7
; 01234567890123456789012345678901234567890123456789012345678901234567890123456789
; rrrrrrrrrrrrrrrrmmmmmmmmmmmmmmMMMMMMMMMMMMMMttttttttt
;     qqqqqqqqqqqqqqqqqqqqqqqqqQqqqqqqqqqqqqqqqqqqqqqqqqqQQQQQQQQQQQQQQQQQQQQQQQQQ
; T                                                                         tttttt
; (r=remainder, m/M=message, t=msg tmp, q/Q=QR code, T/t=draw/qr tmps)

; Capacities:
; first column:   8 bits =  1     byte  (8 * 1)
; left block:    56 bits =  7     bytes (8 * 7)
; middle block: 187 bits = 23.375 bytes (19 * 8 + 5 * 7)
; right block:  108 bits = 13.50  bytes (11 * 8 + 5 * 4)
; total:        359 bits = 44.875 bytes
; critical: middle block overlapping, starting with ID

;---------------------------------------
; QR code variables
; all byte counts based on version 2, level L/M QR code

; input data (remainder and message):
qrData      ds QR_TOTAL             ; input data (38/44 bytes, L/M)
qrRemainder = qrData                ; (QR_DEGREE, 10/16 bytes)
qrMsgData   = qrData + QR_DEGREE    ; (QR_MAX_DATA = 34/28 bytes)
; used during add message only:
qrMsgTmpVars= qrData + QR_TOTAL
qrInputIdx  = qrMsgTmpVars          ; ZP-RAM!
qrMsgIdx    = qrMsgTmpVars + 1      ; ZP-RAM!
qrNewByte   = qrMsgTmpVars + 2      ; ZP-RAM!
qrMsgTmp    = qrMsgTmpVars + 3      ; 6 bytes
qrCrc8      = qrMsgTmp+4
qrUrlPos    = qrMsgTmp+5

qrTmpVars   = grp0RLst + QR_SIZE - 9 ; overlaps with top right eye
;- - - - - - - - - - - - - - - - - - - -
; The QR draw data overlaps the QR code data! It overwrites the QR code data
; while being drawn.
  IFNCONST QR_NON_OVER
QR_NON_OVER = 4 ; Note: can be reduced down to 1, but then relies on error correction
  ENDIF
; generated QR code data, used for drawing (76 bytes needed):
qrCodeLst   = qrData + QR_NON_OVER  ; all but 4 bytes overlap (version 2 only!)
            ds NUM_FIRST + QR_SIZE*3 - QR_TOTAL + QR_NON_OVER   ; 36 bytes
_QR_CODE_LST_SIZE   = . - qrCodeLst
_QR_RAM             = . - qrRamStart

  IF . > qrRamEnd
    QR_ECHO ""
    QR_ECHO "!!! ERROR: QR code RAM data overwrites game RAM data! (", qrRamEnd, "<", ., ") !!!"
    QR_ECHO "   ", [. - qrRamStart]d, "bytes ZP RAM required!"
    ERR
  ENDIF

; drawing variables (named for sprite display):
grp0LLst    = qrCodeLst + QR_SIZE * 0
firstMsl    = qrCodeLst + QR_SIZE * 1
grp1Lst     = qrCodeLst + NUM_FIRST + QR_SIZE * 1
grp0RLst    = qrCodeLst + NUM_FIRST + QR_SIZE * 2   ; note: this could be an extra RAM area
;- - - - - - - - - - - - - - - - - - - -
; used during drawing only:
qrDispVars  = qrData  ; 3 bytes (overlaps with qrRemainder)


;===============================================================================
; Q R   C O D E   M A C R O S
;===============================================================================

  MAC BIT_W     ; skip 2 bytes, 4 cycles
    .byte   $2c
  ENDM

;-----------------------------------------------------------
  MAC QR_ECHO
;-----------------------------------------------------------
   IF QR_ECHO_ON
    ECHO    {0}
   ENDIF
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
;   for (int j = 16-1; j >= 0; j--)
    ldx     #QR_DEGREE-1
.loopJ
;     qrRemainder[j] ^= reedSolomonMultiply(generator[j], factor);
    lda     QR_Generator,x
    _RS_MULT
    dex
    bmi     .skip0
    eor     qrRemainder,x
.skip0
    sta     qrRemainder+1,x     ; last byte uses RAM mirror address
;   }
    txa
    bpl     .loopJ
; }
    ldx     .i
    dex
    bpl     .loopI
TIM_RM_E
  ENDM ; /_RS_REMAINDER

;-----------------------------------------------------------
; Draws the raw codewords (including data and ECC) onto the given QR Code. This requires the initial state of
; the QR Code to be black at function modules and white at codeword modules (including unused remainder bits).
  MAC _DRAW_CODEWORDS
;-----------------------------------------------------------
TIM_DC_S
; Note: This part has the maximum RAM usage
.row    = qrTmpVars+0
.column = qrTmpVars+1       ; current column - 1
.y      = qrTmpVars+2       ; current column - 0/1
.iByte  = qrTmpVars+3       ; ZP-RAM!
.iBit   = qrTmpVars+4       ; ZP-RAM!

; int i = 0;  // Bit index into the data
; Note: 2600 code has data in reversed order
    lda     #$ff
    sta     .iBit           ; reset bit index
    lda     #QR_TOTAL-1
    sta     .iByte
; // Do the funny zigzag scan
; Note: 2600 code has .column decreased by 1 (for easier up/down calculation)
; for (int column = qrsize - 1; column >= 1; column -= 2) {  // Index of right column in each column pair
    ldy     #QR_SIZE-1-1    ; = 23
.loopColumns
;  if (column == 6)
    cpy     #6-1
    bne     .notColumn6
;    column = 5;
    dey                     ; skip the vertical timing column
.notColumn6
    sty     .column

;   for (int row = 0; row < qrsize; row++) {  // Vertical counter
    ldx     #QR_SIZE-1
.loopRows
    stx     .row
;       bool upward = ((column + 1) & 2) != 0; // 2600 code works in reverse
    lda     .column         ; this is tricky due to skipped vertical timing column
    lsr
    lsr                     ; defines carry
    bcc     .notUp
;       int y = upward ? qrsize - 1 - row : row;  // Actual y coordinate
    lda     #QR_SIZE-1
    sbc     .row            ; C == 1!
    tax
.notUp
    stx     .y
;     for (int j = 0; j < 2; j++) {
; some tricky code with column here:
    ldy     .column
    iny                     ; Y = column - 0 or 1
.loopJ
;       int x = column - j;  // Actual x coordinate
;       if (!getModule(qrcode, x, y) && i < dataLen * 8) {
;    ldy     .x
;    ldx     .y
    jsr     _QrCheckPixel   ; check if pixel belongs to function data
    bcs     .skipPixel      ;  yes, skip
; clear column bytes on demand:
    tya
    eor     #QR_SIZE-1
    bne     .skipClearRight
    sta     grp0RLst,x      ; A = 0
.skipClearRight
    eor     #(QR_SIZE-1-8)^(QR_SIZE-1)      ; = $08
    bne     .skipClearMiddle
    sta     grp1Lst,x       ; A = 0
.skipClearMiddle
; Note: left already fully cleared by asl qrData,x
;         bool black = getBit(qrData[i >> 3], 7 - (i & 7));
    ldx     .iByte
    asl     qrData,x       ; this also partially clears the draw data
    ldx     .y
    bcc     .skipInv
;         setModule(qrcode, x, y, black);
;    ldy     .x
    jsr     _QrInvertPixel
.skipInv
;         i++;
    lsr     .iBit
    bne     .skipByte
    dec     .iBit           ; -> $ff
    dec     .iByte          ; ZP-RAM!
    bmi     .exitDraw       ; code exits here!
.skipByte
;       }
.skipPixel
    dey                     ; left/right zigzag
    cpy     .column
    beq     .loopJ
;     } // for j
    ldx     .row            ; up/down zigzag
    dex
    bpl     .loopRows
;   } // for row
; go to next two columns:
    dey                     ; -> .column - 2
    bpl     .loopColumns    ; unconditional!
; } // for column

.exitDraw
TIM_DC_E
  ENDM ; /_DRAW_CODEWORDS

;---------------------------------------------------------------
  MAC QR_BITMAP_CODE
;---------------------------------------------------------------
_qrBitMapCode
;---------------------------------------------------------------
_QrCheckPixel SUBROUTINE
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
_QrInvertPixel SUBROUTINE
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

    QR_ECHO "  QR Code bitmap code:", [. - _qrBitMapCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrBitMapCode
  ENDM ; /QR_BITMAP_CODE

;-----------------------------------------------------------
  MAC _DRAW_FUNC
;-----------------------------------------------------------
TIM_DF_S
; Draws all function, alignment, timing and mask pattern over existing codewords
; clear horizontal timing byte:
    ldy     #0
    sty     qrCodeLst + NUM_FIRST + QR_SIZE*2-1 - 6
    ldx     #_QR_CODE_LST_SIZE-1
.loopEor
    lda     qrCodeLst,x
; clear top right eye:
    cpx     #NUM_FIRST + QR_SIZE*3-1 - 8
    bcc     .skipOra
    tya                     ; clear top, right "eye" (not cleared by message data)
.skipOra
    eor     _QrFuncData,x   ; apply function, alignment, timing and mask pattern
    sta     qrCodeLst,x
    dex
    bpl     .loopEor
TIM_DF_E
  ENDM ; /_DRAW_FUNC

; ********** The user macros and code start here: **********

;-----------------------------------------------------------
  MAC QR_START_MSG
;-----------------------------------------------------------
TIM_MS_S
; copy init data:
    ldx     #_QR_MSG_INIT_LEN
.loopInit
    lda     QrMsgInit - 1,x
    sta     qrData + QR_TOTAL - 1 - _QR_MSG_INIT_LEN,x
    dex
    bne     .loopInit
    stx     qrCrc8
    stx     qrInputIdx      ; only even or odd needed (URL has even length)
   IF QR_LEVEL = QR_LVL_L
    lda     #$15
   ENDIF
   IF QR_LEVEL = QR_LVL_M
    lda     #$0f
   ENDIF
    sta     qrMsgIdx
    lda     #$29
    sta     qrNewByte
  ENDM

;---------------------------------------------------------------
  MAC QR_ADD_MSG_CODE
;---------------------------------------------------------------
_qrAddMsgCode
;---------------------------------------------------------------
QrAddMsg SUBROUTINE
;---------------------------------------------------------------
.hexVal     = qrMsgTmp+3   ; saves 1 byte stack

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
    jsr     QrAddMsgChar
    lda     .hexVal
    and     #$0f

    ; falls through to next routine
; /QrAddMsg

;---------------------------------------------------------------
QrAddMsgChar SUBROUTINE
;---------------------------------------------------------------
; must be inside a subroutine for alphanumeric mode!
    tax
    lda     qrInputIdx
    inc     qrInputIdx      ; ZP-RAM!
    lsr                     ; 1st or 2nd byte?
    lda     qrMsgTmp
    stx     qrMsgTmp
; stored first byte will be handled
; A) by next ADD_MSG_BYTE or
; B) by STOP_MSG
    bcc     .doneFirstByte

; combine both bytes into 11 bits:
; multiply by 45 (%101101) (= 0..1980):
.prodLo     = qrMsgTmp+1           ; TODO: SC-RAM (RMW!)
.factor2    = qrMsgTmp+2

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
    adc     .factor2    ; 45-1
.noAdd
    ror
    ror     .prodLo     ; pull another bit out for the next iteration
    dex                 ; inc/dec don't modify carry; only shifts and adds do
    bne     .loopMult
; A = high byte, .prodLo = low byte of product
; add converted 2nd byte:
    tax                     ; high byte
    lda     qrMsgTmp        ; 2nd byte
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
    jsr     _QrAddBits
    lda     .prodLo         ; low byte

    ; falls through to next routine
; /QrAddMsgChar

;---------------------------------------------------------------
_QrAdd8Bits ;SUBROUTINE
;---------------------------------------------------------------
.tmpByte    = qrMsgTmp

    ldy     #8              ; 8 bits
_QrAddBits
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
.doneFirstByte
    rts
; /_QrAddBits

    QR_ECHO "  QR Code message code #2:", [. - _qrAddMsgCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrAddMsgCode

  ENDM  ; /QR_ADD_MSG_CODE

;-----------------------------------------------------------
  MAC QR_STOP_MSG
;-----------------------------------------------------------
    lda     qrCrc8
    jsr     QrAddMsg

  IF 0 ;{
; as long as we add only bytes (char pairs), this will always skip
    lda     qrInputIdx      ; TODO: this could be defined at assemble time
    lsr
    bcc     .noSecondByte
    lda     qrMsgTmp
    asl
    asl
    ldy     #6
    jsr     _QrAddBits
.noSecondByte
  ENDIF ;}
   IF !QR_PADDING
    tya                      ; Y = 0 returning from QrAddMsg/_QrAddBits
    jsr     _QrAdd8Bits      ; make sure last byte is written
   ELSE ;{
; add terminator
    lda     #(QR_TERM << 4)
    ldy     #4
    jsr     _QrAddBits
; fill and store last byte:
    ldx     qrMsgIdx
    lda     qrNewByte       ; TODO: this could be defined at assemble time
    cmp     #1              ; only byte full marker?
    beq     .emptyByte      ;  yes, byte empty
.loopBits
    asl
    bcc     .loopBits
    sta     qrMsgData,x
    dex
.emptyByte
    txa                     ; TODO: this could be defined at assemble time
    bmi     .donePadding
    lda     #$ec            ; TODO: this could be defined at assemble time
.loopPadding
    sta     qrMsgData,x
    eor     #$ec ^ $11
    dex
    bpl     .loopPadding
.donePadding
   ENDIF ;}/QR_PADDING
  ENDM ; /QR_STOP_MSG

;-----------------------------------------------------------
  MAC QR_GEN_CODE
;-----------------------------------------------------------
; This is the main macro to use!
_qrCodeCode

    QR_STOP_MSG
TIM_MS_E

TIM_GN_S
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
TIM_GN_E

    QR_ECHO "  QR Code encoding code:", [. - _qrCodeCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrCodeCode
  ENDM ; /QR_GEN_CODE

;-----------------------------------------------------------
  MAC QR_CODE_DATA
;-----------------------------------------------------------
; Add this to your code's data area
_qrCodeData

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

QrMsgInit
    .byte   $fd, $95, $2c, $fa, $bc, $ed, $54, $b3
    .byte   $56, $27
; mix mode (4 bits), length (9 bits) and first URL char (3 bits):
_QR_TOTAL_MSG_LEN   = (QR_MSG_LEN * 2) + _QR_URL_LEN + 2   ; include CRC
    .byte   #$03 + ((_QR_TOTAL_MSG_LEN & $1f) << 3)     ; 5/9 len bits
    .byte   (QR_MODE << 4) + (_QR_TOTAL_MSG_LEN >> 5)   ; 4/9 len bits
_QR_MSG_INIT_LEN    = . - QrMsgInit
_QR_URL_LEN         = 16

    QR_ECHO "  QR Code encoding data:", [. - _qrCodeData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrCodeData

  ENDM ; /QR_CODE_DATA

;---------------------------------------------------------------
  MAC QR_DRAW_CODE
;---------------------------------------------------------------
  IF QR_SPRITE_GFX
; Display: M1, P0a, P1, P0b (25 pixel)
_QR_BLOCK_H = 2     ; QR code pixel height
.tmpFirst   = qrDispVars    ; leftmost pixel column (-> M1), ZP-RAM!

_qrDrawCode
    ldx     #QR_FORE_COL    ; black QR code...
    sta     WSYNC
;---------------------------------------
    lda     #QR_BACK_COL    ; ...on white background
    sta     COLUBK
    stx     COLUP0
    stx     COLUP1
    lda     #%001|$80       ; two copies of GRP0
    sta     NUSIZ0
    sta     HMM1
    ldx     #$1f
    stx     HMP0
    inx
    stx     HMP1
    php                     ; waste 7 cycles
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
; QR code display kernel:
    ldx     #QR_SIZE-1
.loopQrKernel               ;           @70*
    ldy     #_QR_BLOCK_H    ; 2 = 2
.loopBlock
    sta     WSYNC           ; 3 = 3     @75*
;---------------------------------------
;M1-P0-P1-P0
    cpx     #15             ; 2
    bne     .notMidFirst    ; 3/2
    lda     firstMsl        ; 3
    bcs     .setTmpFirst    ; 3 = 10

.skipSetFirst               ;10
    bne     .contKernel     ; 3

.notMidFirst                ; 5
    cpx     #7              ; 2
    bne     .skipSetFirst   ; 3/2
    lda     #$fe            ; 2 = 11
.setTmpFirst
    sta     .tmpFirst       ; 3 = 3
.contKernel                 ;           @13/14
    lda     .tmpFirst       ; 3
    asl                     ; 2
    sta     ENAM1           ; 3 =  8
    lda     grp1Lst,x       ; 4
    sta     GRP1            ; 3
    lda     grp0LLst,x      ; 4
    sta     GRP0            ; 3 = 14
    sec                     ; 2         needed for 1st ror (25th bit)
    lda     grp0RLst,x      ; 4
    nop                     ; 2
    dey                     ; 2
    sta     GRP0            ; 3 = 13    @48/49
    bne     .loopBlock      ; 2/3
    ror     .tmpFirst       ; 5
    dex                     ; 2
    bpl     .loopQrKernel   ; 3/2=12/11 @60/59*
    sty     ENAM1           ; 3
    sty     GRP1            ; 3
    sty     GRP0            ; 3
    ldx     #{2}
.loopWaitBtm
    sta     WSYNC
;---------------------------------------
    dex
    bne     .loopWaitBtm

    QR_ECHO "  QR Code sprite kernel:", [. - _qrDrawCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrDrawCode

  ELSE ; /QR_SPRITE_GFX

_QR_BLOCK_H = 7
.tmpFirst   = qrDispVars    ; leftmost pixel column (-> M1), ZP-RAM!
.pf0R1LLst  = grp0LLst
.pf2LLst    = grp1Lst
.pf1RLst    = grp0RLst

_qrDrawCode
    lda     #QR_FORE_COL    ; black QR code...
    sta     COLUPF
    lda     #QR_BACK_COL    ; ...on white background
    sta     COLUBK

; some vertical centering:
    ldx     #{1}            ; (200 - QR_SIZE * _QR_BLOCK_H) / 2
.waitTop
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitTop
    stx     CTRLPF

    lda     #%01111111      ;           = $7f
    sta     .tmpFirst
; QR code display kernel:
    ldx     #QR_SIZE-1
.loopQrKernel               ;           @60
    ldy     #_QR_BLOCK_H    ; 2

    cpx     #15             ; 2
    bne     .notMidFirst    ; 3/2
    lda.w   firstMsl        ; 4
    bcs     .setTmpFirst    ; 3 = 11

.notMidFirst                ; 5
    cpx     #7              ; 2
    bne     .contKernel     ; 3/2
    lda     #%11111110      ; 2 = 11    = $fe
.setTmpFirst
    sta     .tmpFirst       ; 3 =  3    @76!
;---------------------------------------
    bcs     .contKernel1    ; 3         @03!

; |PF0 |  PF1   |  PF2   |PF0 |  PF1   |  PF2   |
; |    |7......0|0......7|4..7|7......0|        |
; |....|...XXXXX|XXXXXXXX|XXXX|XXXXXXXX|........|
.loopBlock                  ;           @45
    lda     #0              ; 2
    sta     PF2             ; 3         @50
    sta     PF0             ; 3         @53
.contKernel                 ;           @72
    sta     WSYNC
;---------------------------------------
    lda     .tmpFirst       ; 3 =  3
.contKernel1                ;           @03
    lsr                     ; 2
    lda     .pf0R1LLst,x    ; 4
    and     #%1111          ; 2
    bcs     .setFirst       ; 2/3
    bcc     .clearFirst     ; 3

.setFirst
    ora     #%10000         ; 2         CF needed for 1st ror
.clearFirst
    sta     PF1             ; 3 = 16    @19
    lda     .pf2LLst,x      ; 4
    sta     PF2             ; 3         @26
    lda     .pf0R1LLst,x    ; 4
    sta     PF0             ; 3         @33     >=27
    lda     .pf1RLst,x      ; 4
    sta     PF1             ; 3 = 21    @40     >=38
    dey                     ; 2
    bne     .loopBlock      ; 3/2= 5/4  @44/45
    ror     .tmpFirst       ; 5         ROR required for 25th bit
    dex                     ; 2
    sty     PF2             ; 3
    sty     PF0             ; 3 = 13    @57
    bpl     .loopQrKernel   ; 3/2= 3/2  @60
    sty     PF1             ; 3         @62

    ldx     #{2}
.waitBtm
    dex
    sta     WSYNC
;---------------------------------------
    bne     .waitBtm
; (was 733 now 735)

    QR_ECHO "  QR Code PF kernel:", [. - _qrDrawCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrDrawCode
  ENDIF ; /!QR_SPRITE_GFX
 ENDM ; /QR_DRAW_CODE

;---------------------------------------------------------------
  MAC _QR_ARRANGE_DRAW_DATA
;---------------------------------------------------------------
TIM_AS_S
   IF !QR_SPRITE_GFX
; rearrange bitmap data for PF display
; |PF0 |  PF1   |  PF2   |PF0 |  PF1   |  PF2   |
; |    |7......0|0......7|4..7|7......0|        |
; |....|...XXXXX|XXXXXXXX|XXXX|XXXXXXXX|........|
;           |  P0L   |   P1   |  P0R   |
;          0|abcdefgh|ijklmnop|qrstuvwx| ->
;       -> 0|ponmabcd|lkjihgfe|qrstuvwx|

    ldx     #QR_SIZE-1
.loopRows
; rearrange low 4 bits of grp1Lst into grp0LLst:
    lda     grp0LLst,x
    pha
    lda     grp1Lst,x
    ldy     #4
.loopShift0a
    lsr                     ; 3..0 -> 0..3
    rol     grp0LLst,x
    dey
    bne     .loopShift0a
; rearrange high 4 bits of grp1Lst into grp1Lst:
    ldy     #4
.loopShift1a
    lsr
    rol     grp1Lst,x
    dey
    bne     .loopShift1a
; rearrange high 4 bits of grp0LLst into grp0LLst:
    pla
    pha
    ldy     #4
.loopShift0b
    asl
    rol     grp0LLst,x
    dey
    bne     .loopShift0b
; rearrange low 4 bits of grp0LLst into grp1Lst:
    pla
    ldy     #4
.loopShift1b
    lsr
    rol     grp1Lst,x
    dey
    bne     .loopShift1b
; loop:
    dex
    bpl     .loopRows
   ENDIF ; / !QR_SPRITE_GFX
TIM_AS_E
  ENDM

;---------------------------------------------------------------
  MAC _QR_AM    ; pattern mask, value
;---------------------------------------------------------------
; apply mask 0
   LIST OFF
   IF _QR_MASK_IDX
    LIST ON
    .byte   ({2}) ^ ($aa & {1})
    LIST OFF
   ELSE
    LIST ON
    .byte   ({2}) ^ ($55 & {1})
    LIST OFF
   ENDIF
_QR_MASK_IDX SET _QR_MASK_IDX ^ 1
   LIST ON
  ENDM

;---------------------------------------------------------------
  MAC _QR_FUNC_GFX
;---------------------------------------------------------------
_QR_MASK_IDX SET 0

_QrFuncData
; data for QR code version 2, level 0 or 1, mask 0
;GRP0LFunc
    _QR_AM  %00000000, %11111100 | (({1} >> 7) & %1) ; constant, bit 0 of 2nd format copy, level
    _QR_AM  %00000000, %00000100 | (({1} >> 6) & %1) ; constant, bit 1 of 2nd format copy, level
    _QR_AM  %00000000, %01110100 | (({1} >> 5) & %1) ; constant, bit 2 of 2nd format copy, pattern
    _QR_AM  %00000000, %01110100 | (({1} >> 4) & %1) ; constant, bit 3 of 2nd format copy, pattern
    _QR_AM  %00000000, %01110100 | (({1} >> 3) & %1) ; constant, bit 4 of 2nd format copy, pattern
    _QR_AM  %00000000, %00000100 | (({1} >> 2) & %1) ; constant, bit 5 of 2nd format copy, ECC
    _QR_AM  %00000000, %11111100 | (({1} >> 1) & %1) ; constant, bit 6 of 2nd format copy, ECC
    _QR_AM  %00000000, %00000001    ;                  constant, 1 (dark module)
    _QR_AM  %11111011, %00000100    ;  8
    _QR_AM  %11111011, %00000000
    _QR_AM  %11111011, %00000100    ; 10
    _QR_AM  %11111011, %00000000
    _QR_AM  %11111011, %00000100    ; 12
    _QR_AM  %11111011, %00000000
    _QR_AM  %11111011, %00000100    ; 14
    _QR_AM  %11111011, %00000000
    _QR_AM  %00000000, (({1} << 1) & %11111000) | (({1} & %11) | %100) ; constant, bits 1..7 of 1st format copy, 1 (timing bit)
    _QR_AM  %00000000, %00000000 | (({2} >> 7) & %1) ; constant, bit  8 of 1st format copy, ECC
    _QR_AM  %00000000, %11111101    ; 18               constant, 1 (timing bit)                                     ; 18
    _QR_AM  %00000000, %00000100 | (({2} >> 6) & %1) ; constant, bit  9 of 1st format copy, ECC
    _QR_AM  %00000000, %01110100 | (({2} >> 5) & %1) ; constant, bit 10 of 1st format copy, ECC ; 20
    _QR_AM  %00000000, %01110100 | (({2} >> 4) & %1) ; constant, bit 11 of 1st format copy, ECC
    _QR_AM  %00000000, %01110100 | (({2} >> 3) & %1) ; constant, bit 12 of 1st format copy, ECC ; 22
    _QR_AM  %00000000, %00000100 | (({2} >> 2) & %1) ; constant, bit 13 of 1st format copy, ECC
    _QR_AM  %00000000, %11111100 | (({2} >> 1) & %1) ; constant, bit 14 of 1st format copy, ECC ; 24
;FirstFunc
;_QR_MASK_IDX SET _QR_MASK_IDX ^ 1
    _QR_AM  %11111111, %00000000
;_QR_MASK_IDX SET _QR_MASK_IDX ^ 1
;GRP1Func
    _QR_AM  %11111111, %00000000    ;  0
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ;  2
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111110, %00000001    ;  4    alignment pattern
    _QR_AM  %11111110, %00000001
    _QR_AM  %11111110, %00000001    ;  6
    _QR_AM  %11111110, %00000001
    _QR_AM  %11111110, %00000001    ;  8
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 10
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 12
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 14
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 16
    _QR_AM  %11111111, %00000000
    _QR_AM  %00000000, %01010101    ; 18    horizontal timing pattern
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 20
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 22
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 24
_QR_MASK_IDX SET _QR_MASK_IDX ^ 1
;GRP0RFunc
    _QR_AM  %11111111, %00000000    ;  0
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ;  2
    _QR_AM  %11111111, %00000000
    _QR_AM  %00001111, %11110000    ;  4    alignment pattern
    _QR_AM  %00001111, %00010000
    _QR_AM  %00001111, %01010000    ;  6
    _QR_AM  %00001111, %00010000
    _QR_AM  %00001111, %11110000    ;  8
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 10
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 12
    _QR_AM  %11111111, %00000000
    _QR_AM  %11111111, %00000000    ; 14
    _QR_AM  %11111111, %00000000
    _QR_AM  %00000000, (({1} << 7) & $80) | ({2} >> 1) ; bits 7..14 of 2nd format copy
    _QR_AM  %00000000, %00000000    ;       constant    top, right "eye"
    _QR_AM  %00000000, %01111111    ; 18    constant
    _QR_AM  %00000000, %01000001    ;       constant
    _QR_AM  %00000000, %01011101    ; 20    constant
    _QR_AM  %00000000, %01011101    ;       constant
    _QR_AM  %00000000, %01011101    ; 22    constant
    _QR_AM  %00000000, %01000001    ;       constant
    _QR_AM  %00000000, %01111111    ; 24    constant
  ENDM ; /_QR_FUNC_GFX

;---------------------------------------------------------------
  MAC QR_DRAW_DATA
;---------------------------------------------------------------
_qrFuncData ; for 25 pixel

  IF QR_LEVEL = QR_LVL_L
    _QR_FUNC_GFX %11101111, %10001000
  ENDIF
  IF QR_LEVEL = QR_LVL_M
    _QR_FUNC_GFX %10101000, %00100100
  ENDIF

    QR_ECHO "  QR Code function modules data:", [. - _qrFuncData]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrFuncData
  ENDM  ;/QR_DRAW_DATA
