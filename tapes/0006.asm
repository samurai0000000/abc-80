; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0006
; Annotation: Lab06: Loop & Delay (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 6：迴轉及跳越練習 例二 (pp. 118-119)
;   Nested B/C delay loop toggling 8255 Port C (82H) bit 7 (XOR 80H).
; ==============================================================================

port_c      equ     82h             ; 8255 Port C (digit strobes & PC7 speaker)

            org     1000h

            ld      a,0ffh          ; [1000] [3e ff] initial Port C output state
sqwav:      out     (port_c),a      ; [1002] [d3 82] output A to 8255 Port C
            ld      b,0ffh          ; [1004] [06 ff] outer loop count = 255
delay:      ld      c,0ffh          ; [1006] [0e ff] inner loop count = 255
loop:       dec     c               ; [1008] [0d]    decrement inner counter
            jr      nz,loop         ; [1009] [20 fd] loop until C == 0
            djnz    delay           ; [100b] [10 f9] loop until B == 0
            xor     80h             ; [100d] [ee 80] invert bit 7 (speaker / pulse output)
            jp      sqwav           ; [100f] [c3 02 10] repeat square-wave loop
