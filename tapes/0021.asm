; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0021
; Annotation: Lab21: 24-Hour Clock (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 21：時鐘 (pp. 158-163)
;   24-hour BCD clock: time in 1200h..1202h (SS,MM,HH), display buffer at
;   1203h..1208h, modulo table (60h,60h,24h) at 1051h..1053h, and hourly chime
;   subroutine at 1100h..1108h.
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator
scd_k1      equ     04cdh           ; 10 ms display multiplex pass
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
timebf      equ     1200h           ; 3-byte BCD time (1200h=SS, 1201h=MM, 1202h=HH)
outbf       equ     1203h           ; 6-byte display buffer (1203h..1208h)

            org     1000h

start:      di                      ; [1000] [f3]          disable interrupts
            ld      ix,outbf        ; [1001] [dd 21 03 12] IX -> 1203h display buffer
clock:      ld      b,64h           ; [1005] [06 64]       100 * 10 ms = 1.0 second
loop1:      call    scd_k1          ; [1007] [cd cd 04]    refresh 6-digit display
            djnz    loop1           ; [100a] [10 fb]
            call    tmupd           ; [100c] [cd 17 10]    increment SS:MM:HH by 1s
            call    bfupd           ; [100f] [cd 39 10]    format 1200h..1202h into 1203h..1208h
loop2:      nop                     ; [1012] [00]          4347 T-state fine timing trim
            djnz    loop2           ; [1013] [10 fd]
            jr      clock           ; [1015] [18 ee]

tmupd:      ld      hl,modtab       ; [1017] [21 51 10]    HL -> modulo limits (60h,60h,24h)
            ld      de,timebf       ; [101a] [11 00 12]    DE -> 1200h (SS,MM,HH)
            ld      b,03h           ; [101d] [06 03]       3 units (sec, min, hr)
            scf                     ; [101f] [37]          set carry to add 1 second
tminc:      ld      a,(de)          ; [1020] [1a]
            adc     a,00h           ; [1021] [ce 00]
            daa                     ; [1023] [27]
            ld      (de),a          ; [1024] [12]
            sub     (hl)            ; [1025] [96]          compare against modulo limit (60h/24h)
            jr      c,compl         ; [1026] [38 0b]       if < limit, no carry-out
            ld      (de),a          ; [1028] [12]          reset unit to 00h on rollover
            ex      af,af'          ; [1029] [08]
            ld      a,01h           ; [102a] [3e 01]
            cp      b               ; [102c] [b8]          did minutes roll over into hours?
            exx                     ; [102d] [d9]
            call    z,chime         ; [102e] [cc 00 11]    sound hourly chime!
            ex      af,af'          ; [1031] [08]
            exx                     ; [1032] [d9]
compl:      ccf                     ; [1033] [3f]          invert borrow into carry for next unit
            inc     hl              ; [1034] [23]
            inc     de              ; [1035] [13]
            djnz    tminc           ; [1036] [10 e8]
            ret                     ; [1038] [c9]

bfupd:      ld      hl,outbf        ; [1039] [21 03 12]    HL -> 1203h..1208h
            ld      de,timebf       ; [103c] [11 00 12]    DE -> 1200h..1202h
            ld      b,03h           ; [103f] [06 03]
putbf:      ld      a,(de)          ; [1041] [1a]
            call    tobyseg         ; [1042] [cd 25 05]
            inc     de              ; [1045] [13]
            djnz    putbf           ; [1046] [10 f9]
            dec     hl              ; [1048] [2b]
            dec     hl              ; [1049] [2b]
            set     6,(hl)          ; [104a] [cb f6]       turn on DP on hours low digit (1207h)
            dec     hl              ; [104c] [2b]
            dec     hl              ; [104d] [2b]
            set     6,(hl)          ; [104e] [cb f6]       turn on DP on minutes low digit (1205h)
            ret                     ; [1050] [c9]

modtab:     db      60h, 60h, 24h   ; [1051..1053] modulo limits: 60s, 60m, 24h

            defs    1100h - $, 00h
chime:      ld      c,2fh           ; [1100] [0e 2f]       hourly chime pitch
            ld      hl,02f0h        ; [1102] [21 f0 02]    hourly chime duration
            call    sound           ; [1105] [cd 7c 04]
            ret                     ; [1108] [c9]
