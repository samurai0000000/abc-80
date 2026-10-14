; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0010
; Annotation: Lab10: Hex & BCD Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 10：資料顯示 例二 (pp. 130-131)
;   Converts 3 bytes at 1050h..1052h (3Ah, CFh, 6Bh) via ADC A,00h + DAA,
;   formats them into 13c6h..13cbh via 2BYSEG (0525h), and displays via SCD&K.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex 6-digit display & scan keypad
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 segment bytes at (HL)++
outbf       equ     13c6h           ; 6-byte display buffer in RAM

            org     1000h

start:      ld      de,1050h        ; [1000] [11 50 10] DE -> 3-byte data table
            ld      b,03h           ; [1003] [06 03]    3 bytes to adjust
            xor     a               ; [1005] [af]       clear Carry flag
dloop:      ld      a,(de)          ; [1006] [1a]       load byte from (DE)
            adc     a,00h           ; [1007] [ce 00]    add carry from previous byte
            daa                     ; [1009] [27]       decimal adjust
            ld      (de),a          ; [100a] [12]       store adjusted byte back
            inc     de              ; [100b] [13]
            djnz    dloop           ; [100c] [10 f8]
answ3:      ld      ix,outbf        ; [100e] [dd 21 c6 13] IX -> 13c6h display buffer
            ld      de,1050h        ; [1012] [11 50 10] DE -> 1050h..1052h
            ld      hl,outbf        ; [1015] [21 c6 13] HL -> 13c6h..13cbh
            ld      b,03h           ; [1018] [06 03]    3 bytes -> 6 digits
tloop:      ld      a,(de)          ; [101a] [1a]
            call    tobyseg         ; [101b] [cd 25 05] convert A into 2 digits at (HL)
            inc     de              ; [101e] [13]
            djnz    tloop           ; [101f] [10 f9]
disp:       call    scd_k           ; [1021] [cd a6 04] multiplex 6-digit display
            jr      disp            ; [1024] [18 fb]    keep displaying if key pressed

            defs    1050h - $, 00h
            db      3ah, 0cfh, 6bh  ; [1050..1052] book initial data bytes
