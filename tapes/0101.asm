; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0101
; Annotation: Segment Chaser (1000H)
; Start:      1000H
; ==============================================================================

segment     equ     81h             ; 8255 Port B: 7-segment LED segment drivers
digit       equ     82h             ; 8255 Port C: display digit cathode strobes

            org     1000h

start:      ld      a,0feh          ; [1000] [3e fe] initial active-low segment bitmask
loop:       out     (segment),a     ; [1002] [d3 81] drive 7-segment bus (Port B)
            ld      b,a             ; [1004] [47] save current segment bitmask in B
            ld      a,0c0h          ; [1005] [3e c0] enable all 6 digit cathodes (PC0..PC5 low)
            out     (digit),a       ; [1007] [d3 82] drive digit select lines (Port C)
            ld      hl,3000h        ; [1009] [21 00 30] load software delay counter
delay:      dec     hl              ; [100c] [2b] decrement delay counter
            ld      a,h             ; [100d] [7c] test if HL reached 0000h
            or      l               ; [100e] [b5]
            jr      nz,delay        ; [100f] [20 fb] wait until delay completes
            ld      a,b             ; [1011] [78] restore segment bitmask
            rlc     a               ; [1012] [cb 07] rotate active segment bit left
            jr      loop            ; [1014] [18 ec] repeat chaser loop

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
