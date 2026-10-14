; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0023
; Annotation: Lab23: Pulse Counter (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 23：計數器 (pp. 165-167)
;   6-digit BCD counter (000000..999999) with main program (1000h) and 4
;   subroutines: INPUT (1050h), ADDS (1080h), DATAB (10d0h), CLEAR (1100h).
;   Uses the book p. 167 auto-increment target (105dh = 89h -> JP C,1089h) so
;   the counter visibly increments when EAR (PA7) is idle high.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display multiplex pass
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
port_a      equ     80h             ; 8255 Port A (bit 7 = EAR input)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

main:       ld      a,90h           ; [1000] [3e 90] PA=in, PB=out, PC=out
            out     (cwr),a         ; [1002] [d3 83]
            ld      ix,13c6h        ; [1004] [dd 21 c6 13] IX -> 13c6h display buffer
            call    clear           ; [1008] [cd 00 11]    zero 13c6h..13d1h
            call    datab           ; [100b] [cd d0 10]    format initial "000000"
start:      ld      b,05h           ; [100e] [06 05]       5 * 10 ms = 50 ms display scan
scdk:       call    scd_k1          ; [1010] [cd cd 04]
            djnz    scdk            ; [1013] [10 fb]
            call    input           ; [1015] [cd 50 10]    sample input & update counter
            jp      start           ; [1018] [c3 0e 10]

            ; --- Subroutine 1: INPUT (1050h) ---
            defs    1050h - $, 00h
input:      in      a,(port_a)      ; [1050] [db 80] sample Port A
            ld      c,a             ; [1052] [4f]
            ld      b,00h           ; [1053] [06 00] debounce delay (256 loops)
            djnz    $               ; [1055] [10 fe]
            in      a,(port_a)      ; [1057] [db 80] sample Port A again
            and     c               ; [1059] [a1]    debounce AND
            rlc     a               ; [105a] [cb 07] shift bit 7 into Carry
            jp      c,add_go        ; [105c] [da 89 10] book p.167: 1089h=auto-inc, 1080h=EAR edge
            ld      hl,13cfh        ; [105f] [21 cf 13] clear edge latch when PA7 low
            ld      (hl),00h        ; [1062] [36 00]
ret1:       ret                     ; [1064] [c9]

            ; --- Subroutine 2: ADDS (1080h) ---
            defs    1080h - $, 00h
adds:       ld      hl,13cfh        ; [1080] [21 cf 13] check edge latch (13cfh)
            ld      a,(hl)          ; [1083] [7e]
            rla                     ; [1084] [17]       (or CB 17 RL A)
            nop                     ; [1085] [00]       align to book address 1086h
            jp      c,ret1          ; [1086] [da 64 10] already counted this pulse -> return
add_go:     ld      de,13cch        ; [1089] [11 cc 13] DE -> 3-byte BCD counter (13cch..13ceh)
            ld      b,03h           ; [108c] [06 03]
            scf                     ; [108e] [37]       set carry to add 1
addl:       ld      a,(de)          ; [108f] [1a]
            adc     a,00h           ; [1090] [ce 00]
            daa                     ; [1092] [27]
            ld      (de),a          ; [1093] [12]
            inc     de              ; [1094] [13]
            djnz    addl            ; [1095] [10 f8]
            ld      a,80h           ; [1097] [3e 80]    set edge latch bit 7 at 13cfh
            ld      (de),a          ; [1099] [12]
            call    datab           ; [109a] [cd d0 10] update 6-digit display buffer
            ret                     ; [109d] [c9]

            ; --- Subroutine 3: DATAB (10d0h) ---
            defs    10d0h - $, 00h
datab:      ld      de,13cch        ; [10d0] [11 cc 13] DE -> 3-byte BCD counter
            ld      hl,13c6h        ; [10d3] [21 c6 13] HL -> 6-byte display buffer
            ld      b,03h           ; [10d6] [06 03]
bloop:      ld      a,(de)          ; [10d8] [1a]
            call    tobyseg         ; [10d9] [cd 25 05]
            inc     de              ; [10dc] [13]
            djnz    bloop           ; [10dd] [10 f9]
            ret                     ; [10df] [c9]

            ; --- Subroutine 4: CLEAR (1100h) ---
            defs    1100h - $, 00h
clear:      ld      b,0ch           ; [1100] [06 0c]    clear 12 bytes (13c6h..13d1h)
            xor     a               ; [1102] [af]
            ld      hl,13c5h        ; [1103] [21 c5 13]
cloop:      inc     hl              ; [1106] [23]
            ld      (hl),a          ; [1107] [77]
            djnz    cloop           ; [1108] [10 fc]
            ret                     ; [110a] [c9]
