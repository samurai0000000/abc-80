; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0008
; Annotation: Lab08: Shift & Rotate (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 8：移位與旋轉練習 (pp. 122-124)
;   Ex 1: LD A,(1051H) [6Fh] -> SLA A [CB 27] -> LD (1050H),A [DEh]
;   Ex 2: LD A,3Fh -> RRCA [0Fh] -> 9Fh (stored at 1052H)
; Enhancement: Displays "9F6FDE" ([RRCA=9F][IN=6F][SLA=DE]) via CLKSET, then
;   animates 8 live RRCA rotation steps on the 6-digit display with speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Example 1: SLA A on (1051h) = 6Fh -> (1050h) = DEh ---
            ld      a,(1051h)       ; [1000] [3a 51 10] A = 6Fh
            sla     a               ; [1003] [cb 27]    A = 6Fh << 1 = DEh
            ld      (1050h),a       ; [1005] [32 50 10] (1050h) = DEh

            ; --- Book Example 2: RRCA on 3Fh -> (1052h) = 9Fh ---
            ld      a,3fh           ; [1008] [3e 3f]    A = 3Fh
            rrca                    ; [100a] [0f]       A = 9Fh
            ld      (1052h),a       ; [100b] [32 52 10] (1052h) = 9Fh

            ; --- Phase 1: Display 6-digit summary "9F6FDE" ---
            ld      b,03h           ; [100e] [06 03]
            call    clkset          ; [1010] [cd c3 06] dispbf <- [1052][1051][1050]
            call    ms3k17          ; [1013] [cd 64 04]
            ld      b,32h           ; [1016] [06 32]    hold 50 passes (~0.50s)
hold1:      call    scd_k1          ; [1018] [cd cd 04]
            djnz    hold1           ; [101b] [10 fb]

            ; --- Phase 2: Animate 8 live RRCA steps of 3Fh ---
            ld      de,013fh        ; [101d] [11 3f 01] D = step 01..08, E = seed 3Fh
            ld      c,3fh           ; [1020] [0e 3f]    C = rotating byte
rot_lp:     ld      a,c             ; [1022] [79]
            rrca                    ; [1023] [0f]       rotate right 1 bit
            ld      c,a             ; [1024] [4f]
            call    dadp            ; [1025] [cd 11 05] right 2 digits = rotated byte
            call    adrsdp          ; [1028] [cd 04 05] left 4 digits = [step][3F]
            call    ms3k17          ; [102b] [cd 64 04]
            ld      b,14h           ; [102e] [06 14]    20 passes (~0.20s per bit step)
scan:       call    scd_k1          ; [1030] [cd cd 04]
            djnz    scan            ; [1033] [10 fb]
            inc     d               ; [1035] [14]       increment step counter
            ld      a,d             ; [1036] [7a]
            cp      09h             ; [1037] [fe 09]
            jr      nz,rot_lp       ; [1039] [20 e7]
            jr      start           ; [103b] [18 c3]

            defs    1050h - $, 00h
            db      00h             ; [1050] SLA result (becomes DEh)
            db      6fh             ; [1051] book input operand = 6Fh
            db      00h             ; [1052] RRCA result (becomes 9Fh)
