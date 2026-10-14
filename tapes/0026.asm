; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0026
; Annotation: Lab26: X-Y Scope Pattern (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 26：顯示圖案 (pp. 173-174)
;   Streams 37 packed (Y,X) 4-bit coordinate bytes (1040h..1064h, terminated by
;   00h at 1065h) to 8255 Port B (81h) for an X-Y oscilloscope display.
; ==============================================================================

pb          equ     81h             ; 8255 Port B (upper 4b = Y DAC, lower 4b = X DAC)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
begin:      ld      hl,xytab        ; [1004] [21 40 10] HL -> coordinate table
disp:       ld      a,(hl)          ; [1007] [7e]       load (Y,X) coordinate byte
            cp      00h             ; [1008] [fe 00]    end of table (00h)?
            jp      z,begin         ; [100a] [ca 04 10] restart scan from 1040h
            out     (pb),a          ; [100d] [d3 81]    output (Y,X) to Port B DAC
            call    delay           ; [100f] [cd 16 10] beam dwell delay
            inc     hl              ; [1012] [23]
            jp      disp            ; [1013] [c3 07 10]

delay:      ld      c,0ffh          ; [1016] [0e ff]
loop:       dec     c               ; [1018] [0d]
            jp      nz,loop         ; [1019] [c2 18 10]
            ret                     ; [101c] [c9]

            defs    1040h - $, 00h
xytab:      db      92h, 82h, 72h, 62h, 0a3h, 53h, 0b4h, 44h ; [1040..1047]
            db      0c5h, 95h, 85h, 75h, 65h, 35h, 0a6h, 56h ; [1048..104f]
            db      26h, 0d7h, 27h, 0d8h, 28h, 0d9h, 0a9h, 99h ; [1050..1057]
            db      69h, 59h, 29h, 0cah, 3ah, 0bbh, 4bh, 0ach ; [1058..105f]
            db      5ch, 9dh, 8dh, 7dh, 6dh, 00h             ; [1060..1065]
