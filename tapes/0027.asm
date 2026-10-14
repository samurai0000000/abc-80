; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0027
; Annotation: Lab27: School Bell Clock (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 27：校鐘 (pp. 175-178, PDF pp. 47-48)
;   24-hour BCD clock (1200h..1202h = SS,MM,HH; 1203h..1208h = display buffer).
;   TIMET (1060h) checks for :00:00 and :10:00 at every hour >= 08:00 and
;   invokes BALL (107bh) to pull Port C bit 6 low (3Fh -> 82h), play an 8-note
;   chime via SOUND (047ch), restore Port C (FFh), and add 15s to (1200h).
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator (C=pitch, HL=cycles)
scd_k1      equ     04cdh           ; 10 ms display multiplex pass
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
pc          equ     82h             ; 8255 Port C (bit 6 = bell relay, bit 7 = speaker)
timebf      equ     1200h           ; 3-byte BCD time (1200h=SS, 1201h=MM, 1202h=HH)
outbf       equ     1203h           ; 6-byte display buffer (1203h..1208h)

            org     1000h

            di                      ; [1000] [f3]
            ld      ix,outbf        ; [1001] [dd 21 03 12] IX -> 1203h display buffer
clock:      ld      b,64h           ; [1005] [06 64]       100 * 10 ms = 1.0s
loop1:      call    scd_k1          ; [1007] [cd cd 04]
            djnz    loop1           ; [100a] [10 fb]
            call    tmupd           ; [100c] [cd 27 10]    increment SS:MM:HH
            call    bfupd           ; [100f] [cd 3f 10]    update 1203h..1208h display
loop2:      nop                     ; [1012] [00]          4347 T-state fine timing trim
            djnz    loop2           ; [1013] [10 fd]
            call    timet           ; [1015] [cd 60 10]    check school bell schedule
            jr      clock           ; [1018] [18 eb]

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
            cp      00h             ; [1064] [fe 00]       at :00 seconds?
            jr      nz,return       ; [1066] [20 0a]
            inc     hl              ; [1068] [23]          HL -> minutes (1201h)
            ld      a,(hl)          ; [1069] [7e]
            cp      00h             ; [106a] [fe 00]       on the hour (:00)?
            jr      z,ballt         ; [106c] [28 05]
            cp      10h             ; [106e] [fe 10]       10 minutes past (:10)?
            jr      z,ballt         ; [1070] [28 01]
return:     ret                     ; [1072] [c9]
ballt:      inc     hl              ; [1073] [23]          HL -> hours (1202h)
            ld      a,(hl)          ; [1074] [7e]
            sub     08h             ; [1075] [d6 08]       is hour >= 08:00?
            call    nc,ball         ; [1077] [d4 7b 10]    trigger school bell!
            ret                     ; [107a] [c9]

ball:       ld      a,3fh           ; [107b] [3e 3f]       pull PC6 low (activate bell relay)
            out     (pc),a          ; [107d] [d3 82]
            exx                     ; [107f] [d9]
            ld      c,65h           ; [1080] [0e 65]       chime note 1
            ld      hl,0400h        ; [1082] [21 00 04]
            call    sound           ; [1085] [cd 7c 04]
            ld      c,80h           ; [1088] [0e 80]       chime note 2
            ld      hl,03aah        ; [108a] [21 aa 03]
            call    sound           ; [108d] [cd 7c 04]
            ld      c,72h           ; [1090] [0e 72]       chime note 3
            ld      hl,03aah        ; [1092] [21 aa 03]
            call    sound           ; [1095] [cd 7c 04]
            ld      c,0adh          ; [1098] [0e ad]       chime note 4
            ld      hl,03aah        ; [109a] [21 aa 03]
            call    sound           ; [109d] [cd 7c 04]
            ld      b,0ffh          ; [10a0] [06 ff]       inter-phrase pause
delay:      ld      c,0ffh          ; [10a2] [0e ff]
dloop:      dec     c               ; [10a4] [0d]
            jr      nz,dloop        ; [10a5] [20 fd]
            djnz    delay           ; [10a7] [10 f9]
            ld      c,0adh          ; [10a9] [0e ad]       chime note 5
            ld      hl,02aah        ; [10ab] [21 aa 02]
            call    sound           ; [10ae] [cd 7c 04]
            ld      c,72h           ; [10b1] [0e 72]       chime note 6
            ld      hl,0400h        ; [10b3] [21 00 04]
            call    sound           ; [10b6] [cd 7c 04]
            ld      c,65h           ; [10b9] [0e 65]       chime note 7
            ld      hl,0400h        ; [10bb] [21 00 04]
            call    sound           ; [10be] [cd 7c 04]
            ld      c,80h           ; [10c1] [0e 80]       chime note 8
            ld      hl,04aah        ; [10c3] [21 aa 04]
            call    sound           ; [10c6] [cd 7c 04]
            exx                     ; [10c9] [d9]
            ld      a,0ffh          ; [10ca] [3e ff]       release bell relay (PC6 high)
            out     (pc),a          ; [10cc] [d3 82]
            ld      a,(timebf)      ; [10ce] [3a 00 12]    compensate 15s chime duration
            add     a,15h           ; [10d1] [c6 15]
            ld      (timebf),a      ; [10d3] [32 00 12]
            ret                     ; [10d6] [c9]
