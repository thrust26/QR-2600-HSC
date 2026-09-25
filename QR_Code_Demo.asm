; *** HSC QR code demo ***
; (C)2021/2026 Thomas Jentzsch

; This demo shows, how to use the QR code generation library for displaying
; high score QR codes. These can be scanned with your smartphone (most cameras
; support them natively) and then send to the PlusROM High Score Club.
; This allows adding high scores without using a PlusCart or emulator.

; *** General Use ***
; QR Code generation:
; - QR_START_MSG
; - add payload (using QrAddMsg for each byte)
; - QR_GEN_CODE
; QR Code display:
; - QR_DRAW_CODE {upper border} {lower boarder}


    processor 6502
  LIST OFF
    include vcs.h
  LIST ON

BASE_ADR        = $f000

NTSC_TIM        = 1

;PLUSROM_ID      = 255       ; fake ID
PLUSROM_ID      = 57
SCORE_BYTES     = 3         ; example number

; define message payload size:
QR_MSG_LEN = 1 + 3 + 1 + 1  ; PlusROM game ID, 3 x score, stage, variation
; Note: An optional, short user id is planned, so that no further input
; is quired on the website. The user id would be entered inside the game then.
; There it could be stored and reused using e.g. the SaveKey.


;===============================================================================
; Q R   A S S E M B L E R - S W I T C H E S
;===============================================================================

;QR_LEVEL        = QR_LVL_L ; error correction level (default M)
; Enable this if your payload exceeds the maximum message size. This will weaken
; error correction but provide space for 4 extra chars.
QR_SPRITE_GFX   = 1 ; (-38 bytes) display playfield(0) or sprite graphics(1)
; Sprite graphics are small, but sufficient. And allow to display your own
; graphics above and below.
; Note: Step away from the display if your QR code reader has problems.


;===============================================================================
; Z P - V A R I A B L E S
;===============================================================================

    SEG.U   variables
    ORG     $80

tmpVars     ds 2            ; fireButton, resync
; example variables
scoreLst    ds SCORE_BYTES  ; game score
scoreLo     = scoreLst
scoreMid    = scoreLst+1
scoreHi     = scoreLst+2
stage       ds 1            ; game stage (level, wave...)
variation   ds 1            ; game variation

qrRamStart                  ; QR code generation needs a LOT of ZP-RAM, which starts here
    ds      83              ; organize your RAM so that you have a large unused area
                            ;  after the game ends,
                            ;  This is the most tricky part for you!
qrRamEnd                    ; end of QR code RAM

; temporary vars used by demo code:
fireButton  = tmpVars
resync      = tmpVars+1


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
    lda     #0
    tax
    cld                     ; clear BCD math bit
.clearLoop
    dex
    txs
    pha
    bne     .clearLoop

 ; define demo "game results":
    lda     #$56
    sta     scoreLo
    lda     #$34
    sta     scoreMid
    lda     #$12
    sta     scoreHi
    lda     #3
    sta     stage
    lda     #1
    sta     variation

.loop4Ever
    jsr     GenQrCode       ; QR code generation
    jsr     DisplayQrCode   ; display generated QR code
    jmp     .loop4Ever      ; usually one would continue with the game here

;---------------------------------------------------------------
DisplayQrCode SUBROUTINE
;---------------------------------------------------------------
    lda     #2-1
    sta     fireButton      ; mark as pressed before, 2 state changes required
    sta     resync          ; next 6 frames are displayed blank

.mainLoop
    lda     #%00001110
.loopVSync:
    sta     WSYNC
;---------------------------------------
    sta     VSYNC
    lsr
    bne     .loopVSync
; VerticalBlank:
  IF NTSC_TIM
    lda     #44-4
  ELSE
    lda     #77-4
  ENDIF
    sta     TIM64T

.waitVBTim:
    lda     INTIM
    bne     .waitVBTim
    sta     WSYNC
;---------------------------------------
    asl     resync
    bne     .blackScreen
    sta     VBLANK
.blackScreen

; make sure there is a litte gap above and below the QR code!
  IF QR_SPRITE_GFX
    QR_DRAW_CODE 72, 73     ; gaps above and below QR code
  ELSE
    QR_DRAW_CODE 11, 11     ; gaps above and below QR code
  ENDIF

    lda     #2
    sta     VBLANK

; OverScan:
  IF NTSC_TIM
    lda     #36-4
  ELSE
    lda     #63-4
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
GenQrCode SUBROUTINE
;---------------------------------------------------------------
.msgPos     = tmpVars

; stop any audio (optional):
    lda     #0
    sta     AUDV0
    sta     AUDV1
; reset some TIA registers (optional):
  IF QR_SPRITE_GFX
    sta     NUSIZ0
    sta     NUSIZ1
    sta     VDELP0
;   ...
  ENDIF

; *** Generate QR code and resulting graphics from message ***
; Note: The space calculations are only for debugging

_qrMessageCode
; initialize the QR code generation:
    QR_START_MSG            ; start adding your message payload

; add the payload bytes (as defined for PlusCart HSC):
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

    ECHO    "    QR Code message code #1:", [. - _qrMessageCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _qrMessageCode

; generate the QR code for the given message:
    QR_GEN_CODE             ; here all the magic happens! :-)
    rts
; /GenQrCode

; Note: includes are split into 4 parts for more flexible use
; include some extra QR code:
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

    ECHO    "---------------------------------------------------"
    ECHO    "    QR Code total:", [_QR_TOTAL]d, "bytes ROM,", [_QR_RAM]d, "bytes RAM"
    ECHO    ""
    ECHO    "    QR Code Version", [QR_VERSION]d, ", Level", [QR_LEVEL]d, ", Degree", [QR_DEGREE]d, ", Mode", [QR_MODE]d, "(Alphanumeric) -> Capacity", [QR_CAPACITY_BITS]d, "bits"
    ECHO    "      -> Message Space", [QR_MAX_MSG]d, "chars (", [QR_MSG_LEN]d, "used )"
