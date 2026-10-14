; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0009
; Annotation: Lab09: Subroutines HELLO (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 9：叫用副程式 例四 (pp. 128-129)
;   Displays " HELLO" (1040h..1045h). Pressing '-' (11h) blanks the display
;   (1046h..104bh); pressing '+' (10h) scrolls " HELLO" from right to left.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex display & wait for debounced key
scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

start:      ld      ix,hello        ; [1000] [dd 21 40 10] IX -> " HELLO" buffer
disp1:      call    scd_k           ; [1004] [cd a6 04]    display " HELLO" & read key
            cp      11h             ; [1007] [fe 11]       is '-' key (11h) pressed?
            jr      nz,disp1        ; [1009] [20 f9]       loop until '-' pressed
            ld      ix,blank        ; [100b] [dd 21 46 10] IX -> blank buffer
disp2:      call    scd_k           ; [100f] [cd a6 04]    display blank & read key
            cp      10h             ; [1012] [fe 10]       is '+' key (10h) pressed?
            jr      nz,disp2        ; [1014] [20 f9]       loop until '+' pressed
            ld      ix,blank        ; [1016] [dd 21 46 10] start scroll from blank
            ld      c,06h           ; [101a] [0e 06]       6 shift steps
ini1:       ld      b,20h           ; [101c] [06 20]       32 scan passes (~0.32s/step)
ini2:       call    scd_k1          ; [101e] [cd cd 04]    refresh 6-digit display
            djnz    ini2            ; [1021] [10 fb]
            dec     ix              ; [1023] [dd 2b]       shift window 1 digit left
            dec     c               ; [1025] [0d]
            jr      nz,ini1         ; [1026] [20 f4]
            jr      start           ; [1028] [18 d6]       repeat from start

            defs    1040h - $, 00h
hello:      db      0bdh, 85h, 85h, 8fh, 37h, 00h ; [1040..1045] " HELLO" (O,L,L,E,H,' ')
blank:      db      00h, 00h, 00h, 00h, 00h, 00h  ; [1046..104b] 6 blank digits
