; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0015
; Annotation: Lab15: Police Siren (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 15：警車鳴聲 (p. 144)
;   Alternates 265 Hz (C=00h, HL=00c0h, 0.73s) and 352 Hz (C=0c0h, HL=0100h,
;   0.73s) via Monitor ROM SOUND (047ch).
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator (C=pitch, HL=cycles)

            org     1000h

loop:       ld      c,00h           ; [1000] [0e 00] 265 Hz half-period (256 iterations)
            ld      hl,00c0h        ; [1002] [21 c0 00] 192 cycles (~0.73s)
            call    sound           ; [1005] [cd 7c 04] play low siren tone
            ld      c,0c0h          ; [1008] [0e c0] 352 Hz half-period (192 iterations)
            ld      hl,0100h        ; [100a] [21 00 01] 256 cycles (~0.73s)
            call    sound           ; [100d] [cd 7c 04] play high siren tone
            jr      loop            ; [1010] [18 ee] repeat siren loop
