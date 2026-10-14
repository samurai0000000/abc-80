; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0024
; Annotation: Lab24: Traffic Light (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 24：紅綠燈 (pp. 170-171)
;   Drives Road A & Road B Green/Yellow/Red traffic lights (with 3x flashing
;   yellow transitions) on 8255 Port B (81h).
; ==============================================================================

pb          equ     81h             ; 8255 Port B (traffic light LEDs)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
start:      ld      a,0ch           ; [1004] [3e 0c] Road A Green, Road B Red
            out     (pb),a          ; [1006] [d3 81]
            ld      c,07h           ; [1008] [0e 07] hold for 7 delay units
            call    delay           ; [100a] [cd 49 10]
            ld      b,03h           ; [100d] [06 03] flash Road A Yellow 3 times
loop1:      ld      a,0ah           ; [100f] [3e 0a] Road A Yellow ON, Road B Red
            out     (pb),a          ; [1011] [d3 81]
            ld      c,01h           ; [1013] [0e 01]
            call    delay           ; [1015] [cd 49 10]
            ld      a,08h           ; [1018] [3e 08] Road A Yellow OFF, Road B Red
            out     (pb),a          ; [101a] [d3 81]
            ld      c,01h           ; [101c] [0e 01]
            call    delay           ; [101e] [cd 49 10]
            dec     b               ; [1021] [05]
            jp      nz,loop1        ; [1022] [c2 0f 10]

            ld      a,21h           ; [1025] [3e 21] Road B Green, Road A Red
            out     (pb),a          ; [1027] [d3 81]
            ld      c,07h           ; [1029] [0e 07] hold for 7 delay units
            call    delay           ; [102b] [cd 49 10]
            ld      b,03h           ; [102e] [06 03] flash Road B Yellow 3 times
loop2:      ld      a,11h           ; [1030] [3e 11] Road B Yellow ON, Road A Red
            out     (pb),a          ; [1032] [d3 81]
            ld      c,01h           ; [1034] [0e 01]
            call    delay           ; [1036] [cd 49 10]
            ld      a,01h           ; [1039] [3e 01] Road B Yellow OFF, Road A Red
            out     (pb),a          ; [103b] [d3 81]
            ld      c,01h           ; [103d] [0e 01]
            call    delay           ; [103f] [cd 49 10]
            dec     b               ; [1042] [05]
            jp      nz,loop2        ; [1043] [c2 30 10]
            jp      start           ; [1046] [c3 04 10]

delay:      ld      de,8000h        ; [1049] [11 00 80]
wat:        dec     de              ; [104c] [1b]
            ld      a,d             ; [104d] [7a]
            or      e               ; [104e] [b3]
            jp      nz,wat          ; [104f] [c2 4c 10]
            dec     c               ; [1052] [0d]
            jp      nz,delay        ; [1053] [c2 49 10]
            ret                     ; [1056] [c9]
