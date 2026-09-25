; Info:
; - the demo code starts at DoQrCode1 and ends at ExitQrCode
; - only the payload of the message is added, using QrAddMsg for each byte
; - the URL and checksum are created automatically


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
; Q R   M A C R O S
;===============================================================================

  include QRCodeGen2600.inc ; contains all QR code generation code


;===============================================================================
; D E M O   V A R S
;===============================================================================

fireButton  = tmpVars
resync      = tmpVars+1


;===============================================================================
; R O M - C O D E
;===============================================================================
    START_BANK  1

;---------------------------------------------------------------
DisplayQrCode SUBROUTINE
;---------------------------------------------------------------
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
    tya                     ; needed for Bot & Tom
    jmp     ExitQrCode      ; we are done

FireStates
    .byte   $80, $00
; /DisplayQrCode


;---------------------------------------------------------------
DoQrCode1 SUBROUTINE
;---------------------------------------------------------------
.msgPos     = tmpVars

 ; some clean up from Bot & Tom code:
    lda     gameFlags
    and     #~GAME_DO_QR
    sta     gameFlags

; stop any audio (optional):
    lda     #0
    sta     AUDV0
    sta     AUDV1
; reset some TIA registers (optional):
  IF QR_SPRITE_GFX
    sta     NUSIZ1
    sta     VDELP0

   IF 0
    lda     #$56
    sta     scoreLo
    lda     #$34
    sta     scoreMid
    lda     #$12
    sta     scoreHi
   ENDIF
  ENDIF

; *** Generate QR code and resulting graphics from message ***
TIM_MS_S
_MessageCode
; initialize the QR code generation:
    lda     #QR_MSG_LEN
    jsr     QrStartMsg
DEBUG0

; add the payload bytes (as defined for PlusCart HSC):
.scoreIdx   = tmpVars

    lda     #PLUSROM_ID
    jsr     QrAddMsg

    LOAD_VARIATION              ; ..ddttSP
    and     #%00111111
    jsr     QrAddMsg

    ldx     #SCORE_BYTES-1      ; score
.loopAddQrData
    stx     .scoreIdx
    lda     scoreLst,x
    jsr     QrAddMsg            ; hi, mid, lo
    ldx     .scoreIdx
    dex
    bpl     .loopAddQrData

    lda     stage               ; stage last
    jsr     QrAddMsg

; finish adding payload:
DEBUG1
    QR_STOP_MSG
TIM_MS_E

; define message payload size here:
QR_MSG_LEN = 1 + 3 + 1 + 1  ; {+ 2}, game ID, 3 x score, stage, variation {, userId}

  IF QR_MAX_MSG < QR_MSG_LEN
    ECHO     ""
    ECHO    "!!! ERROR: QR code message length (", [QR_MSG_LEN]d, ") > maximum length (", [QR_MAX_MSG]d, ") !!!"
    ERR
  ENDIF

    ECHO    "    QR Code message code #1:", [. - _MessageCode]d, "bytes"
_QR_TOTAL SET _QR_TOTAL + . - _MessageCode

; generate the QR code for the given message:
TIM_GN_S
    QR_GEN_CODE
TIM_GN_E

    lda     #2-1
    sta     fireButton      ; mark as pressed before, 2 state changes required
    sta     resync          ; next 6 frames are displayed blank
    jmp     DisplayQrCode

; include some extra QR code:
    QR_ADD_MSG_CODE
    QR_BITMAP_CODE


;===============================================================================
; Q R   R O M - T A B L E S (Bank 1)
;===============================================================================
    ALIGN_FREE_LBL 256, "QR Rom Tables"

; include QR code data:
; Platform and version specific function module data definition
    QR_DRAW_DATA
    QR_CODE_DATA

; return to game code from this demo:
    RORG_FREE_LBL Bankswitching0 - 3, "Bankswitching"
Bankswitching1
Start1
    bit     BANK0
    ds      3, 0
ExitQrCode
    bit     BANK0
    jmp     DoQrCode1

    END_BANK 1

    ECHO    "---------------------------------------------------"
    ECHO    "    QR Code total:", [_QR_TOTAL]d, "bytes ROM,", [_QR_RAM]d, "bytes RAM"
    ECHO    ""
    ECHO    "    QR Code Version", [QR_VERSION]d, ", Level", [QR_LEVEL]d, ", Degree", [QR_DEGREE]d, ", Mode", [QR_MODE]d, "(Alphanumeric) -> Capacity", [QR_CAPACITY_BITS]d, "bits"
    ECHO    "      -> Message Space", [QR_MAX_MSG]d, "chars (", [QR_MSG_LEN]d, "used )"
