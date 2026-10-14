; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0029
; Annotation: Lab29: Robotic Arm (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 29：模擬機械手 (pp. 184-195, PDF pp. 53-56)
;   Controls 3 stepper motors + electromagnet gripper via Expansion 8255 PPI at
;   ports 00h..03h (74LS139 pin 12):
;     Port A (00h): Motor 1 (Base rotation, bits 0..1 -> 2-bit phase 00..03)
;     Port B (01h): Motor 2 (bits 0..1) & Motor 3 (bits 4..5)
;     Port C (02h): Electromagnet gripper (01h = attract/UP, 00h = release/DOWN)
;     Control (03h): 80h (Mode 0 all outputs)
;   Command script table NN at 1167h..1173h dispatches ONEFOR (00h), ONEBAK (01h),
;   TWOFOR (02h), TWOBAK (03h), THFOR (04h), THBAK (05h), UP (06h), DOWN (07h),
;   and STOP (08h).
; ==============================================================================

pa          equ     00h             ; Expansion 8255 Port A (Motor 1)
pb          equ     01h             ; Expansion 8255 Port B (Motors 2 & 3)
pc          equ     02h             ; Expansion 8255 Port C (Electromagnet)
cwr         equ     03h             ; Expansion 8255 Control Register

            org     1000h

            ld      a,80h           ; [1000] [3e 80] Expansion 8255 Mode 0 all outputs
            out     (cwr),a         ; [1002] [d3 03]
            ld      a,00h           ; [1004] [3e 00] zero all motor & magnet ports
            out     (pa),a          ; [1006] [d3 00]
            out     (pb),a          ; [1008] [d3 01]
            out     (pc),a          ; [100a] [d3 02]
            ld      hl,cmd_tab      ; [100c] [21 67 11] HL -> command sequence table
loop:       ld      a,00h           ; [100f] [3e 00] FORDB1: Motor 1 forward
            cp      (hl)            ; [1011] [be]
            call    z,onefor        ; [1012] [cc 49 10]
            ld      a,01h           ; [1015] [3e 01] BAKDB1: Motor 1 backward
            cp      (hl)            ; [1017] [be]
            call    z,onebak        ; [1018] [cc 6b 10]
            ld      a,02h           ; [101b] [3e 02] FORDB2: Motor 2 forward
            cp      (hl)            ; [101d] [be]
            call    z,twofor        ; [101e] [cc 8e 10]
            ld      a,03h           ; [1021] [3e 03] BAKDB2: Motor 2 backward
            cp      (hl)            ; [1023] [be]
            call    z,twobak        ; [1024] [cc b9 10]
            ld      a,04h           ; [1027] [3e 04] FORDB3: Motor 3 forward
            cp      (hl)            ; [1029] [be]
            call    z,thfor         ; [102a] [cc e4 10]
            ld      a,05h           ; [102d] [3e 05] BAKDB3: Motor 3 backward
            cp      (hl)            ; [102f] [be]
            call    z,thbak         ; [1030] [cc 18 11]
            ld      a,06h           ; [1033] [3e 06] UPDB: Electromagnet ON
            cp      (hl)            ; [1035] [be]
            call    z,up            ; [1036] [cc 4c 11]
            ld      a,07h           ; [1039] [3e 07] DOWNDB: Electromagnet OFF
            cp      (hl)            ; [103b] [be]
            call    z,down          ; [103c] [cc 55 11]
            ld      a,08h           ; [103f] [3e 08] STOPDB: End of trajectory
            cp      (hl)            ; [1041] [be]
            jr      z,stop          ; [1042] [28 03]
            jp      loop            ; [1044] [c3 0f 10]
stop:       halt                    ; [1047] [76]
nn:         db      00h             ; [1048] [00] Motor 1 phase state (00,01,03,02)

            ; --- Motor 1 Forward (00 -> 01 -> 03 -> 02 -> 00) ---
onefor:     inc     hl              ; [1049] [23]
            ld      b,(hl)          ; [104a] [46]    B = step count
loop1:      ld      a,(nn)          ; [104b] [3a 48 10]
            cp      00h             ; [104e] [fe 00]
            jr      z,go1           ; [1050] [28 0a]
            cp      01h             ; [1052] [fe 01]
            jr      z,run1          ; [1054] [28 05]
            cp      02h             ; [1056] [fe 02]
            jr      z,run1          ; [1058] [28 01]
            inc     a               ; [105a] [3c]
run1:       inc     a               ; [105b] [3c]
go1:        inc     a               ; [105c] [3c]
            and     03h             ; [105d] [e6 03]
            ld      (nn),a          ; [105f] [32 48 10]
            out     (pa),a          ; [1062] [d3 00]
            call    delay           ; [1064] [cd 5e 11]
            djnz    loop1           ; [1067] [10 e2]
            inc     hl              ; [1069] [23]
            ret                     ; [106a] [c9]

            ; --- Motor 1 Backward (00 -> 02 -> 03 -> 01 -> 00) ---
onebak:     inc     hl              ; [106b] [23]
            ld      b,(hl)          ; [106c] [46]
loop2:      ld      a,(nn)          ; [106d] [3a 48 10]
            cp      00h             ; [1070] [fe 00]
            jr      z,run2          ; [1072] [28 09]
            cp      02h             ; [1074] [fe 02]
            jr      z,go2           ; [1076] [28 06]
            cp      03h             ; [1078] [fe 03]
            jr      z,run2          ; [107a] [28 01]
            inc     a               ; [107c] [3c]
run2:       inc     a               ; [107d] [3c]
go2:        inc     a               ; [107e] [3c]
            and     03h             ; [107f] [e6 03]
            ld      (nn),a          ; [1081] [32 48 10]
            out     (pa),a          ; [1084] [d3 00]
            call    delay           ; [1086] [cd 5e 11]
            djnz    loop2           ; [1089] [10 e2]
            inc     hl              ; [108b] [23]
            ret                     ; [108c] [c9]

portb:      db      00h             ; [108d] [00] Port B state (Motors 2 & 3)

            ; --- Motor 2 Forward (Port B bits 0..1) ---
twofor:     inc     hl              ; [108e] [23]
            ld      b,(hl)          ; [108f] [46]
loop3:      ld      a,(portb)       ; [1090] [3a 8d 10]
            and     30h             ; [1093] [e6 30] preserve Motor 3 bits
            ld      c,a             ; [1095] [4f]
            ld      a,(portb)       ; [1096] [3a 8d 10]
            and     03h             ; [1099] [e6 03] isolate Motor 2 bits
            cp      00h             ; [109b] [fe 00]
            jr      z,go3           ; [109d] [28 0a]
            cp      01h             ; [109f] [fe 01]
            jr      z,run3          ; [10a1] [28 05]
            cp      02h             ; [10a3] [fe 02]
            jr      z,run3          ; [10a5] [28 01]
            inc     a               ; [10a7] [3c]
run3:       inc     a               ; [10a8] [3c]
go3:        inc     a               ; [10a9] [3c]
            and     03h             ; [10aa] [e6 03]
            or      c               ; [10ac] [b1]
            ld      (portb),a       ; [10ad] [32 8d 10]
            out     (pb),a          ; [10b0] [d3 01]
            call    delay           ; [10b2] [cd 5e 11]
            djnz    loop3           ; [10b5] [10 d9]
            inc     hl              ; [10b7] [23]
            ret                     ; [10b8] [c9]

            ; --- Motor 2 Backward (Port B bits 0..1) ---
twobak:     inc     hl              ; [10b9] [23]
            ld      b,(hl)          ; [10ba] [46]
loop4:      ld      a,(portb)       ; [10bb] [3a 8d 10]
            and     30h             ; [10be] [e6 30]
            ld      c,a             ; [10c0] [4f]
            ld      a,(portb)       ; [10c1] [3a 8d 10]
            and     03h             ; [10c4] [e6 03]
            cp      00h             ; [10c6] [fe 00]
            jr      z,run4          ; [10c8] [28 09]
            cp      02h             ; [10ca] [fe 02]
            jr      z,go4           ; [10cc] [28 06]
            cp      03h             ; [10ce] [fe 03]
            jr      z,run4          ; [10d0] [28 01]
            inc     a               ; [10d2] [3c]
run4:       inc     a               ; [10d3] [3c]
go4:        inc     a               ; [10d4] [3c]
            and     03h             ; [10d5] [e6 03]
            or      c               ; [10d7] [b1]
            ld      (portb),a       ; [10d8] [32 8d 10]
            out     (pb),a          ; [10db] [d3 01]
            call    delay           ; [10dd] [cd 5e 11]
            djnz    loop4           ; [10e0] [10 d9]
            inc     hl              ; [10e2] [23]
            ret                     ; [10e3] [c9]

            ; --- Motor 3 Forward (Port B bits 4..5: 00 -> 10 -> 30 -> 20 -> 00) ---
thfor:      inc     hl              ; [10e4] [23]
            ld      b,(hl)          ; [10e5] [46]
loop5:      ld      a,(portb)       ; [10e6] [3a 8d 10]
            and     03h             ; [10e9] [e6 03] preserve Motor 2 bits
            ld      c,a             ; [10eb] [4f]
            ld      a,(portb)       ; [10ec] [3a 8d 10]
            and     30h             ; [10ef] [e6 30] isolate Motor 3 bits
            cp      00h             ; [10f1] [fe 00]
            jr      nz,next1        ; [10f3] [20 04]
            ld      a,10h           ; [10f5] [3e 10]
            jr      oper1           ; [10f7] [18 12]
next1:      cp      10h             ; [10f9] [fe 10]
            jr      nz,go5          ; [10fb] [20 04]
            ld      a,30h           ; [10fd] [3e 30]
            jr      oper1           ; [10ff] [18 0a]
go5:        cp      20h             ; [1101] [fe 20]
            jr      nz,run5         ; [1103] [20 04]
            ld      a,00h           ; [1105] [3e 00]
            jr      oper1           ; [1107] [18 02]
run5:       ld      a,20h           ; [1109] [3e 20]
oper1:      or      c               ; [110b] [b1]
            ld      (portb),a       ; [110c] [32 8d 10]
            out     (pb),a          ; [110f] [d3 01]
            call    delay           ; [1111] [cd 5e 11]
            djnz    loop5           ; [1114] [10 d0]
            inc     hl              ; [1116] [23]
            ret                     ; [1117] [c9]

            ; --- Motor 3 Backward (Port B bits 4..5: 00 -> 20 -> 30 -> 10 -> 00) ---
thbak:      inc     hl              ; [1118] [23]
            ld      b,(hl)          ; [1119] [46]
loop6:      ld      a,(portb)       ; [111a] [3a 8d 10]
            and     03h             ; [111d] [e6 03]
            ld      c,a             ; [111f] [4f]
            ld      a,(portb)       ; [1120] [3a 8d 10]
            and     30h             ; [1123] [e6 30]
            cp      00h             ; [1125] [fe 00]
            jr      nz,next2        ; [1127] [20 04]
            ld      a,20h           ; [1129] [3e 20]
            jr      oper2           ; [112b] [18 12]
next2:      cp      20h             ; [112d] [fe 20]
            jr      nz,go6          ; [112f] [20 04]
            ld      a,30h           ; [1131] [3e 30]
            jr      oper2           ; [1133] [18 0a]
go6:        cp      30h             ; [1135] [fe 30]
            jr      nz,run6         ; [1137] [20 04]
            ld      a,10h           ; [1139] [3e 10]
            jr      oper2           ; [113b] [18 02]
run6:       ld      a,00h           ; [113d] [3e 00]
oper2:      or      c               ; [113f] [b1]
            ld      (portb),a       ; [1140] [32 8d 10]
            out     (pb),a          ; [1143] [d3 01]
            call    delay           ; [1145] [cd 5e 11]
            djnz    loop6           ; [1148] [10 d0]
            inc     hl              ; [114a] [23]
            ret                     ; [114b] [c9]

            ; --- Electromagnet ON (UP) & OFF (DOWN) ---
up:         ld      a,01h           ; [114c] [3e 01] energize electromagnet on Port C
            out     (pc),a          ; [114e] [d3 02]
            call    delay           ; [1150] [cd 5e 11]
            inc     hl              ; [1153] [23]
            ret                     ; [1154] [c9]

down:       ld      a,00h           ; [1155] [3e 00] release electromagnet on Port C
            out     (pc),a          ; [1157] [d3 02]
            call    delay           ; [1159] [cd 5e 11]
            inc     hl              ; [115c] [23]
            ret                     ; [115d] [c9]

delay:      ld      de,4000h        ; [115e] [11 00 40] step pulse interval delay
wat:        dec     de              ; [1161] [1b]
            ld      a,d             ; [1162] [7a]
            or      e               ; [1163] [b3]
            jr      nz,wat          ; [1164] [20 fb]
            ret                     ; [1166] [c9]

            ; --- Robotic Arm Trajectory Command Table (1167h..1173h) ---
cmd_tab:    db      00h, 0ah        ; [1167..1168] Motor 1 forward 10 steps
            db      02h, 10h        ; [1169..116a] Motor 2 forward 16 steps
            db      05h, 08h        ; [116b..116c] Motor 3 backward 8 steps
            db      06h             ; [116d]       Electromagnet ON (pick object)
            db      04h, 0bh        ; [116e..116f] Motor 3 forward 11 steps
            db      01h, 0eh        ; [1170..1171] Motor 1 backward 14 steps
            db      07h             ; [1172]       Electromagnet OFF (place object)
            db      08h             ; [1173]       STOP
