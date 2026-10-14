; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0025
; Annotation: Lab25: LED Chaser (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 25：燈光順序控制 (p. 172)
;   Rotates a single active-low LED bit (FEh -> FDh -> ... -> 7Fh) across 8255
;   Port B (81h) using RLCA and a 65536-count 16-bit ADC HL,DE delay loop.
; ==============================================================================

pb          equ     81h             ; 8255 Port B (8-LED chaser output)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
            ld      a,0feh          ; [1004] [3e fe] initial active-low bitmask (PB0 low)
next:       out     (pb),a          ; [1006] [d3 81] output pattern to Port B
            ld      b,a             ; [1008] [47]    save bitmask in B
            ld      hl,0000h        ; [1009] [21 00 00]
            ld      de,0001h        ; [100c] [11 01 00]
loop:       adc     hl,de           ; [100f] [ed 5a] increment HL until 16-bit overflow to 0000h
            jp      nz,loop         ; [1011] [c2 0f 10]
            ld      a,b             ; [1014] [78]    restore bitmask
            rlca                    ; [1015] [07]    rotate active-low bit left
            jp      next            ; [1016] [c3 06 10]
