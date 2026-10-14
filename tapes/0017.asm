; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0017
; Annotation: Lab17: Pac-Man Sprite (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 17：小精靈 (pp. 149-151)
;   Animates Pac-Man chomping (EX (SP),IX between 1079h and 1089h) and scrolling
;   across the 6-digit display with 1 kHz / 2 kHz sound effects (SOU1K / SOU2K).
; ==============================================================================

ms1k        equ     0470h           ; Monitor ROM 1 kHz tone generator
ms2k        equ     0475h           ; Monitor ROM 2 kHz tone generator
scd_k1      equ     04cdh           ; 10 ms 6-digit display multiplex pass
disp2       equ     1079h           ; frame 1 initial pointer
disp4       equ     1089h           ; frame 2 initial pointer

            org     1000h

inidip:     ld      c,08h           ; [1000] [0e 08] 8 stationary chomp cycles
            ld      hl,disp4        ; [1002] [21 89 10]
            push    hl              ; [1005] [e5]    put disp4 on stack for EX (SP),IX
            ld      ix,disp2        ; [1006] [dd 21 79 10]
a1:         call    sou1k           ; [100a] [cd 52 10] 1 kHz chirp
            ex      (sp),ix         ; [100d] [dd e3]    swap mouth open/closed frame
            ld      b,0eh           ; [100f] [06 0e]    14 scan passes (0.14s)
a2:         call    scd_k1          ; [1011] [cd cd 04]
            djnz    a2              ; [1014] [10 fb]
            call    sou2k           ; [1016] [cd 5b 10] 2 kHz chirp
            ld      b,0eh           ; [1019] [06 0e]
a3:         call    scd_k1          ; [101b] [cd cd 04]
            djnz    a3              ; [101e] [10 fb]
            dec     c               ; [1020] [0d]
            jr      nz,a1           ; [1021] [20 e7]

            ld      c,12h           ; [1023] [0e 12] 18 scrolling steps
            ld      ix,disp2        ; [1025] [dd 21 79 10]
            ld      hl,disp4        ; [1029] [21 89 10]
            push    hl              ; [102c] [e5]
bloop:      ld      b,0ch           ; [102d] [06 0c] 12 scan passes (0.12s)
b2:         call    scd_k1          ; [102f] [cd cd 04]
            djnz    b2              ; [1032] [10 fb]
            dec     ix              ; [1034] [dd 2b] scroll frame 1 left
            dec     c               ; [1036] [0d]
            jr      z,c1            ; [1037] [28 10]
            ex      (sp),ix         ; [1039] [dd e3] switch to frame 2
            ld      b,0ch           ; [103b] [06 0c]
b3:         call    scd_k1          ; [103d] [cd cd 04]
            djnz    b3              ; [1040] [10 fb]
            call    sou1k           ; [1042] [cd 52 10] 1 kHz step chirp
            dec     ix              ; [1045] [dd 2b] scroll frame 2 left
            ex      (sp),ix         ; [1047] [dd e3] switch back to frame 1
c1:         dec     c               ; [1049] [0d]
            jr      nz,bloop        ; [104a] [20 e1]
            pop     hl              ; [104c] [e1]    balance stack (second push)
            pop     hl              ; [104d] [e1]    balance stack (first push)
            jp      inidip          ; [104e] [c3 00 10] repeat animation

            defs    1052h - $, 00h
sou1k:      exx                     ; [1052] [d9]
            ld      hl,0050h        ; [1053] [21 50 00] 80 cycles @ 1 kHz (80 ms)
            call    ms1k            ; [1056] [cd 70 04]
            exx                     ; [1059] [d9]
            ret                     ; [105a] [c9]

sou2k:      exx                     ; [105b] [d9]
            ld      hl,00a0h        ; [105c] [21 a0 00] 160 cycles @ 2 kHz (80 ms)
            call    ms2k            ; [105f] [cd 75 04]
            exx                     ; [1062] [d9]
            ret                     ; [1063] [c9]

            defs    1070h - $, 00h
            db      0bdh, 0bfh, 02h, 8dh, 0a7h, 3fh, 00h, 00h, 00h ; [1070..1078] "ABC-80   "
            db      0bdh, 00h, 3fh, 3fh, 3fh, 3fh, 00h             ; [1079..107f] Pac-Man open + pellets
            db      0bdh, 0bfh, 02h, 8dh, 0a7h, 3fh, 00h, 00h, 00h ; [1080..1088] "ABC-80   "
            db      38h, 00h, 1eh, 1eh, 1eh, 00h                   ; [1089..108e] Pac-Man chomp + pellets
