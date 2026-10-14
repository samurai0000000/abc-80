; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0100
; Annotation: BCD Up-Counter (1000H)
; Start:      1000H
; ==============================================================================

dadp        equ     0511h           ; format A into right 2 7-segment digits
scd_k1      equ     04cdh           ; multiplex 6-digit 7-segment display once
cnt_ram     equ     1030h           ; 1-byte BCD counter storage in user RAM

            org     1000h

start:      xor     a               ; [1000] [af] initialize BCD counter = 00h
loop:       ld      (cnt_ram),a     ; [1001] [32 30 10] store current BCD count
            ld      a,(cnt_ram)     ; [1004] [3a 30 10] load current BCD count
            call    dadp            ; [1007] [cd 11 05] convert A to 2 display digits
            ld      b,20h           ; [100a] [06 20] 32 scan passes (~0.32s per count)
scan:       call    scd_k1          ; [100c] [cd cd 04] refresh 6-digit display
            djnz    scan            ; [100f] [10 fb] hold display
            ld      a,(cnt_ram)     ; [1011] [3a 30 10] reload BCD count
            add     a,01h           ; [1014] [c6 01] increment by 1
            daa                     ; [1016] [27] decimal adjust for BCD (00..99)
            jr      loop            ; [1017] [18 e8] repeat indefinitely

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
