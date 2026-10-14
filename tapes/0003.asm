; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0003
; Annotation: Dual-Tone Chime (1000H)
; Start:      1000H
; ==============================================================================

ms1k        equ     0470h           ; Monitor ROM 1 kHz tone generator (HL = cycles)
ms2k        equ     0475h           ; Monitor ROM 2 kHz tone generator (HL = cycles)

            org     1000h

start:      ld      hl,00fah        ; [1000] [21 fa 00] 250 cycles @ 1 kHz (~0.25s)
            call    ms1k            ; [1003] [cd 70 04] play 1 kHz tone on speaker (PC7)
            ld      hl,01f4h        ; [1006] [21 f4 01] 500 cycles @ 2 kHz (~0.25s)
            call    ms2k            ; [1009] [cd 75 04] play 2 kHz tone on speaker (PC7)
            jp      start           ; [100c] [c3 00 10] repeat dual-tone chime

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
