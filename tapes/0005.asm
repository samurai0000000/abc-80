; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0005
; Annotation: Lab05: BCD Subtraction (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 5：算術減法練習 例三 (pp. 116-117)
;   Subtracts 3-byte BCD 796825 (25h,68h,79h at 1060h..1062h)
;   minus 335836 (36h,58h,33h at 1050h..1052h) = 460989 (89h,09h,46h)
;   using SBC A,(HL) and DAA.
; Enhancement: Cycles Difference (460989), Minuend (796825), and Subtrahend
;   (335836) on the 6-digit 7-segment display with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Initialize 1050h..1052h = 335836 and compute book DEF ---
            call    init_sub        ; [1000] [cd 31 10] 1050h..1052h <- 36h, 58h, 33h
            call    do_sub          ; [1003] [cd 1f 10] 1050h..1052h <- 89h, 09h, 46h (460989)
            call    show3           ; [1006] [cd 3d 10] display Difference "460989"

            ; --- Show Minuend ("796825") ---
            ld      hl,1060h        ; [1009] [21 60 10]
            ld      de,1050h        ; [100c] [11 50 10]
            ld      bc,0003h        ; [100f] [01 03 00]
            ldir                    ; [1012] [ed b0]
            call    show3           ; [1014] [cd 3d 10]

            ; --- Show Subtrahend ("335836") then recompute Difference ---
            call    init_sub        ; [1017] [cd 31 10]
            call    show3           ; [101a] [cd 3d 10]
            jr      start           ; [101d] [18 e1]

            ; --- Exact Book Example 3 Multi-Byte BCD Subtraction Routine ---
do_sub:     ld      hl,104fh        ; [101f] [21 4f 10] HL -> byte count (03h)
            ld      b,(hl)          ; [1022] [46]       B = 03h
            ld      de,1060h        ; [1023] [11 60 10] DE -> minuend (1060h..1062h)
            xor     a               ; [1026] [af]       clear Borrow (Carry) flag
            inc     hl              ; [1027] [23]       HL -> subtrahend (1050h..1052h)
def_lp:     ld      a,(de)          ; [1028] [1a]       load minuend byte
            sbc     a,(hl)          ; [1029] [9e]       subtract subtrahend byte with borrow
            daa                     ; [102a] [27]       decimal adjust for BCD
            ld      (hl),a          ; [102b] [77]       store difference byte into 1050h..1052h
            inc     hl              ; [102c] [23]
            inc     de              ; [102d] [13]
            djnz    def_lp          ; [102e] [10 f8]
            ret                     ; [1030] [c9]

init_sub:   ld      hl,subtra       ; [1031] [21 53 10]
            ld      de,1050h        ; [1034] [11 50 10]
            ld      bc,0003h        ; [1037] [01 03 00]
            ldir                    ; [103a] [ed b0]
            ret                     ; [103c] [c9]

show3:      ld      b,03h           ; [103d] [06 03]
            call    clkset          ; [103f] [cd c3 06]
            call    ms3k17          ; [1042] [cd 64 04]
            ld      b,32h           ; [1045] [06 32] 50 scan passes (~0.50s)
scan:       call    scd_k1          ; [1047] [cd cd 04]
            djnz    scan            ; [104a] [10 fb]
            ret                     ; [104c] [c9]

            defs    104fh - $, 00h
            db      03h             ; [104f] [03] byte count = 3
            db      36h, 58h, 33h   ; [1050..1052] initial subtrahend / diff buffer (335836)
subtra:     db      36h, 58h, 33h   ; [1053..1055] backup copy of subtrahend (335836)
            defs    1060h - $, 00h
            db      25h, 68h, 79h   ; [1060..1062] minuend (796825)
