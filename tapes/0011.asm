; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0011
; Annotation: Lab11: FEDCBA Scroll (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 11：副程式練習 例二 (pp. 134-135)
;   Scrolls "FEDCBA" (1040h..1045h) from right to left across the 6-digit
;   display (0.32s per step via SCD&K1), then holds on SCD&K until a key press.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex display & wait for key press
scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

start:      ld      ix,blank        ; [1000] [dd 21 46 10] IX -> blank buffer (1046h)
            ld      c,06h           ; [1004] [0e 06]       6 shift steps
ini1:       ld      b,20h           ; [1006] [06 20]       32 passes (~0.32s per step)
ini2:       call    scd_k1          ; [1008] [cd cd 04]    refresh 6-digit display
            djnz    ini2            ; [100b] [10 fb]
            dec     ix              ; [100d] [dd 2b]       shift window 1 byte toward 1040h
            dec     c               ; [100f] [0d]
            jr      nz,ini1         ; [1010] [20 f4]
            call    scd_k           ; [1012] [cd a6 04]    hold "FEDCBA" until key pressed
            jr      start           ; [1015] [18 e9]       replay scroll on key press

            defs    1040h - $, 00h
fedcba:     db      0fh, 8fh, 0b3h, 8dh, 0a7h, 3fh ; [1040..1045] "FEDCBA" (A,B,C,d,E,F)
blank:      db      00h, 00h, 00h, 00h, 00h, 00h   ; [1046..104b] 6 blank digits
