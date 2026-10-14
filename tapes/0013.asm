; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0013
; Annotation: Lab13: Marquee Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 13：移動顯示 例三 (pp. 138-140)
;   Scrolls all 26 alphabet characters (A..Z at 1046h..105fh, framed by 6 blanks
;   at 1040h..1045h and 1060h..1065h) from right to left across the 6-digit LEDs.
; ==============================================================================

scd_k1      equ     04cdh           ; single 10 ms display multiplex pass
blank       equ     105fh           ; initial window pointer (blank tail)

            org     1000h

start:      ld      ix,blank        ; [1000] [dd 21 5f 10] IX -> start of marquee scroll
            ld      c,20h           ; [1004] [0e 20]       32 shift steps
loop1:      ld      b,60h           ; [1006] [06 60]       96 scan passes (~0.96s/step)
loop2:      call    scd_k1          ; [1008] [cd cd 04]    refresh 6-digit display
            djnz    loop2           ; [100b] [10 fb]
            dec     ix              ; [100d] [dd 2b]       shift window 1 character left
            dec     c               ; [100f] [0d]
            jr      nz,loop1        ; [1010] [20 f4]
            jp      start           ; [1012] [c3 00 10]    repeat marquee continuously

            defs    1040h - $, 00h
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [1040..1045] leading blanks
            db      8ah, 0b6h, 07h, 0a9h, 0b7h, 0b5h       ; [1046..104b] Z..U
            db      87h, 0aeh, 03h, 3eh, 1fh, 0a3h         ; [104c..1051] T..O
            db      23h, 2bh, 85h, 97h, 0b1h, 89h          ; [1052..1057] N..I
            db      37h, 0adh, 0fh, 8fh, 0b3h, 8dh         ; [1058..105d] H..C
            db      0a7h, 3fh                              ; [105e..105f] B, A
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [1060..1065] trailing blanks
