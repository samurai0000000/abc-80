; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0016
; Annotation: Lab16: Roulette Game (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 16：輪盤遊戲 (pp. 146-148)
;   Traces the 16-segment perimeter of the 6-digit display using TABLE at
;   1040h..1060h. Holding '+' (position code 03h) freezes the roulette wheel.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display scan (A = matrix pos if C=0)
outbf       equ     1100h           ; 6-byte display buffer in RAM

            org     1000h

int:        ld      hl,table        ; [1000] [21 40 10] HL -> roulette segment table
loop:       ld      ix,outbf        ; [1003] [dd 21 00 11] IX -> 1100h..1105h buffer
            call    clrbf           ; [1007] [cd 30 10] clear all 6 display digits
            ld      e,(hl)          ; [100a] [5e]       E = digit index (00h..05h, or FFh)
            inc     e               ; [100b] [1c]       test for FFh end-of-table marker
            jr      z,int           ; [100c] [28 f2]    wrap around to start of table
            dec     e               ; [100e] [1d]       restore digit index E
            ld      d,00h           ; [100f] [16 00]
            add     ix,de           ; [1011] [dd 19]    IX -> target digit in outbf
            inc     hl              ; [1013] [23]
            ld      a,(hl)          ; [1014] [7e]       A = active segment bitmask
            ld      (ix+00h),a      ; [1015] [dd 77 00] write segment mask into digit
            ld      ix,outbf        ; [1018] [dd 21 00 11] restore IX -> outbf
            ld      b,03h           ; [101c] [06 03]    3 scan passes (30 ms per step)
light:      call    scd_k1          ; [101e] [cd cd 04] multiplex display & scan keypad
            jr      c,nscan         ; [1021] [38 01]    if no key pressed (C=1), skip
            ld      c,a             ; [1023] [4f]       record pressed key position code
nscan:      djnz    light           ; [1024] [10 f8]
            ld      a,c             ; [1026] [79]
            cp      03h             ; [1027] [fe 03]    was '+' (pos code 03h) pressed?
            jr      z,stop          ; [1029] [28 02]    freeze on current segment!
            inc     hl              ; [102b] [23]       advance to next table pair
            inc     hl              ; [102c] [23]
stop:       dec     hl              ; [102d] [2b]       point back to digit index byte
            jr      loop            ; [102e] [18 d3]

clrbf:      ld      b,06h           ; [1030] [06 06]    clear 6 bytes at (IX+0..5)
crl:        ld      (ix+00h),00h    ; [1032] [dd 36 00 00]
            inc     ix              ; [1036] [dd 23]
            djnz    crl             ; [1038] [10 f8]
            ld      de,0fffah       ; [103a] [11 fa ff] DE = -6
            add     ix,de           ; [103d] [dd 19]    restore IX to start of buffer
            ret                     ; [103f] [c9]

table:      db      05h, 08h, 04h, 08h, 03h, 08h, 02h, 08h ; [1040..1047] top bar a (L->R)
            db      01h, 08h, 00h, 08h, 00h, 10h, 00h, 20h ; [1048..104f] right edge b,c
            db      00h, 80h, 01h, 80h, 02h, 80h, 03h, 80h ; [1050..1057] bottom bar d (R->L)
            db      04h, 80h, 05h, 80h, 05h, 01h, 05h, 04h ; [1058..105f] left edge e,f
            db      0ffh                                   ; [1060] end marker
