; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0014
; Annotation: Lab14: Telephone Ring (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 14：電話聲響 (pp. 141-143)
;   1000h (BEGIN): Telephone ring (20 bursts of 320 Hz + 480 Hz, 2s pause).
;   101fh (BUSY):  Telephone busy tone (0.5s of 400 Hz, 0.5s pause).
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator (C=pitch, HL=cycles)

            org     1000h

begin:      ld      a,14h           ; [1000] [3e 14] 20 warble bursts per ring
ring:       ex      af,af'          ; [1002] [08]    save burst counter in AF'
            ld      c,0d3h          ; [1003] [0e d3] 320 Hz half-period constant
            ld      hl,0008h        ; [1005] [21 08 00] 8 cycles @ 320 Hz (25 ms)
            call    sound           ; [1008] [cd 7c 04]
            ld      c,8ch           ; [100b] [0e 8c] 480 Hz half-period constant
            ld      hl,000ch        ; [100d] [21 0c 00] 12 cycles @ 480 Hz (25 ms)
            call    sound           ; [1010] [cd 7c 04]
            ex      af,af'          ; [1013] [08]    restore burst counter
            dec     a               ; [1014] [3d]
            jr      nz,ring         ; [1015] [20 eb] complete 1.0s ring warble
            ld      bc,3350h        ; [1017] [01 50 33] 2.0s silent interval
            call    delay           ; [101a] [cd 2f 10]
            jr      begin           ; [101d] [18 e1] repeat telephone ring

busy:       ld      c,0a8h          ; [101f] [0e a8] 400 Hz half-period constant
            ld      hl,00c8h        ; [1021] [21 c8 00] 200 cycles @ 400 Hz (0.5s)
            call    sound           ; [1024] [cd 7c 04]
            ld      bc,30d4h        ; [1027] [01 d4 30] 0.5s silent interval
            call    delay           ; [102a] [cd 2f 10]
            jr      busy            ; [102d] [18 f0] repeat busy tone

delay:      ex      (sp),hl         ; [102f] [e3]    19 T-states delay padding
            ex      (sp),hl         ; [1030] [e3]    19 T-states delay padding
            cpi                     ; [1031] [ed a1] decrement BC (P/V=0 when BC==0)
            ret     po              ; [1033] [e0]    return when BC reaches 0
            jr      delay           ; [1034] [18 f9]
