; *** HSC QR code demo ***
; (C)2021/2026 Thomas Jentzsch

; This demo shows, how to use the QR code generation library for displaying
; high score QR codes. These can be scanned with your smartphone (most cameras
; support them natively) and then send to the PlusROM High Score Club.
; This allows adding high scores without using a PlusCart or emulator.

; *** General Use ***
; There are only a few DASM macros you have to use:
;
; For QR Code generation:
; - QR_START_MSG
; - add payload (using QrAddMsg subroutine for each byte)
; - QR_GEN_CODE
;
; For QR Code display:
; - QR_DRAW_CODE {upper border} {lower border}
;
; Additional macros (put them where you want):
; - QR_ADD_MSG_CODE
; - QR_BITMAP_CODE
; - QR_DRAW_DATA
; - QR_CODE_DATA


    processor 6502
  LIST OFF
    include vcs.h
  LIST ON

BASE_ADR        = $f000

NTSC_TIM        = 1         ; 0 for PAL-50

;PLUSROM_ID      = 255       ; fake ID
PLUSROM_ID      = 57
SCORE_BYTES     = 3         ; example number


;===============================================================================
; Q R   A S S E M B L E R - S W I T C H E S
;===============================================================================

QR_BACK_COL     = $0e   ; white
QR_FORE_COL     = $80   ; black
; Note: other color combinations work too, as long as the contrast is high enough

QR_SPRITE_GFX   = 0 ; (-30 bytes) display playfield(0) or sprite graphics(1)
; Sprite graphics are small, but sufficient. And allow to display your own
; graphics above and below.
; Note: Step away from the display if your QR code reader has problems.

; define message payload size:
QR_MSG_LEN      = 1 + SCORE_BYTES + 1 + 1;+4;+4; PlusROM game ID, 3 x score, stage, variation
; Note: An optional, short user id is planned. This will be mapped to an
; existing, long id. So that no further input is quired on the website.
; The user id would be entered inside the game then. There it could be stored
; and reused using e.g. the SaveKey.

;QR_LEVEL        = QR_LVL_L ; error correction level (default M)
; Enable this if your payload exceeds the maximum message size. This will weaken
; error correction but provide space for 4 extra chars.

QR_PADDING      = 1         ; add padding bytes to fill any space left
; Usually QR reader simply ignore the padding bytes. If you want to be 100%
; correct, you can enable this line. This costs 29 extra bytes ROM.

QR_ECHO_ON      = 1 ; 1 = echo some debug output to console
; Enable line for some debug output


;===============================================================================
; Z P - V A R I A B L E S
;===============================================================================

    SEG.U   variables
    ORG     $80

tmpVars     ds 2            ; score loop, fireButton, resync
; example variables:
scoreLst    ds SCORE_BYTES  ; game score
scoreLo     = scoreLst
scoreMid    = scoreLst+1
scoreHi     = scoreLst+2
stage       ds 1            ; game stage (level, wave...)
variation   ds 1            ; game variation

; these two variables define the RAM area for the QR code generation:
qrRamStart                  ; QR code generation needs a LOT of ZP-RAM, which
    ds      80              ; starts here. Organize your RAM so that you have a
                            ; large unused area of RAM after the game ends.
                            ; This is the most tricky part for you!
qrRamEnd                    ; end of QR code RAM


;===============================================================================
; Q R   M A C R O S
;===============================================================================

  LIST OFF
    include QRCodeGen2600.asm ; contains all QR code generation code
  LIST ON


;===============================================================================
; R O M - C O D E
;===============================================================================
    SEG     Bank0
    ORG     BASE_ADR, $00

;---------------------------------------------------------------
Start SUBROUTINE
;---------------------------------------------------------------
;    lda     #0
;    tax
    cld                     ; clear BCD math bit
;.clearLoop
;    dex
;    txs
;    pha
;    bne     .clearLoop

; clear TIA only, keep ZP-RAM random for testing:
    ldx     #$7f
    lda     #0
.loopClear
    sta     $00,x
    dex
    bpl     .loopClear
    txs

 ; define demo "game results":
;    lda     #$14
;    sta     variation
;    lda     #$12
;    sta     scoreHi
;    lda     #$34
;    sta     scoreMid
;    lda     #$56
;    sta     scoreLo
;    lda     #$23
;    sta     stage

    lda     #$15
    sta     variation
    lda     #$F7
    sta     scoreHi
    lda     #$aa
    sta     scoreMid
    lda     #$24
    sta     scoreLo
    lda     #$DE
    sta     stage

    lda     #2
    sta     VBLANK

; just loop generation and display:
.loop4Ever
    jsr     GenerateQrCode  ; QR code generation
    jsr     DisplayQrCode   ; generated QR code display
    jmp     .loop4Ever      ; usually one would continue with the game here

;---------------------------------------------------------------
DisplayQrCode SUBROUTINE
;---------------------------------------------------------------
fireButton  = tmpVars
resync      = tmpVars+1

    lda     #2-1            ; debounce fire button and init resync
    sta     fireButton      ; mark as pressed before, 2 state changes required
    sta     resync          ; it takes ~8 frames to generate the QR code, so the
                            ; next 6 frames are displayed blank for resync

.mainLoop
    lda     #%00001110
