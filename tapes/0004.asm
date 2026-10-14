; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0004
; Annotation: Fibonacci Display (1000H)
; Start:      1000H
; ==============================================================================

ms3k17      equ     0464h           ; Monitor ROM 3 kHz key-click tone (preserves regs)
scd_k1      equ     04cdh           ; Monitor ROM 6-digit display multiplexer pass
adrsdp      equ     0504h           ; Monitor ROM format DE into left 4 address digits
dadp        equ     0511h           ; Monitor ROM format A into right 2 data digits
fib_tab     equ     1040h           ; Destination table in user RAM (1040h..104ah)

            org     1000h

start:      ld      de,fib_tab      ; [1000] [11 40 10] DE = table pointer (1040h..104ah)
            ld      bc,0001h        ; [1003] [01 01 00] B = F(n-1) = 00h, C = F(n) = 01h
term:       ld      a,c             ; [1006] [79] A = current BCD Fibonacci term
            ld      (de),a          ; [1007] [12] store term into RAM table at (DE)
            call    dadp            ; [1008] [cd 11 05] format A onto right 2 data digits
            call    adrsdp          ; [100b] [cd 04 05] format DE onto left 4 address digits
            call    ms3k17          ; [100e] [cd 64 04] emit 3 kHz speaker tick
            push    bc              ; [1011] [c5] preserve Fibonacci pair (B, C)
            ld      b,28h           ; [1012] [06 28] 40 display scan passes (~0.40s)
scan:       call    scd_k1          ; [1014] [cd cd 04] multiplex 6-digit 7-segment display
            djnz    scan            ; [1017] [10 fb] hold display
            pop     bc              ; [1019] [c1] restore Fibonacci pair (B, C)
            ld      a,c             ; [101a] [79] A = F(n)
            add     a,b             ; [101b] [80] A = F(n) + F(n-1)
            daa                     ; [101c] [27] BCD adjust (01,01,02,03,05,08,13,21,34,55,89)
            jr      c,start         ; [101d] [38 e1] restart at 1000h when 2-digit BCD overflows
            ld      b,c             ; [101f] [41] B = old F(n)
            ld      c,a             ; [1020] [4f] C = new F(n+1)
            inc     de              ; [1021] [13] advance RAM table address
            jr      term            ; [1022] [18 e2] compute and display next term

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
