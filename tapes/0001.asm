; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0001
; Annotation: Lab01: Data Transfer (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 1：資料傳送練習 (pp. 99-102)
;   Ex 1: 8-bit transfer 01H..05H -> 1050H..1054H
;   Ex 2: 16-bit transfer BC=1234H, DE=5678H, HL=1050H -> 1060H..1065H
; Enhancement: Cycles through 1050H..1054H and 1060H..1065H on the 6-digit
;   7-segment display (ADRSDP + DADP + SCD&K1) with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click (preserves regs)
scd_k1      equ     04cdh           ; 6-digit display multiplex pass (10 ms)
adrsdp      equ     0504h           ; DE -> left 4 address digits (17c8h..17cbh)
dadp        equ     0511h           ; A  -> right 2 data digits (17c6h..17c7h)
dispbf      equ     17c6h           ; system 6-digit display buffer

            org     1000h

start:      ; --- Book Example 1: 8-bit register & indirect RAM transfer ---
            ld      a,01h           ; [1000] [3e 01] A = 01h
            ld      b,02h           ; [1002] [06 02] B = 02h
            ld      c,03h           ; [1004] [0e 03] C = 03h
            ld      d,04h           ; [1006] [16 04] D = 04h
            ld      e,05h           ; [1008] [1e 05] E = 05h
            ld      hl,1050h        ; [100a] [21 50 10] HL -> 1050h
            ld      (hl),a          ; [100d] [77] (1050h) = 01h
            inc     hl              ; [100e] [23]
            ld      (hl),b          ; [100f] [70] (1051h) = 02h
            inc     hl              ; [1010] [23]
            ld      (hl),c          ; [1011] [71] (1052h) = 03h
            inc     hl              ; [1012] [23]
            ld      (hl),d          ; [1013] [72] (1053h) = 04h
            inc     hl              ; [1014] [23]
            ld      (hl),e          ; [1015] [73] (1054h) = 05h

            ; --- Book Example 2: 16-bit extended RAM transfer ---
            ld      bc,1234h        ; [1016] [01 34 12] BC = 1234h
            ld      de,5678h        ; [1019] [11 78 56] DE = 5678h
            ld      hl,1050h        ; [101c] [21 50 10] HL = 1050h
            ld      (1060h),bc      ; [101f] [ed 43 60 10] (1060h..1061h) = 34h, 12h
            ld      (1062h),de      ; [1023] [ed 53 62 10] (1062h..1063h) = 78h, 56h
            ld      (1064h),hl      ; [1027] [22 64 10] (1064h..1065h) = 50h, 10h

            ; --- Lively 7-Segment & Speaker Inspection Loop ---
            ld      ix,dispbf       ; [102a] [dd 21 c6 17] IX -> system display buffer
            ld      de,1050h        ; [102e] [11 50 10] sweep Ex 1 (1050h..1054h)
sweep:      ld      a,(de)          ; [1031] [1a] load transferred byte at (DE)
            call    dadp            ; [1032] [cd 11 05] format byte onto right 2 digits
            call    adrsdp          ; [1035] [cd 04 05] format DE onto left 4 digits
            call    ms3k17          ; [1038] [cd 64 04] emit 3 kHz speaker click
            ld      b,20h           ; [103b] [06 20] hold for 32 scan passes (~0.32s)
scan:       call    scd_k1          ; [103d] [cd cd 04] refresh 6-digit display
            djnz    scan            ; [1040] [10 fb]
            inc     e               ; [1042] [1c] next RAM address
            ld      a,e             ; [1043] [7b]
            cp      55h             ; [1044] [fe 55] finished 1050h..1054h?
            jr      nz,chk66        ; [1046] [20 02]
            ld      e,60h           ; [1048] [1e 60] advance DE to 1060h..1065h
chk66:      cp      66h             ; [104a] [fe 66] finished 1060h..1065h?
            jr      nz,sweep        ; [104c] [20 e3]
            jr      start           ; [104e] [18 b0] repeat indefinitely

            defs    1056h - $, 00h  ; [1050..1055] textbook Ex 1 destination buffer