.loopVSync:
    sta     WSYNC
;---------------------------------------
    sta     VSYNC
    lsr
    bne     .loopVSync
; VerticalBlank:
  IF QR_SPRITE_GFX
_EXTRA_LINES    = 0
  ELSE
_EXTRA_LINES    = 4         ; PF display needs some extra lines for a nice gap
  ENDIF

  IF NTSC_TIM
    lda     #44-_EXTRA_LINES
  ELSE
    lda     #77-_EXTRA_LINES
  ENDIF
    sta     TIM64T

.waitVBTim:
    lda     INTIM
    bne     .waitVBTim
    sta     WSYNC
;---------------------------------------
    asl     resync
    bne     .blackScreen    ; no display during resync
    sta     VBLANK
.blackScreen

; make sure there is a litte gap above and below the QR code!
  IF QR_SPRITE_GFX
    QR_DRAW_CODE 70, 70     ; gaps above and below QR code
; Note: you can draw your own graphics above and below (or besides)
  ELSE
    QR_DRAW_CODE 11, 11     ; gaps above and below QR code
  ENDIF

    lda     #2
    sta     VBLANK

; OverScan:
  IF NTSC_TIM
    lda     #36-_EXTRA_LINES
  ELSE
    lda     #63-_EXTRA_LINES
  ENDIF
    sta     TIM64T

; release -> press -> release
    ldy     fireButton
    lda     INPT4
    eor     FireStates,y    ; state changed
    bpl     .continue       ;  no, continue
    dec     fireButton      ;  yes, number of state changes == 0?
    bmi     .exitDisplay    ;  yes, exit display
.continue
.waitOVTim
    lda     INTIM
    bne     .waitOVTim
    jmp     .mainLoop

.exitDisplay
    rts                     ; we are done

FireStates
    .byte   $80, $00
; /DisplayQrCode

;---------------------------------------------------------------
GenerateQrCode SUBROUTINE
;---------------------------------------------------------------
.msgPos     = tmpVars

; stop any audio (optional):
;    lda     #0
;    sta     AUDV0
;    sta     AUDV1
; reset some TIA registers (optional):
  IF QR_SPRITE_GFX
;    sta     NUSIZ1
;    ...
  ENDIF

; *** Generate QR code and resulting graphics from message ***
; Note: The space calculations are only for debugging

_qrMessageCode
; initialize the QR code generation:
    QR_START_MSG            ; start adding your message payload

; add the payload bytes (as defined for PlusROM HSC):
.scoreIdx   = tmpVars

; Note: add the values in the same order as if sending them directly to the HSC
; add PlusROM game ID:
    lda     #PLUSROM_ID
    jsr     QrAddMsg
; add game variation:
    lda     variation
    jsr     QrAddMsg
; add high, mid, low score values:
    ldx     #SCORE_BYTES-1
.loopAddScore
    stx     .scoreIdx
    lda     scoreLst,x
    jsr     QrAddMsg
    ldx     .scoreIdx
    dex
    bpl     .loopAddScore
; add stage:
    lda     stage
    jsr     QrAddMsg
; H.FIRMAPLUS.DE/Q3915F7AA24DE5C

; H.FIRMAPLUS.DE/Q3915F7AA24DE6A3F4158BA
;    lda     #$6a
;    jsr     QrAddMsg
;    lda     #$3F
;    jsr     QrAddMsg
;    lda     #$41
;    jsr     QrAddMsg
;    lda     #$58
;    jsr     QrAddMsg

;; H.FIRMAPLUS.DE/Q3915F7AA24DE6A3F4158FD6CE57FBE
;    lda     #$FD
;    jsr     QrAddMsg
;    lda     #$6C
;    jsr     QrAddMsg
;    lda     #$E5
;    jsr     QrAddMsg
;    lda     #$7F
;    jsr     QrAddMsg

    QR_ECHO "  QR Code message code #1:", [. - _qrMessageCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrMessageCode

; generate the QR code for the given message:
    QR_GEN_CODE             ; here all the magic happens! :-)
    rts
; /GenerateQrCode

; Note: these macros are split into 4 parts for more flexible use
; they include some extra QR code:
    QR_ADD_MSG_CODE
    QR_BITMAP_CODE

; include QR code data:
    QR_DRAW_DATA
    QR_CODE_DATA

    ORG BASE_ADR  + $ffc
    .word   Start
    .word   0


;===============================================================================
; O U T P U T
;===============================================================================

    QR_ECHO "  --------------------------------------------"
    QR_ECHO "  QR Code total:", [_QR_TOTAL]d, "bytes ROM,", [_QR_RAM]d, "bytes RAM"
    QR_ECHO ""
    QR_ECHO "  QR Code Version", [QR_VERSION]d, ", Level", [QR_LEVEL]d, ", Degree", [QR_DEGREE]d, ", Mode", [QR_MODE]d, "(Alphanumeric) -> Capacity", [QR_CAPACITY_BITS]d, "bits"
    QR_ECHO "    -> Message Space:", [QR_MAX_MSG]d, "bytes (", [QR_MSG_LEN]d, "used )"
  IF QR_PADDING
    QR_ECHO ""
    QR_ECHO "  *** QR Code padding enabled ***"
  ENDIF