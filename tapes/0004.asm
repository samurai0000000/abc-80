; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0004
; Annotation: Lab04: BCD Addition (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 4：算術加法練習 例四 (pp. 113-114)
;   Adds 3-byte BCD 473256 (56h,32h,47h) + 422344 (44h,23h,42h) = 895600 (00h,56h,89h)
;   at 1050h..1052h using ADC A,(HL) and DAA.
; Enhancement: Cycles Addend 1 (473256), Addend 2 (422344), and Sum (895600) on
;   the 6-digit 7-segment display with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Initialize 1050h..1052h = 473256 and compute book SUM ---
            call    init_a1         ; [1000] [cd 2f 10] 1050h..1052h <- 56h, 32h, 47h
            call     do_sum          ; [1003] [cd 1e 10] 1050h..1052h <- 00h, 56h, 89h (895600)
            call    show3           ; [1006] [cd 3b 10] display Sum "895600"

            ; --- Show Addend 1 ("473256") ---
            call    init_a1         ; [1009] [cd 2f 10]
            call    show3           ; [100c] [cd 3b 10]

            ; --- Show Addend 2 ("422344") then immediately recompute Sum ---
            ld      hl,1060h        ; [100f] [21 60 10]
            ld      de,1050h        ; [1012] [11 50 10]
            ld      bc,0003h        ; [1015] [01 03 00]
            ldir                    ; [1018] [ed b0]
            call    show3           ; [101a] [cd 3b 10]
            jr      start           ; [101d] [18 e1]

            ; --- Exact Book Example 4 Multi-Byte BCD Addition Routine ---
do_sum:     ld      hl,104fh        ; [101f] [21 4f 10] HL -> byte count (03h)
            ld      b,(hl)          ; [1022] [46]       B = 03h
            ld      de,1060h        ; [1023] [11 60 10] DE -> addend 2 (1060h..1062h)
            sub     a               ; [1026] [97]       clear A and Carry flag
            inc     hl              ; [1027] [23]       HL -> 1050h
sum_lp:     ld      a,(de)          ; [1028] [1a]       load byte of addend 2
            adc     a,(hl)          ; [1029] [8e]       add with carry byte of addend 1
            daa                     ; [102a] [27]       decimal adjust for BCD
            ld      (hl),a          ; [102b] [77]       store sum byte into 1050h..1052h
            inc     hl              ; [102c] [23]
            inc     de              ; [102d] [13]
            djnz    sum_lp          ; [102e] [10 f8]
            ret                     ; [1030] [c9]

init_a1:    ld      hl,addend1      ; [1031] [21 53 10]
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
            db      56h, 32h, 47h   ; [1050..1052] initial addend 1 / sum buffer (473256)
addend1:    db      56h, 32h, 47h   ; [1053..1055] backup copy of addend 1 (473256)
            defs    1060h - $, 00h
            db      44h, 23h, 42h   ; [1060..1062] addend 2 (422344)
