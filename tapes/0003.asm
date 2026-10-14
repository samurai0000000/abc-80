; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0003
; Annotation: Lab03: XOR CPL & NEG (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 3：邏輯指令練習 (二) (pp. 108-110)
;   Ex 1 (XOR): 58H XOR 58H = 00H (1054H), 39H XOR 36H = 0FH (1050H)
;   Ex 3 (CPL): CPL(5BH) = A4H (1's complement -> 1051H)
;   Ex 4 (NEG): NEG(5BH) = A5H (2's complement -> 1052H)
; Enhancement: Cycles 6-digit summary "A5A40F" and individual operand/result
;   pairs ("3936 0F", "005B A4", "005B A5") with 3 kHz speaker clicks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Ex 1: XOR ---
            ld      a,58h           ; [1000] [3e 58]
            xor     a               ; [1002] [af]    58h XOR 58h = 00h
            ld      (1054h),a       ; [1003] [32 54 10]
            ld      a,39h           ; [1006] [3e 39]
            ld      b,36h           ; [1008] [06 36]
            xor     b               ; [100a] [a8]    39h XOR 36h = 0Fh
            ld      (1050h),a       ; [100b] [32 50 10]

            ; --- Book Ex 3: CPL (1's complement of 5Bh = A4h) ---
            ld      a,(1053h)       ; [100e] [3a 53 10] A = 5Bh
            cpl                     ; [1011] [2f]       A = ~5Bh = A4h
            ld      (1051h),a       ; [1012] [32 51 10]

            ; --- Book Ex 4: NEG (2's complement of 5Bh = A5h) ---
            ld      a,(1053h)       ; [1015] [3a 53 10] A = 5Bh
            neg                     ; [1018] [ed 44]    A = -5Bh = A5h
            ld      (1052h),a       ; [101a] [32 52 10]

            ; --- Phase 1: Display all 3 results "A5A40F" ---
            ld      b,03h           ; [101d] [06 03]
            call    clkset          ; [101f] [cd c3 06] dispbf <- [1052][1051][1050]
            call    hold            ; [1022] [cd 42 10]

            ; --- Phase 2: Display XOR "3936 0F" ---
            ld      de,3936h        ; [1025] [11 36 39]
            ld      a,(1050h)       ; [1028] [3a 50 10]
            call    showpair        ; [102b] [cd 3c 10]

            ; --- Phase 3: Display CPL "005B A4" ---
            ld      de,005bh        ; [102e] [11 5b 00]
            ld      a,(1051h)       ; [1031] [3a 51 10]
            call    showpair        ; [1034] [cd 3f 10]

            ; --- Phase 4: Display NEG "005B A5" (DE is still 005Bh) ---
            ld      a,(1052h)       ; [1037] [3a 52 10]
            call    showpair        ; [103a] [cd 3f 10]
            jr      start           ; [103d] [18 c1]

showpair:   call    dadp            ; [103f] [cd 11 05]
            call    adrsdp          ; [1042] [cd 04 05]
hold:       call    ms3k17          ; [1045] [cd 64 04]
            ld      b,28h           ; [1048] [06 28] 40 scan passes (~0.40s)
scan:       call    scd_k1          ; [104a] [cd cd 04]
            djnz    scan            ; [104d] [10 fb]
            ret                     ; [104f] [c9]

            defs    1053h - $, 00h
operand:    db      5bh             ; [1053] [5b] book input byte for CPL/NEG
