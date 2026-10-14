; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0020
; Annotation: Lab20: Stopwatch (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 20：馬錶 (pp. 156-157)
;   Centisecond (1/100s) BCD stopwatch in DE displayed at 1100h..1105h as
;   "SS-CC " via SCD&K1 (04cdh) and 2BYSEG (0525h). Holding any key pauses.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display scan (C=0 if key held)
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
outbuf      equ     1100h           ; 6-byte display buffer in RAM

            org     1000h

start:      ld      ix,outbuf       ; [1000] [dd 21 00 11] IX -> 1100h..1105h
            ld      de,0000h        ; [1004] [11 00 00]    reset BCD stopwatch D=00s, E=00cs
loop:       call    scd_k1          ; [1007] [cd cd 04]    10 ms display scan & key check
            jr      nc,loop         ; [100a] [30 fb]       pause stopwatch while any key held
            ld      a,e             ; [100c] [7b]          increment centiseconds (00..99)
            add     a,01h           ; [100d] [c6 01]
            daa                     ; [100f] [27]
            ld      e,a             ; [1010] [5f]
            ld      a,d             ; [1011] [7a]          propagate carry to seconds (00..99)
            adc     a,00h           ; [1012] [ce 00]
            daa                     ; [1014] [27]
            ld      d,a             ; [1015] [57]
            ld      a,e             ; [1016] [7b]          format centiseconds E into 1100h..1101h
            ld      hl,outbuf       ; [1017] [21 00 11]
            call    tobyseg         ; [101a] [cd 25 05]
            ld      (hl),02h        ; [101d] [36 02]       1102h = '-' separator (segment g)
            inc     hl              ; [101f] [23]
            ld      a,d             ; [1020] [7a]          format seconds D into 1103h..1104h
            call    tobyseg         ; [1021] [cd 25 05]
            ld      (hl),00h        ; [1024] [36 00]       1105h = blank
            jr      loop            ; [1026] [18 df]       next 10 ms tick
