; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0022
; Annotation: Lab22: 1kHz Signal Gen (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 22：信號產生器 (pp. 164-165)
;   Outputs a continuous 1 kHz square-wave test signal on 8255 Port C (82h)
;   bit 7 (C = 42h -> 1 kHz, 1Fh -> 2 kHz, 14h -> 3 kHz).
; ==============================================================================

digit       equ     82h             ; 8255 Port C (bit 7 speaker / MIC out)

            org     1000h

begin:      ld      hl,0064h        ; [1000] [21 64 00] 100 full cycles per burst
start:      ld      c,42h           ; [1003] [0e 42]    1 kHz half-period constant
sound:      add     hl,hl           ; [1005] [29]       HL * 2 = 200 half-cycles
            ld      de,0001h        ; [1006] [11 01 00]
            ld      a,0ffh          ; [1009] [3e ff]
sqwave:     out     (digit),a       ; [100b] [d3 82]    drive Port C bit 7
            ld      b,c             ; [100d] [41]       load half-period delay
            djnz    $               ; [100e] [10 fe]    spin B*13 - 5 T-states
            xor     80h             ; [1010] [ee 80]    toggle bit 7
            sbc     hl,de           ; [1012] [ed 52]    decrement half-cycle counter
            jr      nz,sqwave       ; [1014] [20 f5]
            jr      begin           ; [1016] [18 e8]    repeat continuously
