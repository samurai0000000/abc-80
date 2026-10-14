; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0028
; Annotation: Lab28: Appliance Timer (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 28：家庭電器電腦化控制 (pp. 179-183)
;   24-hour BCD clock (1200h..1202h = SS,MM,HH; 1203h..1208h = display buffer)
;   driving 8 household appliance relays on Auxiliary 8255 Port C (42h) via
;   74LS139 pin 11 (ports 40h..43h). On every hour (SS=00, MM=00), ESAVE
;   (106fh) indexes into the BCD-hour table at 1100h..1124h and outputs the
;   8-channel relay bitmask to Port 42h.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display multiplex pass
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
aux_pc      equ     42h             ; Aux 8255 Port C (8 appliance relay channels)
aux_cwr     equ     43h             ; Aux 8255 Control Word Register
timebf      equ     1200h           ; 3-byte BCD time (1200h=SS, 1201h=MM, 1202h=HH)
outbf       equ     1203h           ; 6-byte display buffer (1203h..1208h)

            org     1000h

            ld      a,80h           ; [1000] [3e 80] Aux 8255 Mode 0 all outputs
            out     (aux_cwr),a     ; [1002] [d3 43]
            sla     a               ; [1004] [cb 27] A = 00h (all relays initially OFF)
            out     (aux_pc),a      ; [1006] [d3 42]
            ld      ix,outbf        ; [1008] [dd 21 03 12] IX -> 1203h display buffer
clock:      ld      b,64h           ; [100c] [06 64]       100 * 10 ms = 1.0s
loop1:      call    scd_k1          ; [100e] [cd cd 04]
            djnz    loop1           ; [1011] [10 fb]
            call    tmupd           ; [1013] [cd 27 10]    increment SS:MM:HH
            call    bfupd           ; [1016] [cd 3f 10]    update 1203h..1208h display
loop2:      nop                     ; [1019] [00]          4347 T-state fine timing trim
            djnz    loop2           ; [101a] [10 fd]
            call    timet           ; [101c] [cd 60 10]    update appliance relays on hour
            jr      clock           ; [101f] [18 eb]

            defs    1027h - $, 00h
tmupd:      ld      hl,modtab       ; [1027] [21 57 10]    HL -> modulo limits (60h,60h,24h)
            ld      de,timebf       ; [102a] [11 00 12]    DE -> 1200h..1202h
            ld      b,03h           ; [102d] [06 03]
            scf                     ; [102f] [37]
tminc:      ld      a,(de)          ; [1030] [1a]
            adc     a,00h           ; [1031] [ce 00]
            daa                     ; [1033] [27]
            ld      (de),a          ; [1034] [12]
            sub     (hl)            ; [1035] [96]
            jr      c,compl         ; [1036] [38 01]
            ld      (de),a          ; [1038] [12]
compl:      ccf                     ; [1039] [3f]
            inc     hl              ; [103a] [23]
            inc     de              ; [103b] [13]
            djnz    tminc           ; [103c] [10 f2]
            ret                     ; [103e] [c9]

bfupd:      ld      hl,outbf        ; [103f] [21 03 12]
            ld      de,timebf       ; [1042] [11 00 12]
            ld      b,03h           ; [1045] [06 03]
putbf:      ld      a,(de)          ; [1047] [1a]
            call    tobyseg         ; [1048] [cd 25 05]
            inc     de              ; [104b] [13]
            djnz    putbf           ; [104c] [10 f9]
            dec     hl              ; [104e] [2b]
            dec     hl              ; [104f] [2b]
            set     6,(hl)          ; [1050] [cb f6]       DP on hours low digit (1207h)
            dec     hl              ; [1052] [2b]
            dec     hl              ; [1053] [2b]
            set     6,(hl)          ; [1054] [cb f6]       DP on minutes low digit (1205h)
            ret                     ; [1056] [c9]

modtab:     db      60h, 60h, 24h   ; [1057..1059] modulo limits: 60s, 60m, 24h

            defs    1060h - $, 00h
timet:      ld      hl,timebf       ; [1060] [21 00 12]    HL -> seconds (1200h)
            ld      a,(hl)          ; [1063] [7e]
            cp      00h             ; [1064] [fe 00]
            jr      nz,retur        ; [1066] [20 06]
            inc     hl              ; [1068] [23]          HL -> minutes (1201h)
            ld      a,(hl)          ; [1069] [7e]
            cp      00h             ; [106a] [fe 00]
            jr      z,esave         ; [106c] [28 01]
retur:      ret                     ; [106e] [c9]

esave:      nop                     ; [106f] [00]
            inc     hl              ; [1070] [23]          HL -> hours (1202h, BCD 00h..24h)
            ld      e,(hl)          ; [1071] [5e]
            ld      d,11h           ; [1072] [16 11]       DE = 1100h + BCD_hour
            ld      a,(de)          ; [1074] [1a]          load scheduled relay mask
            out     (aux_pc),a      ; [1075] [d3 42]       drive 8 appliance relays on Port 42h
            ret                     ; [1077] [c9]

            defs    1100h - $, 00h
            ; 24-Hour BCD-indexed Appliance Relay Schedule Table (1100h..1124h)
            db      60h, 60h, 60h, 60h, 60h                ; [1100..1104] 00:00-04:00
            db      68h, 0e1h, 0e0h, 0c4h, 0c4h            ; [1105..1109] 05:00-09:00
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [110a..110f] BCD gap (0Ah..0Fh)
            db      0d0h, 0d8h, 0d2h, 0d0h, 0d4h           ; [1110..1114] 10:00-14:00
            db      0d4h, 0d4h, 0d8h, 0d2h, 0f2h           ; [1115..1119] 15:00-19:00
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [111a..111f] BCD gap (1Ah..1Fh)
            db      0b2h, 0e2h, 65h, 65h, 60h              ; [1120..1124] 20:00-24:00
