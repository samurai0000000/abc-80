; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0002
; Annotation: Lab02: Logic AND & OR (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 2：邏輯指令練習 (一) (pp. 103-107)
;   Ex 3 (AND): 63H&F0H=60H (1050H), 47H&0FH=07H (1051H), 5FH&F9H=59H (1052H) -> 590760
;   Ex 4 (OR):  63H|F0H=F3H (1050H), 47H|0FH=4FH (1051H), 5FH|F9H=FFH (1052H) -> FF4FF3
; Enhancement: Alternates 590760 (AND) and FF4FF3 (OR) on all 6 7-segment LEDs
;   using CLKSET (06C3H) and SCD&K1 (04CDH) with 3 kHz speaker clicks (0464H).
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Example 3: 3-Byte Bitwise AND (59 07 60) ---
            ld      b,63h           ; [1000] [06 63] B = 63h
            ld      a,0f0h          ; [1002] [3e f0] A = F0h
            and     b               ; [1004] [a0]    A = 63h & F0h = 60h
            ld      (1050h),a       ; [1005] [32 50 10]
            ld      c,47h           ; [1008] [0e 47] C = 47h
            ld      a,0fh           ; [100a] [3e 0f] A = 0Fh
            and     c               ; [100c] [a1]    A = 47h & 0Fh = 07h
            ld      (1051h),a       ; [100d] [32 51 10]
            ld      d,5fh           ; [1010] [16 5f] D = 5Fh
            ld      a,0f9h          ; [1012] [3e f9] A = F9h
            and     d               ; [1014] [a2]    A = 5Fh & F9h = 59h
            ld      (1052h),a       ; [1015] [32 52 10]
            call    show3           ; [1018] [cd 38 10] display "590760" + click

            ; --- Book Example 4: 3-Byte Bitwise OR (FF 4F F3) ---
            ld      b,63h           ; [101b] [06 63] B = 63h
            ld      a,0f0h          ; [101d] [3e f0] A = F0h
            or      b               ; [101f] [b0]    A = 63h | F0h = F3h
            ld      (1050h),a       ; [1020] [32 50 10]
            ld      c,47h           ; [1023] [0e 47] C = 47h
            ld      a,0fh           ; [1025] [3e 0f] A = 0Fh
            or      c               ; [1027] [b1]    A = 47h | 0Fh = 4Fh
            ld      (1051h),a       ; [1028] [32 51 10]
            ld      d,5fh           ; [102b] [16 5f] D = 5Fh
            ld      a,0f9h          ; [102d] [3e f9] A = F9h
            or      d               ; [102f] [b2]    A = 5Fh | F9h = FFh
            ld      (1052h),a       ; [1030] [32 52 10]
            call    show3           ; [1033] [cd 38 10] display "FF4FF3" + click
            jr      start           ; [1036] [18 c8] repeat loop

show3:      ld      b,03h           ; [1038] [06 03] format 3 bytes at 1050h..1052h
            call    clkset          ; [103a] [cd c3 06] populate dispbf (17c6h..17cbh)
            call    ms3k17          ; [103d] [cd 64 04] 3 kHz speaker click
            ld      b,32h           ; [1040] [06 32] 50 scan passes (~0.50s)
scan:       call    scd_k1          ; [1042] [cd cd 04] multiplex 6-digit display
            djnz    scan            ; [1045] [10 fb]
            ret                     ; [1047] [c9]

            defs    1050h - $, 00h
            db      00h, 00h, 00h   ; [1050..1052] 3-byte logic result buffer
