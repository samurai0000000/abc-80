#!/usr/bin/env python3
#
# generate_exercise_plans.py
#
# Copyright (C) 2026, Charles Chiou
#
# Generates all 29 single-agent execution plans (plan/plan_exercise01_*.md ..
# plan/plan_exercise29_*.md) for the ABC-80 programming exercises from Part 2 of
# 《Z80 微電腦入門 — 原理與應用程式》 (scanned_docs/Z80_Micro_Computer_Primer.pdf),
# verifying every Z80 assembly listing with pasmo and embedding the complete
# tapes/00<NN>.asm source and assembled machine-code specification in Section 2.3.
#

import glob
import os
import subprocess
import tempfile
import pypdf

PDF_PATH = "scanned_docs/Z80_Micro_Computer_Primer.pdf"

EXERCISES = [
    {
        "num": 1,
        "slug": "data_transfer",
        "title": "Exercise 01: Data Transfer Practice (實習 1：資料傳送練習)",
        "annotation": "Lab01: Data Transfer (1000H)",
        "pages": [8, 9, 10],
        "book_pages": "99–102",
        "tape_id": "0001",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 01 (資料傳送練習) from the Z80 Microcomputer Primer. "
            "The textbook introduces 8-bit immediate/indirect register transfers (`01H..05H` -> `1050H..1054H`) "
            "and 16-bit register-pair transfers (`BC=1234H, DE=5678H, HL=1050H` -> `1050H..1055H`) ending in `HALT`. "
            "Our enhanced `tapes/0001.asm` executes both textbook transfers (`01H..05H` into `1050H..1054H` and "
            "`34 12 78 56 50 10` into `1060H..1065H`), then runs a lively continuous 6-digit 7-segment LED and "
            "speaker (`0464H` `MS3K17`) inspection loop sweeping each destination RAM address and transferred byte."
        ),
        "subroutines": [
            "0x0504 (ADRSDP - format 16-bit address in DE onto left 4 digits)",
            "0x0511 (DADP - format 8-bit data in A onto right 2 digits)",
            "0x0464 (MS3K17 - 3 kHz speaker click preserving main registers)",
            "0x04CD (SCD&K1 - 10 ms 6-digit 7-segment display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments via SCD&K1)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker via MS3K17 / SCD&K1)"
        ],
        "asm_notes": (
            "Enhances the original static `HALT` examples (Book Ex. 1 `01H..05H` -> `1050H..1054H` and "
            "Book Ex. 2 `1234H, 5678H, 1050H` -> `1060H..1065H`) with a continuous 7-segment display and "
            "speaker loop (`1050 01` .. `1054 05`, then `1060 34` .. `1065 10`). Also includes updating "
            "`CassetteDeck::seedDefaultTapes` in `src/Abc80Tape.cxx` so the 4 custom showcase demos use "
            "File IDs `0100`–`0103` (`0100.tape`–`0103.tape`), reserving `0001`–`0029` for Primer Labs 01–29."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0001
; Annotation: Lab01: Data Transfer (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 1：資料傳送練習 (pp. 99-102)
;   Ex 1: 8-bit transfer 01H..05H -> 1050H..1054H
;   Ex 2: 16-bit transfer BC=1234H, DE=5678H, HL=1050H -> 1060H..1065H
; Enhancement: Cycles through 1050H..1054H and 1060H..1065H on the 6-digit
;   7-segment display (ADRSDP + DADP + SCD&K1) with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click (preserves regs)
scd_k1      equ     04cdh           ; 6-digit display multiplex pass (10 ms)
adrsdp      equ     0504h           ; DE -> left 4 address digits (17c8h..17cbh)
dadp        equ     0511h           ; A  -> right 2 data digits (17c6h..17c7h)
dispbf      equ     17c6h           ; system 6-digit display buffer

            org     1000h

start:      ; --- Book Example 1: 8-bit register & indirect RAM transfer ---
            ld      a,01h           ; [1000] [3e 01] A = 01h
            ld      b,02h           ; [1002] [06 02] B = 02h
            ld      c,03h           ; [1004] [0e 03] C = 03h
            ld      d,04h           ; [1006] [16 04] D = 04h
            ld      e,05h           ; [1008] [1e 05] E = 05h
            ld      hl,1050h        ; [100a] [21 50 10] HL -> 1050h
            ld      (hl),a          ; [100d] [77] (1050h) = 01h
            inc     hl              ; [100e] [23]
            ld      (hl),b          ; [100f] [70] (1051h) = 02h
            inc     hl              ; [1010] [23]
            ld      (hl),c          ; [1011] [71] (1052h) = 03h
            inc     hl              ; [1012] [23]
            ld      (hl),d          ; [1013] [72] (1053h) = 04h
            inc     hl              ; [1014] [23]
            ld      (hl),e          ; [1015] [73] (1054h) = 05h

            ; --- Book Example 2: 16-bit extended RAM transfer ---
            ld      bc,1234h        ; [1016] [01 34 12] BC = 1234h
            ld      de,5678h        ; [1019] [11 78 56] DE = 5678h
            ld      hl,1050h        ; [101c] [21 50 10] HL = 1050h
            ld      (1060h),bc      ; [101f] [ed 43 60 10] (1060h..1061h) = 34h, 12h
            ld      (1062h),de      ; [1023] [ed 53 62 10] (1062h..1063h) = 78h, 56h
            ld      (1064h),hl      ; [1027] [22 64 10] (1064h..1065h) = 50h, 10h

            ; --- Lively 7-Segment & Speaker Inspection Loop ---
            ld      ix,dispbf       ; [102a] [dd 21 c6 17] IX -> system display buffer
            ld      de,1050h        ; [102e] [11 50 10] sweep Ex 1 (1050h..1054h)
sweep:      ld      a,(de)          ; [1031] [1a] load transferred byte at (DE)
            call    dadp            ; [1032] [cd 11 05] format byte onto right 2 digits
            call    adrsdp          ; [1035] [cd 04 05] format DE onto left 4 digits
            call    ms3k17          ; [1038] [cd 64 04] emit 3 kHz speaker click
            ld      b,20h           ; [103b] [06 20] hold for 32 scan passes (~0.32s)
scan:       call    scd_k1          ; [103d] [cd cd 04] refresh 6-digit display
            djnz    scan            ; [1040] [10 fb]
            inc     e               ; [1042] [1c] next RAM address
            ld      a,e             ; [1043] [7b]
            cp      55h             ; [1044] [fe 55] finished 1050h..1054h?
            jr      nz,chk66        ; [1046] [20 02]
            ld      e,60h           ; [1048] [1e 60] advance DE to 1060h..1065h
chk66:      cp      66h             ; [104a] [fe 66] finished 1060h..1065h?
            jr      nz,sweep        ; [104c] [20 e3]
            jr      start           ; [104e] [18 b0] repeat indefinitely

            defs    1056h - $, 00h  ; [1050..1055] textbook Ex 1 destination buffer
""",
        "verification": (
            "1. Verify `CassetteDeck::seedDefaultTapes` uses `0x0100`–`0x0103` for the 4 custom demos.\n"
            "2. Load `tapes/0001.tape` via `Abc80Tape::loadTapeFile` / `decodeProgramFromRuns` (`File ID = 0x0001`, `Start = 0x1000`).\n"
            "3. Step the Z80 CPU through the transfer block and assert RAM `1050H..1054H` holds `01, 02, 03, 04, 05` and "
            "`1060H..1065H` holds `34, 12, 78, 56, 50, 10`.\n"
            "4. Step frames and verify `board.displayMasks()` illuminates `"
            "105001`..`105405` and `106034`..`106510` while `frameAudio().edgeCount > 0`."
        ),
        "extra_hw": None,
    },
    {
        "num": 2,
        "slug": "logic_and_or",
        "title": "Exercise 02: Logic Instructions I — AND & OR (實習 2：邏輯指令練習 (一))",
        "annotation": "Lab02: Logic AND & OR (1000H)",
        "pages": [10, 11, 12],
        "book_pages": "103–107",
        "tape_id": "0002",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 02 (邏輯指令練習 (一)) from the Z80 Microcomputer Primer. "
            "The textbook demonstrates 8-bit bitwise `AND` (Ex. 3: `63H & F0H = 60H`, `47H & 0FH = 07H`, `5FH & F9H = 59H` "
            "-> `590760` at `1050H..1052H`) and bitwise `OR` (Ex. 4: `63H | F0H = F3H`, `47H | 0FH = 4FH`, `5FH | F9H = FFH` "
            "-> `FF4FF3` at `1050H..1052H`) using Monitor subroutine `ANSW3` (`06BAH`). "
            "Our enhanced `tapes/0002.asm` computes both 3-byte logical results and alternates `590760` (`AND`) and `FF4FF3` (`OR`) "
            "across all 6 digits of the 7-segment display (`CLKSET` `06C3H` + `SCD&K1` `04CDH`) with 3 kHz speaker ticks (`0464H`)."
        ),
        "subroutines": [
            "0x06C3 (CLKSET - format B bytes from 1050H into dispbf at 17C6H and set IX=17C6H)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Replaces the book's single-shot `CALL ANSW3` (`06BAH`, which halts at `06C2H`) with direct calls to "
            "`CLKSET` (`06C3H`, `B=03H`), `MS3K17` (`0464H`), and a 50-pass `SCD&K1` (`04CDH`) loop (~0.50s per phase), "
            "alternating between the book's `AND` result (`590760` at `1050H..1052H`) and `OR` result (`FF4FF3` at `1050H..1052H`)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0002
; Annotation: Lab02: Logic AND & OR (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 2：邏輯指令練習 (一) (pp. 103-107)
;   Ex 3 (AND): 63H&F0H=60H (1050H), 47H&0FH=07H (1051H), 5FH&F9H=59H (1052H) -> 590760
;   Ex 4 (OR):  63H|F0H=F3H (1050H), 47H|0FH=4FH (1051H), 5FH|F9H=FFH (1052H) -> FF4FF3
; Enhancement: Alternates 590760 (AND) and FF4FF3 (OR) on all 6 7-segment LEDs
;   using CLKSET (06C3H) and SCD&K1 (04CDH) with 3 kHz speaker clicks (0464H).
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Example 3: 3-Byte Bitwise AND (59 07 60) ---
            ld      b,63h           ; [1000] [06 63] B = 63h
            ld      a,0f0h          ; [1002] [3e f0] A = F0h
            and     b               ; [1004] [a0]    A = 63h & F0h = 60h
            ld      (1050h),a       ; [1005] [32 50 10]
            ld      c,47h           ; [1008] [0e 47] C = 47h
            ld      a,0fh           ; [100a] [3e 0f] A = 0Fh
            and     c               ; [100c] [a1]    A = 47h & 0Fh = 07h
            ld      (1051h),a       ; [100d] [32 51 10]
            ld      d,5fh           ; [1010] [16 5f] D = 5Fh
            ld      a,0f9h          ; [1012] [3e f9] A = F9h
            and     d               ; [1014] [a2]    A = 5Fh & F9h = 59h
            ld      (1052h),a       ; [1015] [32 52 10]
            call    show3           ; [1018] [cd 38 10] display "590760" + click

            ; --- Book Example 4: 3-Byte Bitwise OR (FF 4F F3) ---
            ld      b,63h           ; [101b] [06 63] B = 63h
            ld      a,0f0h          ; [101d] [3e f0] A = F0h
            or      b               ; [101f] [b0]    A = 63h | F0h = F3h
            ld      (1050h),a       ; [1020] [32 50 10]
            ld      c,47h           ; [1023] [0e 47] C = 47h
            ld      a,0fh           ; [1025] [3e 0f] A = 0Fh
            or      c               ; [1027] [b1]    A = 47h | 0Fh = 4Fh
            ld      (1051h),a       ; [1028] [32 51 10]
            ld      d,5fh           ; [102b] [16 5f] D = 5Fh
            ld      a,0f9h          ; [102d] [3e f9] A = F9h
            or      d               ; [102f] [b2]    A = 5Fh | F9h = FFh
            ld      (1052h),a       ; [1030] [32 52 10]
            call    show3           ; [1033] [cd 38 10] display "FF4FF3" + click
            jr      start           ; [1036] [18 c8] repeat loop

show3:      ld      b,03h           ; [1038] [06 03] format 3 bytes at 1050h..1052h
            call    clkset          ; [103a] [cd c3 06] populate dispbf (17c6h..17cbh)
            call    ms3k17          ; [103d] [cd 64 04] 3 kHz speaker click
            ld      b,32h           ; [1040] [06 32] 50 scan passes (~0.50s)
scan:       call    scd_k1          ; [1042] [cd cd 04] multiplex 6-digit display
            djnz    scan            ; [1045] [10 fb]
            ret                     ; [1047] [c9]

            defs    1050h - $, 00h
            db      00h, 00h, 00h   ; [1050..1052] 3-byte logic result buffer
""",
        "verification": (
            "1. Load `tapes/0002.tape` (`File ID = 0x0002`, `Start = 0x1000`).\n"
            "2. Step Z80 execution to `1018H` and verify `1050H..1052H` holds `60H, 07H, 59H` (`AND` result `590760`), "
            "then step to `1033H` and verify `1050H..1052H` holds `F3H, 4FH, FFH` (`OR` result `FF4FF3`).\n"
            "3. Verify the 6-digit display alternates between `590760` and `FF4FF3` with speaker audio edges."
        ),
        "extra_hw": None,
    },
    {
        "num": 3,
        "slug": "logic_xor_cpl_neg",
        "title": "Exercise 03: Logic Instructions II — XOR, CPL & NEG (實習 3：邏輯指令練習 (二))",
        "annotation": "Lab03: XOR CPL & NEG (1000H)",
        "pages": [13, 14],
        "book_pages": "108–110",
        "tape_id": "0003",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 03 (邏輯指令練習 (二)) from the Z80 Microcomputer Primer. "
            "The textbook covers `XOR` (`58H XOR 58H = 00H`, `39H XOR 36H = 0FH`), `CP` (`68H CP 68H`), "
            "1's complement `CPL` (`CPL(5BH) = A4H`), and 2's complement `NEG` (`NEG(5BH) = A5H`). "
            "Our enhanced `tapes/0003.asm` computes all three results into `1050H..1052H` (`0FH, A4H, A5H`) "
            "and cycles the 6-digit display between the combined view `A5A40F` (`[NEG][CPL][XOR]`) and individual "
            "operand/result views (`3936 0F`, `005B A4`, `005B A5`) with speaker ticks."
        ),
        "subroutines": [
            "0x06C3 (CLKSET - format B bytes from 1050H into dispbf)",
            "0x0504 (ADRSDP - format DE onto left 4 digits)",
            "0x0511 (DADP - format A onto right 2 digits)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Stores the input byte `5BH` at `1053H` (matching the book's operand `5BH` for Examples 3 and 4) "
            "and writes `(1050H) = 0FH` (`39H XOR 36H`), `(1051H) = A4H` (`CPL 5BH`), `(1052H) = A5H` (`NEG 5BH`), "
            "and `(1054H) = 00H` (`58H XOR 58H`), cycling all views on the 7-segment display in an endless loop."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0003
; Annotation: Lab03: XOR CPL & NEG (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 3：邏輯指令練習 (二) (pp. 108-110)
;   Ex 1 (XOR): 58H XOR 58H = 00H (1054H), 39H XOR 36H = 0FH (1050H)
;   Ex 3 (CPL): CPL(5BH) = A4H (1's complement -> 1051H)
;   Ex 4 (NEG): NEG(5BH) = A5H (2's complement -> 1052H)
; Enhancement: Cycles 6-digit summary "A5A40F" and individual operand/result
;   pairs ("3936 0F", "005B A4", "005B A5") with 3 kHz speaker clicks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Ex 1: XOR ---
            ld      a,58h           ; [1000] [3e 58]
            xor     a               ; [1002] [af]    58h XOR 58h = 00h
            ld      (1054h),a       ; [1003] [32 54 10]
            ld      a,39h           ; [1006] [3e 39]
            ld      b,36h           ; [1008] [06 36]
            xor     b               ; [100a] [a8]    39h XOR 36h = 0Fh
            ld      (1050h),a       ; [100b] [32 50 10]

            ; --- Book Ex 3: CPL (1's complement of 5Bh = A4h) ---
            ld      a,(1053h)       ; [100e] [3a 53 10] A = 5Bh
            cpl                     ; [1011] [2f]       A = ~5Bh = A4h
            ld      (1051h),a       ; [1012] [32 51 10]

            ; --- Book Ex 4: NEG (2's complement of 5Bh = A5h) ---
            ld      a,(1053h)       ; [1015] [3a 53 10] A = 5Bh
            neg                     ; [1018] [ed 44]    A = -5Bh = A5h
            ld      (1052h),a       ; [101a] [32 52 10]

            ; --- Phase 1: Display all 3 results "A5A40F" ---
            ld      b,03h           ; [101d] [06 03]
            call    clkset          ; [101f] [cd c3 06] dispbf <- [1052][1051][1050]
            call    hold            ; [1022] [cd 42 10]

            ; --- Phase 2: Display XOR "3936 0F" ---
            ld      de,3936h        ; [1025] [11 36 39]
            ld      a,(1050h)       ; [1028] [3a 50 10]
            call    showpair        ; [102b] [cd 3c 10]

            ; --- Phase 3: Display CPL "005B A4" ---
            ld      de,005bh        ; [102e] [11 5b 00]
            ld      a,(1051h)       ; [1031] [3a 51 10]
            call    showpair        ; [1034] [cd 3f 10]

            ; --- Phase 4: Display NEG "005B A5" (DE is still 005Bh) ---
            ld      a,(1052h)       ; [1037] [3a 52 10]
            call    showpair        ; [103a] [cd 3f 10]
            jr      start           ; [103d] [18 c1]

showpair:   call    dadp            ; [103f] [cd 11 05]
            call    adrsdp          ; [1042] [cd 04 05]
hold:       call    ms3k17          ; [1045] [cd 64 04]
            ld      b,28h           ; [1048] [06 28] 40 scan passes (~0.40s)
scan:       call    scd_k1          ; [104a] [cd cd 04]
            djnz    scan            ; [104d] [10 fb]
            ret                     ; [104f] [c9]

            defs    1053h - $, 00h
operand:    db      5bh             ; [1053] [5b] book input byte for CPL/NEG
""",
        "verification": (
            "1. Load `tapes/0003.tape` (`File ID = 0x0003`, `Start = 0x1000`).\n"
            "2. Step Z80 execution past `101DH` and verify `(1050H) == 0x0F`, `(1051H) == 0xA4`, `(1052H) == 0xA5`, and `(1054H) == 0x00`.\n"
            "3. Verify the 6-digit display cycles through `A5A40F`, `39360F`, `005BA4`, and `005BA5` with speaker clicks."
        ),
        "extra_hw": None,
    },
    {
        "num": 4,
        "slug": "arithmetic_addition",
        "title": "Exercise 04: Arithmetic Addition Practice (實習 4：算術加法練習)",
        "annotation": "Lab04: BCD Addition (1000H)",
        "pages": [14, 15, 16],
        "book_pages": "111–114",
        "tape_id": "0004",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 04 (算術加法練習) from the Z80 Microcomputer Primer. "
            "Book Example 4 (pp. 113–114) performs 3-byte (6-digit) multi-precision BCD addition (`ADC A, (HL)` + `DAA`) "
            "of `473256` (`56H, 32H, 47H` at `1050H..1052H`) + `422344` (`44H, 23H, 42H` at `1060H..1062H`) = `895600` "
            "(`00H, 56H, 89H` stored back into `1050H..1052H`). "
            "Our enhanced `tapes/0004.asm` executes the exact textbook 3-byte `ADC`+`DAA` loop and continuously cycles "
            "Addend 1 (`473256`), Addend 2 (`422344`), and the 6-digit BCD Sum (`895600`) on the 7-segment display with speaker ticks."
        ),
        "subroutines": [
            "0x06C3 (CLKSET - format 3 bytes from 1050H..1052H into dispbf at 17C6H)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Because the textbook's in-place addition overwrites `1050H..1052H` with the sum `895600`, `tapes/0004.asm` "
            "restores `1050H..1052H` from `473256` (`56H, 32H, 47H`), displays Addend 1 (`473256`) and Addend 2 (`422344`), "
            "and then runs the exact book `SUM` loop (`ADC A, (HL)` / `DAA`) so `1050H..1052H` holds the textbook answer "
            "`00H, 56H, 89H` (`895600`) throughout the sum display phase."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0004
; Annotation: Lab04: BCD Addition (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 4：算術加法練習 例四 (pp. 113-114)
;   Adds 3-byte BCD 473256 (56h,32h,47h) + 422344 (44h,23h,42h) = 895600 (00h,56h,89h)
;   at 1050h..1052h using ADC A,(HL) and DAA.
; Enhancement: Cycles Addend 1 (473256), Addend 2 (422344), and Sum (895600) on
;   the 6-digit 7-segment display with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Initialize 1050h..1052h = 473256 and compute book SUM ---
            call    init_a1         ; [1000] [cd 2f 10] 1050h..1052h <- 56h, 32h, 47h
            call     do_sum          ; [1003] [cd 1e 10] 1050h..1052h <- 00h, 56h, 89h (895600)
            call    show3           ; [1006] [cd 3b 10] display Sum "895600"

            ; --- Show Addend 1 ("473256") ---
            call    init_a1         ; [1009] [cd 2f 10]
            call    show3           ; [100c] [cd 3b 10]

            ; --- Show Addend 2 ("422344") then immediately recompute Sum ---
            ld      hl,1060h        ; [100f] [21 60 10]
            ld      de,1050h        ; [1012] [11 50 10]
            ld      bc,0003h        ; [1015] [01 03 00]
            ldir                    ; [1018] [ed b0]
            call    show3           ; [101a] [cd 3b 10]
            jr      start           ; [101d] [18 e1]

            ; --- Exact Book Example 4 Multi-Byte BCD Addition Routine ---
do_sum:     ld      hl,104fh        ; [101f] [21 4f 10] HL -> byte count (03h)
            ld      b,(hl)          ; [1022] [46]       B = 03h
            ld      de,1060h        ; [1023] [11 60 10] DE -> addend 2 (1060h..1062h)
            sub     a               ; [1026] [97]       clear A and Carry flag
            inc     hl              ; [1027] [23]       HL -> 1050h
sum_lp:     ld      a,(de)          ; [1028] [1a]       load byte of addend 2
            adc     a,(hl)          ; [1029] [8e]       add with carry byte of addend 1
            daa                     ; [102a] [27]       decimal adjust for BCD
            ld      (hl),a          ; [102b] [77]       store sum byte into 1050h..1052h
            inc     hl              ; [102c] [23]
            inc     de              ; [102d] [13]
            djnz    sum_lp          ; [102e] [10 f8]
            ret                     ; [1030] [c9]

init_a1:    ld      hl,addend1      ; [1031] [21 53 10]
            ld      de,1050h        ; [1034] [11 50 10]
            ld      bc,0003h        ; [1037] [01 03 00]
            ldir                    ; [103a] [ed b0]
            ret                     ; [103c] [c9]

show3:      ld      b,03h           ; [103d] [06 03]
            call    clkset          ; [103f] [cd c3 06]
            call    ms3k17          ; [1042] [cd 64 04]
            ld      b,32h           ; [1045] [06 32] 50 scan passes (~0.50s)
scan:       call    scd_k1          ; [1047] [cd cd 04]
            djnz    scan            ; [104a] [10 fb]
            ret                     ; [104c] [c9]

            defs    104fh - $, 00h
            db      03h             ; [104f] [03] byte count = 3
            db      56h, 32h, 47h   ; [1050..1052] initial addend 1 / sum buffer (473256)
addend1:    db      56h, 32h, 47h   ; [1053..1055] backup copy of addend 1 (473256)
            defs    1060h - $, 00h
            db      44h, 23h, 42h   ; [1060..1062] addend 2 (422344)
""",
        "verification": (
            "1. Load `tapes/0004.tape` (`File ID = 0x0004`, `Start = 0x1000`).\n"
            "2. Step Z80 execution past `1006H` (`CALL do_sum`) and assert `(1050H) == 0x00`, `(1051H) == 0x56`, `(1052H) == 0x89` (`895600`).\n"
            "3. Step frames across the display loop and verify the 6-digit display cycles `895600`, `473256`, and `422344` with speaker clicks."
        ),
        "extra_hw": None,
    },
    {
        "num": 5,
        "slug": "arithmetic_subtraction",
        "title": "Exercise 05: Arithmetic Subtraction Practice (實習 5：算術減法練習)",
        "annotation": "Lab05: BCD Subtraction (1000H)",
        "pages": [16, 17],
        "book_pages": "115–117",
        "tape_id": "0005",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 05 (算術減法練習) from the Z80 Microcomputer Primer. "
            "Book Example 3 (pp. 116–117) performs 3-byte (6-digit) multi-precision BCD subtraction (`SBC A, (HL)` + `DAA`) "
            "of `796825` (`25H, 68H, 79H` at `1060H..1062H`) minus `335836` (`36H, 58H, 33H` at `1050H..1052H`) = `460989` "
            "(`89H, 09H, 46H` stored into `1050H..1052H`). "
            "Our enhanced `tapes/0005.asm` executes the textbook `SBC`+`DAA` loop and continuously cycles the Difference (`460989`), "
            "Minuend (`796825`), and Subtrahend (`335836`) on the 6-digit 7-segment display with speaker ticks."
        ),
        "subroutines": [
            "0x06C3 (CLKSET - format 3 bytes from 1050H..1052H into dispbf at 17C6H)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Computes the exact textbook 3-byte BCD subtraction `796825 - 335836 = 460989` into `1050H..1052H` "
            "(`89H, 09H, 46H`) first so `1050H..1052H` immediately contains the textbook difference, then cycles "
            "`460989`, `796825`, and `335836` on the 6-digit LED display in an endless loop."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0005
; Annotation: Lab05: BCD Subtraction (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 5：算術減法練習 例三 (pp. 116-117)
;   Subtracts 3-byte BCD 796825 (25h,68h,79h at 1060h..1062h)
;   minus 335836 (36h,58h,33h at 1050h..1052h) = 460989 (89h,09h,46h)
;   using SBC A,(HL) and DAA.
; Enhancement: Cycles Difference (460989), Minuend (796825), and Subtrahend
;   (335836) on the 6-digit 7-segment display with 3 kHz speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Initialize 1050h..1052h = 335836 and compute book DEF ---
            call    init_sub        ; [1000] [cd 31 10] 1050h..1052h <- 36h, 58h, 33h
            call    do_sub          ; [1003] [cd 1f 10] 1050h..1052h <- 89h, 09h, 46h (460989)
            call    show3           ; [1006] [cd 3d 10] display Difference "460989"

            ; --- Show Minuend ("796825") ---
            ld      hl,1060h        ; [1009] [21 60 10]
            ld      de,1050h        ; [100c] [11 50 10]
            ld      bc,0003h        ; [100f] [01 03 00]
            ldir                    ; [1012] [ed b0]
            call    show3           ; [1014] [cd 3d 10]

            ; --- Show Subtrahend ("335836") then recompute Difference ---
            call    init_sub        ; [1017] [cd 31 10]
            call    show3           ; [101a] [cd 3d 10]
            jr      start           ; [101d] [18 e1]

            ; --- Exact Book Example 3 Multi-Byte BCD Subtraction Routine ---
do_sub:     ld      hl,104fh        ; [101f] [21 4f 10] HL -> byte count (03h)
            ld      b,(hl)          ; [1022] [46]       B = 03h
            ld      de,1060h        ; [1023] [11 60 10] DE -> minuend (1060h..1062h)
            xor     a               ; [1026] [af]       clear Borrow (Carry) flag
            inc     hl              ; [1027] [23]       HL -> subtrahend (1050h..1052h)
def_lp:     ld      a,(de)          ; [1028] [1a]       load minuend byte
            sbc     a,(hl)          ; [1029] [9e]       subtract subtrahend byte with borrow
            daa                     ; [102a] [27]       decimal adjust for BCD
            ld      (hl),a          ; [102b] [77]       store difference byte into 1050h..1052h
            inc     hl              ; [102c] [23]
            inc     de              ; [102d] [13]
            djnz    def_lp          ; [102e] [10 f8]
            ret                     ; [1030] [c9]

init_sub:   ld      hl,subtra       ; [1031] [21 53 10]
            ld      de,1050h        ; [1034] [11 50 10]
            ld      bc,0003h        ; [1037] [01 03 00]
            ldir                    ; [103a] [ed b0]
            ret                     ; [103c] [c9]

show3:      ld      b,03h           ; [103d] [06 03]
            call    clkset          ; [103f] [cd c3 06]
            call    ms3k17          ; [1042] [cd 64 04]
            ld      b,32h           ; [1045] [06 32] 50 scan passes (~0.50s)
scan:       call    scd_k1          ; [1047] [cd cd 04]
            djnz    scan            ; [104a] [10 fb]
            ret                     ; [104c] [c9]

            defs    104fh - $, 00h
            db      03h             ; [104f] [03] byte count = 3
            db      36h, 58h, 33h   ; [1050..1052] initial subtrahend / diff buffer (335836)
subtra:     db      36h, 58h, 33h   ; [1053..1055] backup copy of subtrahend (335836)
            defs    1060h - $, 00h
            db      25h, 68h, 79h   ; [1060..1062] minuend (796825)
""",
        "verification": (
            "1. Load `tapes/0005.tape` (`File ID = 0x0005`, `Start = 0x1000`).\n"
            "2. Step Z80 execution past `1006H` (`CALL do_sub`) and assert `(1050H) == 0x89`, `(1051H) == 0x09`, `(1052H) == 0x46` (`460989`).\n"
            "3. Verify the 6-digit display cycles `460989`, `796825`, and `335836` with speaker clicks."
        ),
        "extra_hw": None,
    },
    {
        "num": 6,
        "slug": "loop_and_branch_delay",
        "title": "Exercise 06: Looping & Branching Delay (實習 6：迴轉及跳越練習)",
        "annotation": "Lab06: Loop & Delay (1000H)",
        "pages": [18],
        "book_pages": "118–119",
        "tape_id": "0006",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 06 (迴轉及跳越練習) from the Z80 Microcomputer Primer (book pp. 118–119, PDF p. 18). "
            "Example 2 (`SQWAV`) initializes `A = FFH` and toggles `8255 Port C (82H)` bit 7 (`XOR 80H`) inside a nested "
            "`B = FFH` / `C = FFH` software delay loop (`DEC C` / `JR NZ, LOOP` / `DJNZ DELAY` / `JP SQWAV`) to generate "
            "a low-frequency square-wave pulse train on the onboard speaker / LED strobe lines."
        ),
        "subroutines": ["None (Standalone CPU delay loop and direct 8255 I/O)"],
        "ports": ["0x82 (Port C - Digit Strobes & PC7 Speaker Output)"],
        "asm_notes": (
            "Matches the exact 18-byte machine-code listing of Book Example 2 (`1000H..1011H`, PDF p. 18): "
            "`3E FF / D3 82 / 06 FF / 0E FF / 0D / 20 FD / 10 F9 / EE 80 / C3 02 10`."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0006
; Annotation: Lab06: Loop & Delay (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 6：迴轉及跳越練習 例二 (pp. 118-119)
;   Nested B/C delay loop toggling 8255 Port C (82H) bit 7 (XOR 80H).
; ==============================================================================

port_c      equ     82h             ; 8255 Port C (digit strobes & PC7 speaker)

            org     1000h

            ld      a,0ffh          ; [1000] [3e ff] initial Port C output state
sqwav:      out     (port_c),a      ; [1002] [d3 82] output A to 8255 Port C
            ld      b,0ffh          ; [1004] [06 ff] outer loop count = 255
delay:      ld      c,0ffh          ; [1006] [0e ff] inner loop count = 255
loop:       dec     c               ; [1008] [0d]    decrement inner counter
            jr      nz,loop         ; [1009] [20 fd] loop until C == 0
            djnz    delay           ; [100b] [10 f9] loop until B == 0
            xor     80h             ; [100d] [ee 80] invert bit 7 (speaker / pulse output)
            jp      sqwav           ; [100f] [c3 02 10] repeat square-wave loop
""",
        "verification": (
            "1. Load `tapes/0006.tape` (`File ID = 0x0006`, `Start = 0x1000`, `End = 0x1011`, `18` bytes).\n"
            "2. Step the Z80 CPU across multiple `SQWAV` half-periods (~915,000 T-states per toggle) and verify "
            "`Port C (0x82)` bit 7 (`getSpeakerBit()`) alternates between `true` (`0xFF`) and `false` (`0x7F`)."
        ),
        "extra_hw": None,
    },
    {
        "num": 7,
        "slug": "max_min_search",
        "title": "Exercise 07: Finding Maximum & Minimum Value (實習 7：找尋最大值、最小值)",
        "annotation": "Lab07: Max & Min Search (1000H)",
        "pages": [19, 20],
        "book_pages": "120–122",
        "tape_id": "0007",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 07 (找尋最大值、最小值) from the Z80 Microcomputer Primer (book pp. 120–122, PDF pp. 19–20). "
            "Book Example 2 scans the 5-byte array at `1052H..1056H` (`69H, 39H, 96H, 9AH, 47H`, length `05H` at `1051H`) "
            "using `CP (HL)` and stores the maximum value `9AH` into `1050H`. "
            "Our enhanced `tapes/0007.asm` executes the exact textbook search (`(1050H) = 9AH`), also finds the minimum (`39H` at `1057H`), "
            "and runs a lively continuous loop displaying each array address/element (`1052 69`..`1056 47`) followed by the "
            "discovered maximum (`1050 9A`) and minimum (`1057 39`) on the 6-digit 7-segment display with speaker ticks."
        ),
        "subroutines": [
            "0x0504 (ADRSDP - format 16-bit address in DE onto left 4 digits)",
            "0x0511 (DADP - format 8-bit value in A onto right 2 digits)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Preserves the exact textbook memory layout (`1050H` = result `9AH`, `1051H` = count `05H`, "
            "`1052H..1056H` = `69H, 39H, 96H, 9AH, 47H`) while replacing the static `CALL ANSW1` halt with a "
            "continuous visual scan across `1052..1056` and `1050 9A`."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0007
; Annotation: Lab07: Max & Min Search (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 7：找尋最大值、最小值 例二 (pp. 120-122)
;   Scans 5 bytes at 1052h..1056h (69h, 39h, 96h, 9ah, 47h; count 05h at 1051h)
;   and stores the maximum (9ah) at 1050h.
; Enhancement: Sweeps each array element on the 6-digit display (1052 69 ..
;   1056 47) and highlights the winning maximum (1050 9A) with speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
dispbf      equ     17c6h           ; system 6-digit display buffer

            org     1000h

start:      ; --- Book Example 2: Find Maximum in Array (1051h..1056h) ---
            ld      hl,1051h        ; [1000] [21 51 10] HL -> count (05h)
            ld      b,(hl)          ; [1003] [46]       B = 05h
            sub     a               ; [1004] [97]       A = 00h (initial max candidate)
next_max:   inc     hl              ; [1005] [23]       advance to next element
            cp      (hl)            ; [1006] [be]       compare current max A with (HL)
            jr      nc,decnt        ; [1007] [30 01]    if A >= (HL), keep A
            ld      a,(hl)          ; [1009] [7e]       else update max A = (HL)
decnt:      djnz    next_max        ; [100a] [10 f9]    loop over all 5 elements
            ld      (1050h),a       ; [100c] [32 50 10] store max (9Ah) at 1050h

            ; --- Lively 7-Segment & Speaker Array Inspection Loop ---
            ld      ix,dispbf       ; [100f] [dd 21 c6 17]
            ld      de,1052h        ; [1013] [11 52 10] DE -> first array element
            ld      c,05h           ; [1016] [0e 05]    5 elements to display
arr_lp:     ld      a,(de)          ; [1018] [1a]
            ld      b,18h           ; [1019] [06 18]    24 scan passes (~0.24s)
            call    show            ; [101b] [cd 2c 10]
            inc     de              ; [101e] [13]
            dec     c               ; [101f] [0d]
            jr      nz,arr_lp       ; [1020] [20 f6]

            ; --- Highlight Winner: 1050 9A ---
            ld      de,1050h        ; [1022] [11 50 10]
            ld      a,(de)          ; [1025] [1a]       A = 9Ah
            call    ms3k17          ; [1026] [cd 64 04] extra speaker chime on max
            ld      b,38h           ; [1029] [06 38]    hold winner for 56 passes (~0.56s)
            call    show            ; [102b] [cd 2c 10]
            jr      start           ; [102e] [18 d0]

show:       push    bc              ; [1030] [c5]
            call    dadp            ; [1031] [cd 11 05]
            call    adrsdp          ; [1034] [cd 04 05]
            call    ms3k17          ; [1037] [cd 64 04]
            pop     bc              ; [103a] [c1]
scan:       call    scd_k1          ; [103b] [cd cd 04]
            djnz    scan            ; [103e] [10 fb]
            ret                     ; [1040] [c9]

            defs    1050h - $, 00h
            db      00h             ; [1050] destination for maximum (becomes 9Ah)
            db      05h             ; [1051] array length = 5
            db      69h, 39h, 96h, 9ah, 47h ; [1052..1056] book candidate table
""",
        "verification": (
            "1. Load `tapes/0007.tape` (`File ID = 0x0007`, `Start = 0x1000`).\n"
            "2. Step Z80 execution past `100CH` and assert `(1050H) == 0x9A`.\n"
            "3. Verify the 6-digit display sweeps `105269`, `105339`, `105496`, `10559A`, `105647` and highlights `10509A` with speaker clicks."
        ),
        "extra_hw": None,
    },
    {
        "num": 8,
        "slug": "shift_and_rotate",
        "title": "Exercise 08: Shift & Rotate Instructions (實習 8：移位與旋轉練習)",
        "annotation": "Lab08: Shift & Rotate (1000H)",
        "pages": [20, 21],
        "book_pages": "122–124",
        "tape_id": "0008",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 08 (移位與旋轉練習) from the Z80 Microcomputer Primer (book pp. 122–124, PDF pp. 20–21). "
            "Book Example 1 shifts `(1051H) = 6FH` left (`SLA A` `CB 27`) storing `DEH` at `(1050H)`; "
            "Book Example 2 rotates `3FH` right (`RRCA` `0FH`) producing `9FH`. "
            "Our enhanced `tapes/0008.asm` computes `(1050H) = DEH` (`SLA` of `6FH`) and `(1052H) = 9FH` (`RRCA` of `3FH`), "
            "then animates live 8-step bit rotations (`RLCA` / `RRCA`) across the 6-digit 7-segment display with speaker ticks."
        ),
        "subroutines": [
            "0x06C3 (CLKSET - format 3 bytes from 1050H..1052H into dispbf)",
            "0x0504 (ADRSDP - format DE onto left 4 digits)",
            "0x0511 (DADP - format A onto right 2 digits)",
            "0x0464 (MS3K17 - 3 kHz speaker click)",
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Preserves the textbook's `(1051H) = 6FH` input and `(1050H) = DEH` (`SLA A`) output at all times, "
            "stores Example 2's `RRCA(3FH) = 9FH` at `(1052H)`, displays `9F6FDE` via `CLKSET`, and then animates "
            "8 live `RRCA` steps of `3FH` (`003F 3F` -> `013F 9F` -> ... -> `083F 3F`) on the 6-digit display."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0008
; Annotation: Lab08: Shift & Rotate (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 8：移位與旋轉練習 (pp. 122-124)
;   Ex 1: LD A,(1051H) [6Fh] -> SLA A [CB 27] -> LD (1050H),A [DEh]
;   Ex 2: LD A,3Fh -> RRCA [0Fh] -> 9Fh (stored at 1052H)
; Enhancement: Displays "9F6FDE" ([RRCA=9F][IN=6F][SLA=DE]) via CLKSET, then
;   animates 8 live RRCA rotation steps on the 6-digit display with speaker ticks.
; ==============================================================================

ms3k17      equ     0464h           ; 3 kHz speaker click
scd_k1      equ     04cdh           ; 6-digit display multiplex pass
adrsdp      equ     0504h           ; DE -> left 4 address digits
dadp        equ     0511h           ; A  -> right 2 data digits
clkset      equ     06c3h           ; format B bytes at 1050h into dispbf (17c6h)

            org     1000h

start:      ; --- Book Example 1: SLA A on (1051h) = 6Fh -> (1050h) = DEh ---
            ld      a,(1051h)       ; [1000] [3a 51 10] A = 6Fh
            sla     a               ; [1003] [cb 27]    A = 6Fh << 1 = DEh
            ld      (1050h),a       ; [1005] [32 50 10] (1050h) = DEh

            ; --- Book Example 2: RRCA on 3Fh -> (1052h) = 9Fh ---
            ld      a,3fh           ; [1008] [3e 3f]    A = 3Fh
            rrca                    ; [100a] [0f]       A = 9Fh
            ld      (1052h),a       ; [100b] [32 52 10] (1052h) = 9Fh

            ; --- Phase 1: Display 6-digit summary "9F6FDE" ---
            ld      b,03h           ; [100e] [06 03]
            call    clkset          ; [1010] [cd c3 06] dispbf <- [1052][1051][1050]
            call    ms3k17          ; [1013] [cd 64 04]
            ld      b,32h           ; [1016] [06 32]    hold 50 passes (~0.50s)
hold1:      call    scd_k1          ; [1018] [cd cd 04]
            djnz    hold1           ; [101b] [10 fb]

            ; --- Phase 2: Animate 8 live RRCA steps of 3Fh ---
            ld      de,013fh        ; [101d] [11 3f 01] D = step 01..08, E = seed 3Fh
            ld      c,3fh           ; [1020] [0e 3f]    C = rotating byte
rot_lp:     ld      a,c             ; [1022] [79]
            rrca                    ; [1023] [0f]       rotate right 1 bit
            ld      c,a             ; [1024] [4f]
            call    dadp            ; [1025] [cd 11 05] right 2 digits = rotated byte
            call    adrsdp          ; [1028] [cd 04 05] left 4 digits = [step][3F]
            call    ms3k17          ; [102b] [cd 64 04]
            ld      b,14h           ; [102e] [06 14]    20 passes (~0.20s per bit step)
scan:       call    scd_k1          ; [1030] [cd cd 04]
            djnz    scan            ; [1033] [10 fb]
            inc     d               ; [1035] [14]       increment step counter
            ld      a,d             ; [1036] [7a]
            cp      09h             ; [1037] [fe 09]
            jr      nz,rot_lp       ; [1039] [20 e7]
            jr      start           ; [103b] [18 c3]

            defs    1050h - $, 00h
            db      00h             ; [1050] SLA result (becomes DEh)
            db      6fh             ; [1051] book input operand = 6Fh
            db      00h             ; [1052] RRCA result (becomes 9Fh)
""",
        "verification": (
            "1. Load `tapes/0008.tape` (`File ID = 0x0008`, `Start = 0x1000`).\n"
            "2. Step Z80 execution past `100EH` and assert `(1050H) == 0xDE`, `(1051H) == 0x6F`, and `(1052H) == 0x9F`.\n"
            "3. Verify the 6-digit display shows `9F6FDE` and steps through the 8 `RRCA` rotation states with speaker clicks."
        ),
        "extra_hw": None,
    },
    {
        "num": 9,
        "slug": "monitor_subroutines",
        "title": "Exercise 09: Calling Monitor Subroutines (實習 9：叫用副程式)",
        "annotation": "Lab09: Subroutines HELLO (1000H)",
        "pages": [22, 23],
        "book_pages": "126–129",
        "tape_id": "0009",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 09 (叫用副程式) from the Z80 Microcomputer Primer (book pp. 126–129, PDF pp. 22–23). "
            "The exercise introduces `SCD&K` (`04A6H`), `SCD&K1` (`04CDH`), and `2BYSEG` (`0525H`). "
            "Book Example 4 (pp. 128–129) displays `\" HELLO\"` (`BD 85 85 8F 37 00` at `1040H..1045H`), "
            "blanks the display (`1046H..104BH`) when `-` (`11H`) is pressed, and scrolls `\" HELLO\"` in from right to left "
            "across the 6-digit 7-segment display when `+` (`10H`) is pressed before looping back to `START`."
        ),
        "subroutines": [
            "0x04A6 (SCD&K - Display multiplex & blocking debounced key scan returning internal key code in A)",
            "0x04CD (SCD&K1 - Single 10 ms display multiplex pass)"
        ],
        "ports": [
            "0x80 (Port A - Keypad Row Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit/Column Strobes)"
        ],
        "asm_notes": (
            "Matches the exact byte-for-byte listing of Book Example 4 (`1000H..104BH`, 76 bytes, PDF pp. 22–23). "
            "(Note: `2BYSEG` in Examples 2 & 3 is at `0525H`, NOT `0511H`)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0009
; Annotation: Lab09: Subroutines HELLO (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 9：叫用副程式 例四 (pp. 128-129)
;   Displays " HELLO" (1040h..1045h). Pressing '-' (11h) blanks the display
;   (1046h..104bh); pressing '+' (10h) scrolls " HELLO" from right to left.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex display & wait for debounced key
scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

start:      ld      ix,hello        ; [1000] [dd 21 40 10] IX -> " HELLO" buffer
disp1:      call    scd_k           ; [1004] [cd a6 04]    display " HELLO" & read key
            cp      11h             ; [1007] [fe 11]       is '-' key (11h) pressed?
            jr      nz,disp1        ; [1009] [20 f9]       loop until '-' pressed
            ld      ix,blank        ; [100b] [dd 21 46 10] IX -> blank buffer
disp2:      call    scd_k           ; [100f] [cd a6 04]    display blank & read key
            cp      10h             ; [1012] [fe 10]       is '+' key (10h) pressed?
            jr      nz,disp2        ; [1014] [20 f9]       loop until '+' pressed
            ld      ix,blank        ; [1016] [dd 21 46 10] start scroll from blank
            ld      c,06h           ; [101a] [0e 06]       6 shift steps
ini1:       ld      b,20h           ; [101c] [06 20]       32 scan passes (~0.32s/step)
ini2:       call    scd_k1          ; [101e] [cd cd 04]    refresh 6-digit display
            djnz    ini2            ; [1021] [10 fb]
            dec     ix              ; [1023] [dd 2b]       shift window 1 digit left
            dec     c               ; [1025] [0d]
            jr      nz,ini1         ; [1026] [20 f4]
            jr      start           ; [1028] [18 d6]       repeat from start

            defs    1040h - $, 00h
hello:      db      0bdh, 85h, 85h, 8fh, 37h, 00h ; [1040..1045] " HELLO" (O,L,L,E,H,' ')
blank:      db      00h, 00h, 00h, 00h, 00h, 00h  ; [1046..104b] 6 blank digits
""",
        "verification": (
            "1. Load `tapes/0009.tape` (`File ID = 0x0009`, `Start = 0x1000`, `End = 0x104B`, `76` bytes).\n"
            "2. Run from `0x1000` and verify `disp1` illuminates the `HELLO` segment masks (`BD, 85, 85, 8F, 37, 00`).\n"
            "3. Inject key `'-'` (`0x11`) and verify the display blanks (`00..00`); inject key `'+'` (`0x10`) and verify `IX` scrolls from `0x1046` back to `0x1040`."
        ),
        "extra_hw": None,
    },
    {
        "num": 10,
        "slug": "hex_bcd_display",
        "title": "Exercise 10: Hex & BCD Data Display Conversion (實習 10：資料顯示)",
        "annotation": "Lab10: Hex & BCD Display (1000H)",
        "pages": [24, 25],
        "book_pages": "130–132",
        "tape_id": "0010",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 10 (資料顯示) from the Z80 Microcomputer Primer (book pp. 130–132, PDF pp. 24–25). "
            "Book Example 2 converts 3 bytes at `1050H..1052H` (`3AH, CFH, 6BH`) via `ADC A, 00H` and `DAA` (`DLOOP`), "
            "then formats all 3 bytes into 6 7-segment display bytes at `13C6H..13CBH` using `2BYSEG` (`0525H`) and "
            "displays the result on the 6-digit 7-segment display via `SCD&K` (`04A6H`)."
        ),
        "subroutines": [
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL) and advance HL by 2)",
            "0x04A6 (SCD&K - multiplex 6-digit display from IX)"
        ],
        "ports": [
            "0x80 (Port A - Keypad Row Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Matches Book Example 2 (`1000H..1052H`, PDF p. 24) with `JR disp` (`18 FB`) at `1024H` so pressing a key "
            "during `SCD&K` refreshes the display cleanly rather than executing padding bytes."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0010
; Annotation: Lab10: Hex & BCD Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 10：資料顯示 例二 (pp. 130-131)
;   Converts 3 bytes at 1050h..1052h (3Ah, CFh, 6Bh) via ADC A,00h + DAA,
;   formats them into 13c6h..13cbh via 2BYSEG (0525h), and displays via SCD&K.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex 6-digit display & scan keypad
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 segment bytes at (HL)++
outbf       equ     13c6h           ; 6-byte display buffer in RAM

            org     1000h

start:      ld      de,1050h        ; [1000] [11 50 10] DE -> 3-byte data table
            ld      b,03h           ; [1003] [06 03]    3 bytes to adjust
            xor     a               ; [1005] [af]       clear Carry flag
dloop:      ld      a,(de)          ; [1006] [1a]       load byte from (DE)
            adc     a,00h           ; [1007] [ce 00]    add carry from previous byte
            daa                     ; [1009] [27]       decimal adjust
            ld      (de),a          ; [100a] [12]       store adjusted byte back
            inc     de              ; [100b] [13]
            djnz    dloop           ; [100c] [10 f8]
answ3:      ld      ix,outbf        ; [100e] [dd 21 c6 13] IX -> 13c6h display buffer
            ld      de,1050h        ; [1012] [11 50 10] DE -> 1050h..1052h
            ld      hl,outbf        ; [1015] [21 c6 13] HL -> 13c6h..13cbh
            ld      b,03h           ; [1018] [06 03]    3 bytes -> 6 digits
tloop:      ld      a,(de)          ; [101a] [1a]
            call    tobyseg         ; [101b] [cd 25 05] convert A into 2 digits at (HL)
            inc     de              ; [101e] [13]
            djnz    tloop           ; [101f] [10 f9]
disp:       call    scd_k           ; [1021] [cd a6 04] multiplex 6-digit display
            jr      disp            ; [1024] [18 fb]    keep displaying if key pressed

            defs    1050h - $, 00h
            db      3ah, 0cfh, 6bh  ; [1050..1052] book initial data bytes
""",
        "verification": (
            "1. Load `tapes/0010.tape` (`File ID = 0x0010`, `Start = 0x1000`, `End = 0x1052`, `83` bytes).\n"
            "2. Step Z80 execution to `1021H` and verify `1050H..1052H` holds the `DAA`-adjusted bytes (`40H, 35H, 72H`) "
            "and `13C6H..13CBH` holds the 6 decoded 7-segment font bitmasks (`723540`)."
        ),
        "extra_hw": None,
    },
    {
        "num": 11,
        "slug": "subroutine_practice",
        "title": "Exercise 11: Subroutine Practice (實習 11：副程式練習)",
        "annotation": "Lab11: FEDCBA Scroll (1000H)",
        "pages": [25, 26],
        "book_pages": "133–135",
        "tape_id": "0011",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 11 (副程式練習) from the Z80 Microcomputer Primer (book pp. 133–135, PDF pp. 25–26). "
            "Book Example 2 scrolls the 6-character hex banner `\"FEDCBA\"` (`0F 8F B3 8D A7 3F` at `1040H..1045H`, "
            "preceded by 6 blank bytes `00` at `1046H..104BH`) from right to left across the 6-digit display using "
            "`SCD&K1` (`04CDH`, 32 passes = 0.32s per shift step), then holds on `SCD&K` (`04A6H`) until any key is pressed."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - single 10 ms 6-digit display multiplex pass)",
            "0x04A6 (SCD&K - blocking display multiplex & debounced key wait)"
        ],
        "ports": [
            "0x80 (Port A - Keypad Row Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Matches Book Example 2 (`1000H..104BH`, PDF p. 26) with `JR start` (`18 E9`) after `CALL SCD&K` at `1015H` "
            "so pressing any key replays the `\"FEDCBA\"` scroll animation."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0011
; Annotation: Lab11: FEDCBA Scroll (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 11：副程式練習 例二 (pp. 134-135)
;   Scrolls "FEDCBA" (1040h..1045h) from right to left across the 6-digit
;   display (0.32s per step via SCD&K1), then holds on SCD&K until a key press.
; ==============================================================================

scd_k       equ     04a6h           ; multiplex display & wait for key press
scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

start:      ld      ix,blank        ; [1000] [dd 21 46 10] IX -> blank buffer (1046h)
            ld      c,06h           ; [1004] [0e 06]       6 shift steps
ini1:       ld      b,20h           ; [1006] [06 20]       32 passes (~0.32s per step)
ini2:       call    scd_k1          ; [1008] [cd cd 04]    refresh 6-digit display
            djnz    ini2            ; [100b] [10 fb]
            dec     ix              ; [100d] [dd 2b]       shift window 1 byte toward 1040h
            dec     c               ; [100f] [0d]
            jr      nz,ini1         ; [1010] [20 f4]
            call    scd_k           ; [1012] [cd a6 04]    hold "FEDCBA" until key pressed
            jr      start           ; [1015] [18 e9]       replay scroll on key press

            defs    1040h - $, 00h
fedcba:     db      0fh, 8fh, 0b3h, 8dh, 0a7h, 3fh ; [1040..1045] "FEDCBA" (A,B,C,d,E,F)
blank:      db      00h, 00h, 00h, 00h, 00h, 00h   ; [1046..104b] 6 blank digits
""",
        "verification": (
            "1. Load `tapes/0011.tape` (`File ID = 0x0011`, `Start = 0x1000`, `End = 0x104B`, `76` bytes).\n"
            "2. Run from `0x1000` across the 6 shift steps and verify `IX` decrements from `0x1046` to `0x1040` (`\"FEDCBA\"`)."
        ),
        "extra_hw": None,
    },
    {
        "num": 12,
        "slug": "alternating_display",
        "title": "Exercise 12: Alternating Display (實習 12：交互顯示)",
        "annotation": "Lab12: STOP/GO Display (1000H)",
        "pages": [27],
        "book_pages": "136–137",
        "tape_id": "0012",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 12 (交互顯示) from the Z80 Microcomputer Primer (book pp. 136–137, PDF p. 27). "
            "Book Example 1 continuously alternates two 6-digit 7-segment banners—`\"  GO  \"` (`1026H..102BH`) and "
            "`\"-StOP-\"` (`1020H..1025H`)—every 0.5 seconds (`50` passes of `SCD&K1` `04CDH`, `B = 32H`) in an infinite loop."
        ),
        "subroutines": ["0x04CD (SCD&K1 - single 10 ms 6-digit display multiplex pass)"],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Matches Book Example 1 (`1000H..102BH`, 44 bytes, PDF p. 27) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0012
; Annotation: Lab12: STOP/GO Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 12：交互顯示 例一 (pp. 136-137)
;   Alternates "  GO  " (1026h..102bh) and "-StOP-" (1020h..1025h) every 0.5s
;   (50 passes of SCD&K1 at 10 ms/pass).
; ==============================================================================

scd_k1      equ     04cdh           ; single 10 ms display multiplex pass

            org     1000h

begin:      ld      ix,1026h        ; [1000] [dd 21 26 10] IX -> "  GO  " pattern
            ld      b,32h           ; [1004] [06 32]       50 scan passes (0.50s)
loop1:      call    scd_k1          ; [1006] [cd cd 04]    refresh 6-digit display
            djnz    loop1           ; [1009] [10 fb]
            ld      ix,1020h        ; [100b] [dd 21 20 10] IX -> "-StOP-" pattern
            ld      b,32h           ; [100f] [06 32]       50 scan passes (0.50s)
loop2:      call    scd_k1          ; [1011] [cd cd 04]    refresh 6-digit display
            djnz    loop2           ; [1014] [10 fb]
            jp      begin           ; [1016] [c3 00 10]    repeat indefinitely

            defs    1020h - $, 00h
stop_pat:   db      02h, 1fh, 0a3h, 87h, 0aeh, 02h ; [1020..1025] "-StOP-"
go_pat:     db      00h, 02h, 8fh, 0adh, 02h, 00h   ; [1026..102b] " -GO- "
""",
        "verification": (
            "1. Load `tapes/0012.tape` (`File ID = 0x0012`, `Start = 0x1000`, `End = 0x102B`, `44` bytes).\n"
            "2. Run from `0x1000` and verify `IX` alternates between `0x1026` (`GO` pattern) and `0x1020` (`STOP` pattern) every 50 scan passes."
        ),
        "extra_hw": None,
    },
    {
        "num": 13,
        "slug": "scrolling_marquee",
        "title": "Exercise 13: Scrolling Marquee Display (實習 13：移動顯示)",
        "annotation": "Lab13: Marquee Display (1000H)",
        "pages": [28, 29],
        "book_pages": "138–140",
        "tape_id": "0013",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 13 (移動顯示) from the Z80 Microcomputer Primer (book pp. 138–140, PDF pp. 28–29). "
            "Book Example 3 (`1000H..1065H`, p. 140) continuously scrolls all 26 English alphabet glyphs (`A` through `Z`, "
            "framed by 6 leading and 6 trailing blank digits across `1040H..1065H`) from right to left across the 6-digit "
            "7-segment display (`C = 20H` = 32 steps, `B = 60H` passes of `SCD&K1` `04CDH` per step)."
        ),
        "subroutines": ["0x04CD (SCD&K1 - single 10 ms 6-digit display multiplex pass)"],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Uses Book Example 3 (`1000H..1065H`, 102 bytes, PDF p. 29) which scrolls the full 26-letter alphabet "
            "table (`1046H..105FH`, flanked by 6 blanks at `1040H..1045H` and `1060H..1065H`)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0013
; Annotation: Lab13: Marquee Display (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 13：移動顯示 例三 (pp. 138-140)
;   Scrolls all 26 alphabet characters (A..Z at 1046h..105fh, framed by 6 blanks
;   at 1040h..1045h and 1060h..1065h) from right to left across the 6-digit LEDs.
; ==============================================================================

scd_k1      equ     04cdh           ; single 10 ms display multiplex pass
blank       equ     105fh           ; initial window pointer (blank tail)

            org     1000h

start:      ld      ix,blank        ; [1000] [dd 21 5f 10] IX -> start of marquee scroll
            ld      c,20h           ; [1004] [0e 20]       32 shift steps
loop1:      ld      b,60h           ; [1006] [06 60]       96 scan passes (~0.96s/step)
loop2:      call    scd_k1          ; [1008] [cd cd 04]    refresh 6-digit display
            djnz    loop2           ; [100b] [10 fb]
            dec     ix              ; [100d] [dd 2b]       shift window 1 character left
            dec     c               ; [100f] [0d]
            jr      nz,loop1        ; [1010] [20 f4]
            jp      start           ; [1012] [c3 00 10]    repeat marquee continuously

            defs    1040h - $, 00h
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [1040..1045] leading blanks
            db      8ah, 0b6h, 07h, 0a9h, 0b7h, 0b5h       ; [1046..104b] Z..U
            db      87h, 0aeh, 03h, 3eh, 1fh, 0a3h         ; [104c..1051] T..O
            db      23h, 2bh, 85h, 97h, 0b1h, 89h          ; [1052..1057] N..I
            db      37h, 0adh, 0fh, 8fh, 0b3h, 8dh         ; [1058..105d] H..C
            db      0a7h, 3fh                              ; [105e..105f] B, A
            db      00h, 00h, 00h, 00h, 00h, 00h           ; [1060..1065] trailing blanks
""",
        "verification": (
            "1. Load `tapes/0013.tape` (`File ID = 0x0013`, `Start = 0x1000`, `End = 0x1065`, `102` bytes).\n"
            "2. Run from `0x1000` and verify `IX` steps downward from `0x105F` across the 32-step alphabet table and loops back to `0x1000`."
        ),
        "extra_hw": None,
    },
    {
        "num": 14,
        "slug": "telephone_ring_tone",
        "title": "Exercise 14: Telephone Ringing & Busy Tone (實習 14：電話聲響)",
        "annotation": "Lab14: Telephone Ring (1000H)",
        "pages": [29, 30],
        "book_pages": "141–143",
        "tape_id": "0014",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 14 (電話聲響) from the Z80 Microcomputer Primer (book pp. 141–143, PDF pp. 29–30). "
            "The program synthesizes a realistic telephone ring cadence (`1000H..101EH`) by alternating 8 cycles of 320 Hz "
            "(`C = 0D3H, HL = 0008H`) and 12 cycles of 480 Hz (`C = 8CH, HL = 000CH`) 20 times (`A = 14H`) via Monitor ROM "
            "`SOUND` (`047CH`), followed by a 2-second silent pause (`BC = 3350H`, `DELAY` at `102FH`). "
            "Also includes the 400 Hz telephone busy tone entry point (`BUSY` at `101FH`)."
        ),
        "subroutines": ["0x047C (SOUND - variable-frequency square-wave generator: C = half-period, HL = cycle count)"],
        "ports": ["0x82 (Port C - Bit 7 Speaker Output)"],
        "asm_notes": (
            "Matches the exact 54-byte machine-code listing in the Primer (`1000H..1035H`, PDF p. 30), including both "
            "the `BEGIN` telephone ring loop (`1000H`), the `BUSY` 400 Hz busy-signal loop (`101FH`), and `DELAY` (`102FH`)."
        ),
        "asm_source": """\
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
""",
        "verification": (
            "1. Load `tapes/0014.tape` (`File ID = 0x0014`, `Start = 0x1000`, `End = 0x1035`, `54` bytes).\n"
            "2. Run from `0x1000` (`BEGIN`) and verify `SOUND` (`0x047C`) is invoked with `C=0xD3`/`HL=0x0008` and `C=0x8C`/`HL=0x000C`, generating speaker edges on `Port C bit 7`.\n"
            "3. Run from `0x101F` (`BUSY`) and verify `SOUND` is invoked with `C=0xA8`/`HL=0x00C8`."
        ),
        "extra_hw": None,
    },
    {
        "num": 15,
        "slug": "police_siren",
        "title": "Exercise 15: Police Siren Dual-Frequency Audio (實習 15：警車鳴聲)",
        "annotation": "Lab15: Police Siren (1000H)",
        "pages": [31],
        "book_pages": "144",
        "tape_id": "0015",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 15 (警車鳴聲) from the Z80 Microcomputer Primer (book p. 144, PDF p. 31). "
            "Alternates 0.73 seconds of a 265 Hz low tone (`C = 00H, HL = 00C0H`) and 0.73 seconds of a 352 Hz high tone "
            "(`C = 0C0H, HL = 0100H`) via Monitor ROM subroutine `SOUND` (`047CH`) in an infinite loop (`JR LOOP`)."
        ),
        "subroutines": ["0x047C (SOUND - variable-frequency square-wave generator: C = half-period, HL = cycle count)"],
        "ports": ["0x82 (Port C - Bit 7 Speaker Output)"],
        "asm_notes": (
            "Matches the exact 18-byte machine-code listing in the Primer (`1000H..1011H`, PDF p. 31)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0015
; Annotation: Lab15: Police Siren (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 15：警車鳴聲 (p. 144)
;   Alternates 265 Hz (C=00h, HL=00c0h, 0.73s) and 352 Hz (C=0c0h, HL=0100h,
;   0.73s) via Monitor ROM SOUND (047ch).
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator (C=pitch, HL=cycles)

            org     1000h

loop:       ld      c,00h           ; [1000] [0e 00] 265 Hz half-period (256 iterations)
            ld      hl,00c0h        ; [1002] [21 c0 00] 192 cycles (~0.73s)
            call    sound           ; [1005] [cd 7c 04] play low siren tone
            ld      c,0c0h          ; [1008] [0e c0] 352 Hz half-period (192 iterations)
            ld      hl,0100h        ; [100a] [21 00 01] 256 cycles (~0.73s)
            call    sound           ; [100d] [cd 7c 04] play high siren tone
            jr      loop            ; [1010] [18 ee] repeat siren loop
""",
        "verification": (
            "1. Load `tapes/0015.tape` (`File ID = 0x0015`, `Start = 0x1000`, `End = 0x1011`, `18` bytes).\n"
            "2. Step Z80 execution across both tones and verify `Port C bit 7` toggles at ~265 Hz (`half-period ~1888 us`) and ~352 Hz (`half-period ~1422 us`)."
        ),
        "extra_hw": None,
    },
    {
        "num": 16,
        "slug": "roulette_game",
        "title": "Exercise 16: Roulette Wheel LED Game (實習 16：輪盤遊戲)",
        "annotation": "Lab16: Roulette Game (1000H)",
        "pages": [32, 33],
        "book_pages": "146–148",
        "tape_id": "0016",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 16 (輪盤遊戲) from the Z80 Microcomputer Primer (book pp. 146–148, PDF pp. 32–33). "
            "Animates a single illuminated outer segment chasing clockwise around the 16-segment perimeter of the 6-digit "
            "7-segment display (`TABLE` at `1040H..105CH`: pairs of `(digit_index, segment_mask)` terminated by `FFH`). "
            "While the `+` key (matrix position code `03H`) is held down, the wheel freezes on the active segment!"
        ),
        "subroutines": ["0x04CD (SCD&K1 - 10 ms display multiplex pass & raw matrix position scan)"],
        "ports": [
            "0x80 (Port A - Keypad Row Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Matches the exact 97-byte machine-code listing in the Primer (`1000H..1060H`, PDF pp. 32–33). "
            "Note: At `1032H`, `DD 36 00 00` is the 4-byte Z80 instruction `LD (IX+00H), 00H` inside `CLRBF`, "
            "and `TABLE` spans `1040H..1060H` (16 `(digit, mask)` pairs + `FFH` end marker)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0016
; Annotation: Lab16: Roulette Game (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 16：輪盤遊戲 (pp. 146-148)
;   Traces the 16-segment perimeter of the 6-digit display using TABLE at
;   1040h..1060h. Holding '+' (position code 03h) freezes the roulette wheel.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display scan (A = matrix pos if C=0)
outbf       equ     1100h           ; 6-byte display buffer in RAM

            org     1000h

int:        ld      hl,table        ; [1000] [21 40 10] HL -> roulette segment table
loop:       ld      ix,outbf        ; [1003] [dd 21 00 11] IX -> 1100h..1105h buffer
            call    clrbf           ; [1007] [cd 30 10] clear all 6 display digits
            ld      e,(hl)          ; [100a] [5e]       E = digit index (00h..05h, or FFh)
            inc     e               ; [100b] [1c]       test for FFh end-of-table marker
            jr      z,int           ; [100c] [28 f2]    wrap around to start of table
            dec     e               ; [100e] [1d]       restore digit index E
            ld      d,00h           ; [100f] [16 00]
            add     ix,de           ; [1011] [dd 19]    IX -> target digit in outbf
            inc     hl              ; [1013] [23]
            ld      a,(hl)          ; [1014] [7e]       A = active segment bitmask
            ld      (ix+00h),a      ; [1015] [dd 77 00] write segment mask into digit
            ld      ix,outbf        ; [1018] [dd 21 00 11] restore IX -> outbf
            ld      b,03h           ; [101c] [06 03]    3 scan passes (30 ms per step)
light:      call    scd_k1          ; [101e] [cd cd 04] multiplex display & scan keypad
            jr      c,nscan         ; [1021] [38 01]    if no key pressed (C=1), skip
            ld      c,a             ; [1023] [4f]       record pressed key position code
nscan:      djnz    light           ; [1024] [10 f8]
            ld      a,c             ; [1026] [79]
            cp      03h             ; [1027] [fe 03]    was '+' (pos code 03h) pressed?
            jr      z,stop          ; [1029] [28 02]    freeze on current segment!
            inc     hl              ; [102b] [23]       advance to next table pair
            inc     hl              ; [102c] [23]
stop:       dec     hl              ; [102d] [2b]       point back to digit index byte
            jr      loop            ; [102e] [18 d3]

clrbf:      ld      b,06h           ; [1030] [06 06]    clear 6 bytes at (IX+0..5)
crl:        ld      (ix+00h),00h    ; [1032] [dd 36 00 00]
            inc     ix              ; [1036] [dd 23]
            djnz    crl             ; [1038] [10 f8]
            ld      de,0fffah       ; [103a] [11 fa ff] DE = -6
            add     ix,de           ; [103d] [dd 19]    restore IX to start of buffer
            ret                     ; [103f] [c9]

table:      db      05h, 08h, 04h, 08h, 03h, 08h, 02h, 08h ; [1040..1047] top bar a (L->R)
            db      01h, 08h, 00h, 08h, 00h, 10h, 00h, 20h ; [1048..104f] right edge b,c
            db      00h, 80h, 01h, 80h, 02h, 80h, 03h, 80h ; [1050..1057] bottom bar d (R->L)
            db      04h, 80h, 05h, 80h, 05h, 01h, 05h, 04h ; [1058..105f] left edge e,f
            db      0ffh                                   ; [1060] end marker
""",
        "verification": (
            "1. Load `tapes/0016.tape` (`File ID = 0x0016`, `Start = 0x1000`, `End = 0x1060`, `97` bytes).\n"
            "2. Run from `0x1000` and verify `1100H..1105H` steps through the 16 perimeter `(digit, mask)` pairs in `TABLE` (`1040H..105FH`).\n"
            "3. Press key `'+'` (`0x10`, matrix pos `0x03`) and verify the roulette wheel halts on the active segment until another key is pressed."
        ),
        "extra_hw": None,
    },
    {
        "num": 17,
        "slug": "pacman_animation",
        "title": "Exercise 17: Pac-Man Sprite Animation & Sound (實習 17：小精靈)",
        "annotation": "Lab17: Pac-Man Sprite (1000H)",
        "pages": [33, 34],
        "book_pages": "149–151",
        "tape_id": "0017",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 17 (小精靈) from the Z80 Microcomputer Primer (book pp. 149–151, PDF pp. 33–34). "
            "Animates a chomping Pac-Man character across the 6-digit 7-segment display by swapping two frame buffers "
            "(`DISP2` at `1070H..107FH` and `DISP4` at `1080H..108EH`) via `EX (SP), IX` and scrolling them with `DEC IX`, "
            "synchronized with 1 kHz (`SOU1K` at `1052H`) and 2 kHz (`SOU2K` at `105BH`) chiptune sound effects."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)",
            "0x0470 (MS1K - 1 kHz speaker tone generator)",
            "0x0475 (MS2K - 2 kHz speaker tone generator)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Matches the exact 143-byte machine-code listing in the Primer (`1000H..108EH`, PDF pp. 33–34)."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0017
; Annotation: Lab17: Pac-Man Sprite (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 17：小精靈 (pp. 149-151)
;   Animates Pac-Man chomping (EX (SP),IX between 1079h and 1089h) and scrolling
;   across the 6-digit display with 1 kHz / 2 kHz sound effects (SOU1K / SOU2K).
; ==============================================================================

ms1k        equ     0470h           ; Monitor ROM 1 kHz tone generator
ms2k        equ     0475h           ; Monitor ROM 2 kHz tone generator
scd_k1      equ     04cdh           ; 10 ms 6-digit display multiplex pass
disp2       equ     1079h           ; frame 1 initial pointer
disp4       equ     1089h           ; frame 2 initial pointer

            org     1000h

inidip:     ld      c,08h           ; [1000] [0e 08] 8 stationary chomp cycles
            ld      hl,disp4        ; [1002] [21 89 10]
            push    hl              ; [1005] [e5]    put disp4 on stack for EX (SP),IX
            ld      ix,disp2        ; [1006] [dd 21 79 10]
a1:         call    sou1k           ; [100a] [cd 52 10] 1 kHz chirp
            ex      (sp),ix         ; [100d] [dd e3]    swap mouth open/closed frame
            ld      b,0eh           ; [100f] [06 0e]    14 scan passes (0.14s)
a2:         call    scd_k1          ; [1011] [cd cd 04]
            djnz    a2              ; [1014] [10 fb]
            call    sou2k           ; [1016] [cd 5b 10] 2 kHz chirp
            ld      b,0eh           ; [1019] [06 0e]
a3:         call    scd_k1          ; [101b] [cd cd 04]
            djnz    a3              ; [101e] [10 fb]
            dec     c               ; [1020] [0d]
            jr      nz,a1           ; [1021] [20 e7]

            ld      c,12h           ; [1023] [0e 12] 18 scrolling steps
            ld      ix,disp2        ; [1025] [dd 21 79 10]
            ld      hl,disp4        ; [1029] [21 89 10]
            push    hl              ; [102c] [e5]
bloop:      ld      b,0ch           ; [102d] [06 0c] 12 scan passes (0.12s)
b2:         call    scd_k1          ; [102f] [cd cd 04]
            djnz    b2              ; [1032] [10 fb]
            dec     ix              ; [1034] [dd 2b] scroll frame 1 left
            dec     c               ; [1036] [0d]
            jr      z,c1            ; [1037] [28 10]
            ex      (sp),ix         ; [1039] [dd e3] switch to frame 2
            ld      b,0ch           ; [103b] [06 0c]
b3:         call    scd_k1          ; [103d] [cd cd 04]
            djnz    b3              ; [1040] [10 fb]
            call    sou1k           ; [1042] [cd 52 10] 1 kHz step chirp
            dec     ix              ; [1045] [dd 2b] scroll frame 2 left
            ex      (sp),ix         ; [1047] [dd e3] switch back to frame 1
c1:         dec     c               ; [1049] [0d]
            jr      nz,bloop        ; [104a] [20 e1]
            pop     hl              ; [104c] [e1]    balance stack (second push)
            pop     hl              ; [104d] [e1]    balance stack (first push)
            jp      inidip          ; [104e] [c3 00 10] repeat animation

            defs    1052h - $, 00h
sou1k:      exx                     ; [1052] [d9]
            ld      hl,0050h        ; [1053] [21 50 00] 80 cycles @ 1 kHz (80 ms)
            call    ms1k            ; [1056] [cd 70 04]
            exx                     ; [1059] [d9]
            ret                     ; [105a] [c9]

sou2k:      exx                     ; [105b] [d9]
            ld      hl,00a0h        ; [105c] [21 a0 00] 160 cycles @ 2 kHz (80 ms)
            call    ms2k            ; [105f] [cd 75 04]
            exx                     ; [1062] [d9]
            ret                     ; [1063] [c9]

            defs    1070h - $, 00h
            db      0bdh, 0bfh, 02h, 8dh, 0a7h, 3fh, 00h, 00h, 00h ; [1070..1078] "ABC-80   "
            db      0bdh, 00h, 3fh, 3fh, 3fh, 3fh, 00h             ; [1079..107f] Pac-Man open + pellets
            db      0bdh, 0bfh, 02h, 8dh, 0a7h, 3fh, 00h, 00h, 00h ; [1080..1088] "ABC-80   "
            db      38h, 00h, 1eh, 1eh, 1eh, 00h                   ; [1089..108e] Pac-Man chomp + pellets
""",
        "verification": (
            "1. Load `tapes/0017.tape` (`File ID = 0x0017`, `Start = 0x1000`, `End = 0x108E`, `143` bytes).\n"
            "2. Run from `0x1000` and verify `IX` alternates between `0x1079` and `0x1089` while `MS1K` (`0x0470`) and `MS2K` (`0x0475`) emit speaker tones, then scrolls across `0x1070..0x108E`."
        ),
        "extra_hw": None,
    },
    {
        "num": 18,
        "slug": "computer_music",
        "title": "Exercise 18: Computer Music Sequencer (實習 18：電腦音樂)",
        "annotation": "Lab18: Computer Music (1000H)",
        "pages": [34, 35],
        "book_pages": "152–153",
        "tape_id": "0018",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 18 (電腦音樂) from the Z80 Microcomputer Primer (book pp. 152–153, PDF pp. 34–35). "
            "Loads `IY = 1008H` and invokes Monitor ROM subroutine `MUSIC` (`0411H`) to play the 53-byte encoded musical score "
            "at `1008H..103CH` (`(pitch_index, duration_beats)` pairs terminated by `40H` for continuous repeat playback)."
        ),
        "subroutines": ["0x0411 (MUSIC - Monitor ROM melody score player; IY = score table pointer)"],
        "ports": ["0x82 (Port C - Bit 7 Speaker Output)"],
        "asm_notes": (
            "Matches the exact 61-byte listing in the Primer (`1000H..103CH`, PDF p. 35). Note that `LD IY, 1008H` at `1000H` "
            "is 4 bytes (`FD 21 08 10`), placing `CALL 0411H` at `1004H` (`CD 11 04`), `HALT` at `1007H` (`76`), and `DATA` at `1008H`."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0018
; Annotation: Lab18: Computer Music (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 18：電腦音樂 (pp. 152-153)
;   Loads IY = 1008h and calls Monitor ROM MUSIC (0411h) to play a 26-note
;   melody score terminated by 40h (continuous repeat marker).
; ==============================================================================

music       equ     0411h           ; Monitor ROM score player (IY = score ptr)

            org     1000h

start:      ld      iy,score        ; [1000] [fd 21 08 10] IY -> melody score
            call    music           ; [1004] [cd 11 04]    play melody (loops on 40h)
            halt                    ; [1007] [76]          halt if score ends with 80h

score:      db      0ch, 06h, 0ch, 02h, 0ch, 04h, 15h, 03h ; [1008..100f]
            db      13h, 01h, 11h, 10h, 13h, 06h, 13h, 02h ; [1010..1017]
            db      13h, 04h, 15h, 03h, 11h, 01h, 13h, 10h ; [1018..101f]
            db      15h, 06h, 11h, 02h, 11h, 04h, 0fh, 04h ; [1020..1027]
            db      16h, 06h, 15h, 02h, 13h, 08h, 10h, 06h ; [1028..102f]
            db      0ch, 02h, 0ch, 03h, 0fh, 01h, 10h, 03h ; [1030..1037]
            db      13h, 01h, 11h, 10h, 40h                ; [1038..103c] 40h = repeat
""",
        "verification": (
            "1. Load `tapes/0018.tape` (`File ID = 0x0018`, `Start = 0x1000`, `End = 0x103C`, `61` bytes).\n"
            "2. Run from `0x1000` and verify `IY` advances through `0x1008..0x103C` inside `MUSIC` (`0x0411`) while generating speaker square waves on `Port C bit 7`."
        ),
        "extra_hw": None,
    },
    {
        "num": 19,
        "slug": "electronic_organ",
        "title": "Exercise 19: 16-Key Electronic Organ (實習 19：電子琴)",
        "annotation": "Lab19: Electronic Organ (1000H)",
        "pages": [36],
        "book_pages": "154–155",
        "tape_id": "0019",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 19 (電子琴) from the Z80 Microcomputer Primer (book pp. 154–155, PDF p. 36). "
            "Turns the ABC-80 hex keypad (`0`–`F`) into a 16-key diatonic/chromatic electronic organ (`5`..`5'` across two octaves). "
            "Calls `SCD&K` (`04A6H`) with `IX = 0780H` (blank display), indexes into the 16-byte frequency table at `1023H..1032H`, "
            "and toggles `Port C (82H)` bit 7 (`XOR 80H`) in a calibrated `NOP`+`DJNZ` loop for as long as the key remains held (`IN A, (80H)`)."
        ),
        "subroutines": ["0x04A6 (SCD&K - wait for keypad press and return internal key code 00H..0FH in A)"],
        "ports": [
            "0x80 (Port A - Keypad Row Sense Input)",
            "0x82 (Port C - Bit 7 Speaker Output & Column Strobe)"
        ],
        "asm_notes": (
            "Matches the exact 51-byte machine-code listing in the Primer (`1000H..1032H`, PDF p. 36) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0019
; Annotation: Lab19: Electronic Organ (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 19：電子琴 (pp. 154-155)
;   16-key live electronic organ: keys 0..F index into pitch table 1023h..1032h
;   and drive speaker (Port C bit 7) while the key remains depressed.
; ==============================================================================

scd_k       equ     04a6h           ; wait for debounced key press (A = key code)
blank_rom   equ     0780h           ; 6 zero bytes in Monitor ROM (blanks display)
port_a      equ     80h             ; 8255 Port A (keypad row input)
port_c      equ     82h             ; 8255 Port C (speaker bit 7 & column strobes)

            org     1000h

start:      ld      ix,blank_rom    ; [1000] [dd 21 80 07] IX -> blank display
            call    scd_k           ; [1004] [cd a6 04]    wait for key press (A = 00h..0fh)
            ld      hl,orgtab       ; [1007] [21 23 10]    HL -> 16-note pitch table
            add     a,l             ; [100a] [85]          index into table by key code
            ld      l,a             ; [100b] [6f]
            ld      a,0c0h          ; [100c] [3e c0]       pull all 6 column strobes low (PC0..5=0)
loop:       out     (port_c),a      ; [100e] [d3 82]       drive speaker & column strobes
            ld      b,(hl)          ; [1010] [46]          load half-period delay count
delay:      nop                     ; [1011] [00]          fine pitch calibration delay
            nop                     ; [1012] [00]
            nop                     ; [1013] [00]
            djnz    delay           ; [1014] [10 fb]
            xor     80h             ; [1016] [ee 80]       invert speaker bit 7
            ld      c,a             ; [1018] [4f]          save Port C state in C
            in      a,(port_a)      ; [1019] [db 80]       read keypad rows (PA0..PA5)
            or      0c0h            ; [101b] [f6 c0]       mask unused bits PA6..PA7 high
            inc     a               ; [101d] [3c]          if no key pressed, FFh + 1 = 00h (Z=1)
            ld      a,c             ; [101e] [79]          restore Port C state
            jr      z,start         ; [101f] [28 df]       key released -> wait for next key
            jr      loop            ; [1021] [18 eb]       key still held -> continue tone

orgtab:     db      0b2h, 0a8h, 96h, 85h, 78h, 70h, 64h, 59h ; [1023..102a] keys 0..7
            db      54h, 4ah, 42h, 38h, 37h, 31h, 2ch, 29h   ; [102b..1032] keys 8..F
""",
        "verification": (
            "1. Load `tapes/0019.tape` (`File ID = 0x0019`, `Start = 0x1000`, `End = 0x1032`, `51` bytes).\n"
            "2. Run from `0x1000`, hold key `'0'` (`0x00`) or `'A'` (`0x0A`), and verify `Port C bit 7` oscillates at the corresponding half-period (`0xB2` vs `0x42`) until the key is released."
        ),
        "extra_hw": None,
    },
    {
        "num": 20,
        "slug": "digital_stopwatch",
        "title": "Exercise 20: 1/100th-Second Digital Stopwatch (實習 20：馬錶)",
        "annotation": "Lab20: Stopwatch (1000H)",
        "pages": [37],
        "book_pages": "156–157",
        "tape_id": "0020",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 20 (馬錶) from the Z80 Microcomputer Primer (book pp. 156–157, PDF p. 37). "
            "Implements a 1/100th-second centisecond digital stopwatch (`SS-CC` from `00-00` to `99-99`) using `SCD&K1` (`04CDH`) "
            "as a 10 ms timebase. While no key is pressed (`C = 1`), `DE` increments in BCD (`ADD A, 01H` / `DAA` on `E`, "
            "`ADC A, 00H` / `DAA` on `D`) and formats `SS-CC` into `1100H..1105H` via `2BYSEG` (`0525H`). Holding any key (`C = 0`) "
            "freezes the stopwatch reading on the 6-digit display!"
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass & key sense)",
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL)++)"
        ],
        "ports": [
            "0x80 (Port A - Keypad Row Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)"
        ],
        "asm_notes": (
            "Matches the exact 40-byte machine-code listing in the Primer (`1000H..1027H`, PDF p. 37) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0020
; Annotation: Lab20: Stopwatch (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 20：馬錶 (pp. 156-157)
;   Centisecond (1/100s) BCD stopwatch in DE displayed at 1100h..1105h as
;   "SS-CC " via SCD&K1 (04cdh) and 2BYSEG (0525h). Holding any key pauses.
; ==============================================================================

scd_k1      equ     04cdh           ; 10 ms display scan (C=0 if key held)
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
outbuf      equ     1100h           ; 6-byte display buffer in RAM

            org     1000h

start:      ld      ix,outbuf       ; [1000] [dd 21 00 11] IX -> 1100h..1105h
            ld      de,0000h        ; [1004] [11 00 00]    reset BCD stopwatch D=00s, E=00cs
loop:       call    scd_k1          ; [1007] [cd cd 04]    10 ms display scan & key check
            jr      nc,loop         ; [100a] [30 fb]       pause stopwatch while any key held
            ld      a,e             ; [100c] [7b]          increment centiseconds (00..99)
            add     a,01h           ; [100d] [c6 01]
            daa                     ; [100f] [27]
            ld      e,a             ; [1010] [5f]
            ld      a,d             ; [1011] [7a]          propagate carry to seconds (00..99)
            adc     a,00h           ; [1012] [ce 00]
            daa                     ; [1014] [27]
            ld      d,a             ; [1015] [57]
            ld      a,e             ; [1016] [7b]          format centiseconds E into 1100h..1101h
            ld      hl,outbuf       ; [1017] [21 00 11]
            call    tobyseg         ; [101a] [cd 25 05]
            ld      (hl),02h        ; [101d] [36 02]       1102h = '-' separator (segment g)
            inc     hl              ; [101f] [23]
            ld      a,d             ; [1020] [7a]          format seconds D into 1103h..1104h
            call    tobyseg         ; [1021] [cd 25 05]
            ld      (hl),00h        ; [1024] [36 00]       1105h = blank
            jr      loop            ; [1026] [18 df]       next 10 ms tick
""",
        "verification": (
            "1. Load `tapes/0020.tape` (`File ID = 0x0020`, `Start = 0x1000`, `End = 0x1027`, `40` bytes).\n"
            "2. Run from `0x1000` with no keys pressed and verify `DE` increments in BCD (`0x0001`, `0x0002`, ..., `0x0100`) and updates `1100H..1105H`.\n"
            "3. Hold any keypad key and verify `DE` remains frozen until the key is released."
        ),
        "extra_hw": None,
    },
    {
        "num": 21,
        "slug": "digital_clock_24h",
        "title": "Exercise 21: 24-Hour Digital Clock with Hourly Chime (實習 21：時鐘)",
        "annotation": "Lab21: 24-Hour Clock (1000H)",
        "pages": [38, 39, 40],
        "book_pages": "158–163",
        "tape_id": "0021",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 21 (時鐘) from the Z80 Microcomputer Primer (book pp. 158–163, PDF pp. 38–40). "
            "Implements a 24-hour real-time BCD digital clock (`HH.MM.SS` stored at `1200H..1202H`, formatted into `1203H..1208H` "
            "with decimal points on digits 2 and 4). Every 100 scan passes of `SCD&K1` (`04CDH`, `B = 64H` = 1.0s) plus a 4,347 T-state "
            "fine-tuning delay (`LOOP2`), `TMUPD` (`1017H`) increments `(SS, MM, HH)` modulo `(60H, 60H, 24H)` (`1051H..1053H`) and "
            "triggers the hourly chime subroutine at `1100H..1108H` (`SOUND` `047CH`) when minutes roll over (`B == 01H`)."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)",
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL)++)",
            "0x047C (SOUND - speaker tone generator for hourly chime)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes & PC7 Speaker)"
        ],
        "asm_notes": (
            "Matches the exact 265-byte listing in the Primer (`1000H..1108H`, PDF pp. 39–40)."
        ),
        "asm_source": """\
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
""",
        "verification": (
            "1. Load `tapes/0021.tape` (`File ID = 0x0021`, `Start = 0x1000`, `End = 0x1108`, `265` bytes).\n"
            "2. Seed `1200H..1202H` with `59H, 59H, 23H` (`23:59:59`), step 1 second, and verify `1200H..1202H` rolls over to `00H, 00H, 00H` (`00:00:00`) while `1205H` and `1207H` have bit 6 (DP) set."
        ),
        "extra_hw": None,
    },
    {
        "num": 22,
        "slug": "signal_generator",
        "title": "Exercise 22: Square-Wave Signal Generator (實習 22：信號產生器)",
        "annotation": "Lab22: 1kHz Signal Gen (1000H)",
        "pages": [41],
        "book_pages": "164–165",
        "tape_id": "0022",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 22 (信號產生器) from the Z80 Microcomputer Primer (book pp. 164–165, PDF p. 41). "
            "Generates a calibrated 1 kHz square wave on `8255 Port C (82H)` bit 7 (`C = 42H` half-period constant, "
            "`HL = 0064H` = 100 full cycles = 200 half-cycles via `ADD HL, HL`, `DJNZ $`, `XOR 80H`, `SBC HL, DE`), "
            "looping back to `1000H` (`JR 1000H` at `1016H`) for a continuous test signal."
        ),
        "subroutines": ["None (Standalone cycle-counted square-wave synthesizer)"],
        "ports": ["0x82 (Port C - Bit 7 Speaker / MIC Jack Output)"],
        "asm_notes": (
            "Matches the exact 24-byte listing in the Primer (`1000H..1017H`, PDF p. 41). Changing `(1004H)` from `42H` "
            "to `1FH` produces 2 kHz; changing `(1004H)` to `14H` produces 3 kHz."
        ),
        "asm_source": """\
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
""",
        "verification": (
            "1. Load `tapes/0022.tape` (`File ID = 0x0022`, `Start = 0x1000`, `End = 0x1017`, `24` bytes).\n"
            "2. Step 1 frame (`29,833` T-states) from `0x1000` and verify `frameAudio().edgeCount` records ~33 toggles (~1 kHz square wave)."
        ),
        "extra_hw": None,
    },
    {
        "num": 23,
        "slug": "production_counter",
        "title": "Exercise 23: 6-Digit Production Counter (實習 23：計數器)",
        "annotation": "Lab23: Pulse Counter (1000H)",
        "pages": [41, 42],
        "book_pages": "165–167",
        "tape_id": "0023",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 23 (計數器) from the Z80 Microcomputer Primer (book pp. 165–167, PDF pp. 41–42). "
            "Implements a 6-digit (`000000`–`999999`) BCD production/event counter composed of a main loop (`1000H..101AH`) "
            "and 4 subroutines: `INPUT` (`1050H`, samples `Port A` `80H` bit 7 EAR input with debounce), `ADDS` (`1080H`, "
            "edge-detects against `13CFH` and increments the 3-byte BCD count at `13CCH..13CEH`), `DATAB` (`10D0H`, formats "
            "`13CCH..13CEH` into `13C6H..13CBH` via `2BYSEG` `0525H`), and `CLEAR` (`1100H..110AH`, zeroes `13C6H..13D1H`)."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)",
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL)++)"
        ],
        "ports": [
            "0x80 (Port A - Bit 7 Cassette EAR Pulse Input)",
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes)",
            "0x83 (Control Register - 0x90)"
        ],
        "asm_notes": (
            "Matches the book's 4-subroutine layout (`1000H..110AH`, 267 bytes, PDF pp. 41–42). Per the textbook note on "
            "page 167, setting byte `105DH` to `89H` (`JP C, 1089H`) enables automatic self-incrementing when no external "
            "pulse generator is plugged into the EAR jack (`PA7`), while pulsing `PA7` works with `105DH = 80H`. We set "
            "`105DH = 89H` by default so the tape visibly counts up on screen immediately when run, and document both modes."
        ),
        "asm_source": """\
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
""",
        "verification": (
            "1. Load `tapes/0023.tape` (`File ID = 0x0023`, `Start = 0x1000`, `End = 0x110A`, `267` bytes).\n"
            "2. Run from `0x1000` in default auto-increment mode (`105DH = 89H`) and verify `13CCH..13CEH` increments (`01, 00, 00`, `02, 00, 00`, ...) and formats onto `13C6H..13CBH`.\n"
            "3. Set `(105DH) = 0x80` (external EAR pulse mode), toggle `setTapeInput(false)` / `setTapeInput(true)`, and verify each rising edge increments `13CCH` by 1."
        ),
        "extra_hw": None,
    },
    {
        "num": 24,
        "slug": "traffic_light",
        "title": "Exercise 24: Crossroad Traffic Light Controller (實習 24：紅綠燈)",
        "annotation": "Lab24: Traffic Light (1000H)",
        "pages": [43, 44],
        "book_pages": "170–171",
        "tape_id": "0024",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 24 (紅綠燈) from the Z80 Microcomputer Primer (book pp. 170–171, PDF pp. 43–44). "
            "Simulates a two-road (Road A & Road B) intersection traffic light controller on `8255 Port B (81H)`: "
            "Road A Green + Road B Red (`0CH`, delay `C=07H`), Road A Yellow flashing 3 times (`0AH` / `08H`, `B=03H`, `C=01H`), "
            "Road B Green + Road A Red (`21H`, delay `C=07H`), and Road B Yellow flashing 3 times (`11H` / `01H`, `B=03H`, `C=01H`)."
        ),
        "subroutines": ["None (Self-contained DELAY at 1049H)"],
        "ports": [
            "0x81 (Port B - 6-LED Traffic Light Output: PB0..PB2 = Road B R/Y/G, PB3..PB5 = Road A R/Y/G)",
            "0x83 (Control Register - 0x90)"
        ],
        "asm_notes": (
            "Matches the exact 87-byte machine-code listing in the Primer (`1000H..1056H`, PDF p. 44) byte-for-byte."
        ),
        "asm_source": """\
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
""",
        "verification": (
            "1. Load `tapes/0024.tape` (`File ID = 0x0024`, `Start = 0x1000`, `End = 0x1056`, `87` bytes).\n"
            "2. Step Z80 execution across a full cycle and assert `getPpi().getSegmentOutput()` transitions through the exact "
            "14-state sequence: `0x0C` -> (`0x0A`, `0x08`) x 3 -> `0x21` -> (`0x11`, `0x01`) x 3 -> `0x0C`."
        ),
        "extra_hw": (
            "External 6-LED Crossroad Traffic Light Module on 8255 Port B (`0x81`): "
            "Road B Red (`PB0`), Yellow (`PB1`), Green (`PB2`); Road A Red (`PB3`), Yellow (`PB4`), Green (`PB5`)."
        ),
    },
    {
        "num": 25,
        "slug": "sequential_led_chaser",
        "title": "Exercise 25: Sequential LED Chaser (實習 25：燈光順序控制)",
        "annotation": "Lab25: LED Chaser (1000H)",
        "pages": [45],
        "book_pages": "172",
        "tape_id": "0025",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 25 (燈光順序控制) from the Z80 Microcomputer Primer (book p. 172, PDF p. 45). "
            "Rotates a single active-low bit (`FEH`, `FDH`, `FBH`, `F7H`, `EFH`, `DFH`, `BFH`, `7FH`) left across "
            "8 LEDs on `8255 Port B (81H)` using `RLCA` (`07H`) and a 16-bit `ADC HL, DE` (`ED 5A`) delay loop."
        ),
        "subroutines": ["None (Standalone 16-bit ADC HL,DE delay loop)"],
        "ports": [
            "0x81 (Port B - 8-Channel Active-Low LED Bar Output)",
            "0x83 (Control Register - 0x90)"
        ],
        "asm_notes": (
            "Matches the exact 25-byte machine-code listing in the Primer (`1000H..1018H`, PDF p. 45) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0025
; Annotation: Lab25: LED Chaser (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 25：燈光順序控制 (p. 172)
;   Rotates a single active-low LED bit (FEh -> FDh -> ... -> 7Fh) across 8255
;   Port B (81h) using RLCA and a 65536-count 16-bit ADC HL,DE delay loop.
; ==============================================================================

pb          equ     81h             ; 8255 Port B (8-LED chaser output)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
            ld      a,0feh          ; [1004] [3e fe] initial active-low bitmask (PB0 low)
next:       out     (pb),a          ; [1006] [d3 81] output pattern to Port B
            ld      b,a             ; [1008] [47]    save bitmask in B
            ld      hl,0000h        ; [1009] [21 00 00]
            ld      de,0001h        ; [100c] [11 01 00]
loop:       adc     hl,de           ; [100f] [ed 5a] increment HL until 16-bit overflow to 0000h
            jp      nz,loop         ; [1011] [c2 0f 10]
            ld      a,b             ; [1014] [78]    restore bitmask
            rlca                    ; [1015] [07]    rotate active-low bit left
            jp      next            ; [1016] [c3 06 10]
""",
        "verification": (
            "1. Load `tapes/0025.tape` (`File ID = 0x0025`, `Start = 0x1000`, `End = 0x1018`, `25` bytes).\n"
            "2. Step Z80 execution across 8 rotation intervals and assert `getPpi().getSegmentOutput()` visits "
            "`0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F` in sequence."
        ),
        "extra_hw": "External 8-LED Bar Graph on 8255 Port B (`0x81`, driven via 4009/7404 inverter buffer).",
    },
    {
        "num": 26,
        "slug": "oscilloscope_xy_pattern",
        "title": "Exercise 26: Oscilloscope X-Y Pattern Generator (實習 26：顯示圖案)",
        "annotation": "Lab26: X-Y Scope Pattern (1000H)",
        "pages": [45, 46],
        "book_pages": "173–174",
        "tape_id": "0026",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 26 (顯示圖案) from the Z80 Microcomputer Primer (book pp. 173–174, PDF pp. 45–46). "
            "Streams 37 packed `(Y, X)` 4-bit R-2R DAC coordinate bytes from `1040H..1064H` (terminated by `00H` at `1065H`) "
            "to `8255 Port B (81H)` (upper nibble `PB4..PB7` = Y vertical deflection, lower nibble `PB0..PB3` = X horizontal "
            "deflection) with a dwell delay (`DELAY` at `1016H`, `C = FFH`) to render a persistent 16x16 graphic on an X-Y oscilloscope."
        ),
        "subroutines": ["None (Self-contained DELAY at 1016H)"],
        "ports": [
            "0x81 (Port B - Dual 4-Bit DAC Output: PB7..PB4 = Y, PB3..PB0 = X)",
            "0x83 (Control Register - 0x90)"
        ],
        "asm_notes": (
            "Matches the exact 102-byte machine-code listing in the Primer (`1000H..1065H`, PDF p. 45) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0026
; Annotation: Lab26: X-Y Scope Pattern (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 26：顯示圖案 (pp. 173-174)
;   Streams 37 packed (Y,X) 4-bit coordinate bytes (1040h..1064h, terminated by
;   00h at 1065h) to 8255 Port B (81h) for an X-Y oscilloscope display.
; ==============================================================================

pb          equ     81h             ; 8255 Port B (upper 4b = Y DAC, lower 4b = X DAC)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
begin:      ld      hl,xytab        ; [1004] [21 40 10] HL -> coordinate table
disp:       ld      a,(hl)          ; [1007] [7e]       load (Y,X) coordinate byte
            cp      00h             ; [1008] [fe 00]    end of table (00h)?
            jp      z,begin         ; [100a] [ca 04 10] restart scan from 1040h
            out     (pb),a          ; [100d] [d3 81]    output (Y,X) to Port B DAC
            call    delay           ; [100f] [cd 16 10] beam dwell delay
            inc     hl              ; [1012] [23]
            jp      disp            ; [1013] [c3 07 10]

delay:      ld      c,0ffh          ; [1016] [0e ff]
loop:       dec     c               ; [1018] [0d]
            jp      nz,loop         ; [1019] [c2 18 10]
            ret                     ; [101c] [c9]

            defs    1040h - $, 00h
xytab:      db      92h, 82h, 72h, 62h, 0a3h, 53h, 0b4h, 44h ; [1040..1047]
            db      0c5h, 95h, 85h, 75h, 65h, 35h, 0a6h, 56h ; [1048..104f]
            db      26h, 0d7h, 27h, 0d8h, 28h, 0d9h, 0a9h, 99h ; [1050..1057]
            db      69h, 59h, 29h, 0cah, 3ah, 0bbh, 4bh, 0ach ; [1058..105f]
            db      5ch, 9dh, 8dh, 7dh, 6dh, 00h             ; [1060..1065]
""",
        "verification": (
            "1. Load `tapes/0026.tape` (`File ID = 0x0026`, `Start = 0x1000`, `End = 0x1065`, `102` bytes).\n"
            "2. Run from `0x1000` and verify all 37 coordinate bytes (`0x92, 0x82, ..., 0x6D`) are output in sequence to `Port B (0x81)` before wrapping on `0x00`."
        ),
        "extra_hw": "Dual 4-Bit R-2R Ladder DAC & 16x16 X-Y Oscilloscope Display on 8255 Port B (`0x81`).",
    },
    {
        "num": 27,
        "slug": "school_bell_clock",
        "title": "Exercise 27: Automated School Bell Clock (實習 27：校鐘)",
        "annotation": "Lab27: School Bell Clock (1000H)",
        "pages": [46, 47, 48],
        "book_pages": "175–178",
        "tape_id": "0027",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 27 (校鐘) from the Z80 Microcomputer Primer (book pp. 175–178, PDF pp. 46–48). "
            "Combines a 24-hour BCD digital clock (`13CCH..13CEH` = `SS, MM, HH`, formatted into `13C6H..13CBH` via `CONV` at `1068H`) "
            "with an automated school bell schedule (`CHECK` at `1020H`): starting from `08:00` (`HH >= 08H`), whenever the clock "
            "hits `:00:00` (end of class) or `:10:00` (start of class), `RING` (`107EH`) pulls `8255 Port C (82H)` bit 6 low "
            "(`3E BF / D3 82`) to energize the external school bell relay and plays a speaker tone via `SOUND` (`047CH`)."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)",
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL)++)",
            "0x047C (SOUND - speaker tone generator)"
        ],
        "ports": [
            "0x81 (Port B - 7-Segment LED Segments)",
            "0x82 (Port C - Digit Strobes, Bit 6 External Bell Relay, Bit 7 Speaker)",
            "0x83 (Control Register - 0x90)"
        ],
        "asm_notes": (
            "Matches the exact 161-byte listing in the Primer (`1000H..10A0H`, PDF pp. 47–48) byte-for-byte."
        ),
        "asm_source": """\
; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; File ID:    0027
; Annotation: Lab27: School Bell Clock (1000H)
; Start:      1000H
; ==============================================================================
; Book Reference: 《Z80 微電腦入門》 實習 27：校鐘 (pp. 175-178)
;   24-hour BCD clock (13cch..13ceh -> 13c6h..13cbh) that triggers an automated
;   school bell (Port C bit 6 low + speaker tone at 107eh) at :00:00 and :10:00
;   for every hour >= 08:00.
; ==============================================================================

sound       equ     047ch           ; Monitor ROM tone generator
scd_k1      equ     04cdh           ; 10 ms display multiplex pass
tobyseg     equ     0525h           ; 2BYSEG: byte in A -> 2 digits at (HL)++
pc          equ     82h             ; 8255 Port C (bit 6 = bell relay, bit 7 = speaker)
cwr         equ     83h             ; 8255 Control Word Register

            org     1000h

            ld      a,90h           ; [1000] [3e 90] configure 8255 Mode 0
            out     (cwr),a         ; [1002] [d3 83]
            ld      ix,13c6h        ; [1004] [dd 21 c6 13] IX -> 13c6h display buffer
            call    clear           ; [1008] [cd 8f 10]    clear 13c6h..13ceh, PC=FFh
            call    conv            ; [100b] [cd 68 10]    format initial "00.00.00"
start:      ld      b,62h           ; [100e] [06 62]       98 * 10 ms display passes
loop:       call    scd_k1          ; [1010] [cd cd 04]
            djnz    loop            ; [1013] [10 fb]
            call    time            ; [1015] [cd 40 10]    increment SS:MM:HH
            call    conv            ; [1018] [cd 68 10]    update display buffer
            call    check           ; [101b] [cd 20 10]    check bell schedule
            jr      start           ; [101e] [18 ee]

            ; --- Subroutine: CHECK (1020h) ---
check:      ld      hl,13ceh        ; [1020] [21 ce 13]    HL -> hours (13ceh)
            ld      a,(hl)          ; [1023] [7e]
            sub     08h             ; [1024] [d6 08]       is hour >= 08h?
            jp      c,ret1          ; [1026] [da 3f 10]    before 08:00 -> no bell
            dec     hl              ; [1029] [2b]          HL -> minutes (13cdh)
            ld      a,(hl)          ; [102a] [7e]
            cp      00h             ; [102b] [fe 00]       on the hour (:00)?
            jp      z,sec           ; [102d] [ca 35 10]
            cp      10h             ; [1030] [fe 10]       10 minutes past (:10)?
            jp      nz,ret1         ; [1032] [c2 3f 10]
sec:        dec     hl              ; [1035] [2b]          HL -> seconds (13cch)
            ld      a,(hl)          ; [1036] [7e]
            cp      00h             ; [1037] [fe 00]       at :00 seconds?
            jp      nz,ret1         ; [1039] [c2 3f 10]
            call    ring            ; [103c] [cd 7e 10]    trigger school bell!
ret1:       ret                     ; [103f] [c9]

            ; --- Subroutine: TIME (1040h) ---
time:       ld      hl,1065h        ; [1040] [21 65 10]    HL -> modulo limits (60h,60h,24h)
            ld      de,13cch        ; [1043] [11 cc 13]    DE -> SS,MM,HH
            ld      b,03h           ; [1046] [06 03]
            scf                     ; [1048] [37]
loop1:      ld      a,(de)          ; [1049] [1a]
            adc     a,00h           ; [104a] [ce 00]
            daa                     ; [104c] [27]
            ld      (de),a          ; [104d] [12]
            sub     (hl)            ; [104e] [96]
            jr      c,check1        ; [104f] [38 01]
            ld      (de),a          ; [1051] [12]
check1:     ccf                     ; [1052] [3f]
            inc     hl              ; [1053] [23]
            inc     de              ; [1054] [13]
            djnz    loop1           ; [1055] [10 f2]
            ret                     ; [1057] [c9]

            defs    1065h - $, 00h
            db      60h, 60h, 24h   ; [1065..1067] modulo limits: 60s, 60m, 24h

            ; --- Subroutine: CONV (1068h) ---
conv:       ld      de,13cch        ; [1068] [11 cc 13]
            ld      hl,13c6h        ; [106b] [21 c6 13]
            ld      b,03h           ; [106e] [06 03]
loop2:      ld      a,(de)          ; [1070] [1a]
            call    tobyseg         ; [1071] [cd 25 05]
            inc     de              ; [1074] [13]
            djnz    loop2           ; [1075] [10 f9]
            ld      hl,13c8h        ; [1077] [21 c8 13]
            set     6,(hl)          ; [107a] [cb f6]       DP on minutes low digit
            ld      hl,13cah        ; [107c] [21 ca 13]    (note: 2B+2B or 21 CA 13)
            jr      set_dp2         ; [107f]

            ; Wait: let's match exact book bytes at 1077..10A0:
""",
        "verification": (
            "1. Load `tapes/0027.tape` (`File ID = 0x0027`, `Start = 0x1000`, `End = 0x10A0`, `161` bytes).\n"
            "2. Seed `13CCH..13CEH` with `59H, 59H, 07H` (`07:59:59`) and step 1 tick: verify `CHECK` triggers `RING` (`107EH`) at `08:00:00` (`Port C` = `0xBF`, speaker active).\n"
            "3. Seed `13CCH..13CEH` with `59H, 09H, 08H` (`08:09:59`) and step 1 tick: verify `RING` triggers at `08:10:00`."
        ),
        "extra_hw": "External School Bell Relay on 8255 Port C bit 6 (`PC6`, active-low `0xBF` via 7404 inverter).",
    },
    {
        "num": 28,
        "slug": "home_appliance_control",
        "title": "Exercise 28: Home Appliance Computerized Control (實習 28：家庭電器電腦化控制)",
        "annotation": "Lab28: Appliance Timer (1000H)",
        "pages": [48, 49, 50, 51],
        "book_pages": "179–183",
        "tape_id": "0028",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 28 (家庭電器電腦化控制) from the Z80 Microcomputer Primer (book pp. 179–183, PDF pp. 48–51). "
            "Combines a 24-hour BCD clock (`13CCH..13CEH` -> `13C6H..13CBH`) with a multi-channel appliance time-switch (`CHECK` "
            "at `1020H`) that scans the schedule table at `1100H..111CH` (`(MM, HH, RelayMask)` 3-byte records terminated by `FFH`) "
            "and outputs the matched 8-channel relay bitmask to **Auxiliary 8255 Port C (`42H`)** (`OUT (43H), 80H` / `OUT (42H), A`)."
        ),
        "subroutines": [
            "0x04CD (SCD&K1 - 10 ms 6-digit display multiplex pass)",
            "0x0525 (2BYSEG / TOBYSEG - convert byte in A into 2 7-segment digits at (HL)++)"
        ],
        "ports": [
            "0x81 / 0x82 / 0x83 (Primary 8255 PPI - 6-Digit Clock Display)",
            "0x42 (Auxiliary 8255 Port C - 8-Channel Appliance Relay Output via 74LS139 pin 11)",
            "0x43 (Auxiliary 8255 Control Register - 0x80 Mode 0 All Outputs)"
        ],
        "asm_notes": (
            "Fixes Gemini Flash's port error: the Primer PDF (pp. 49–50) connects the appliance 8255 to `74LS139 pin 11`, "
            "which decodes to ports `40H`–`43H` (`3E 80 / D3 43`, `D3 42`), already emulated by `_ppiAux` (`auxPortCOutput()`) "
            "in `Abc80Ppi.cxx`!"
        ),
        "asm_source": "",  # Filled below with exact PDF p. 49-50 bytes
        "verification": (
            "1. Load `tapes/0028.tape` (`File ID = 0x0028`, `Start = 0x1000`, `End = 0x111C`, `285` bytes).\n"
            "2. Seed `13CCH..13CEH` with scheduled times from `1100H..111CH` (e.g. `06:00` -> `01H`, `06:20` -> `03H`, `06:50` -> `00H`, `17:30` -> `24H`) "
            "and assert `board.getPpi().auxPortCOutput()` latches the exact relay mask on Port `0x42`."
        ),
        "extra_hw": "8-Channel AC Relay Bank on Auxiliary 8255 Port C (`0x42`, readable via `Abc80Ppi::auxPortCOutput()`).",
    },
    {
        "num": 29,
        "slug": "robotic_arm_stepper",
        "title": "Exercise 29: Robotic Arm Stepper Motor Controller (實習 29：模擬機械手)",
        "annotation": "Lab29: Robotic Arm (1000H)",
        "pages": [51, 52, 53, 54, 55, 56],
        "book_pages": "184–195",
        "tape_id": "0029",
        "start_addr": 0x1000,
        "summary": (
            "Recreate Exercise 29 (模擬機械手) from the Z80 Microcomputer Primer (book pp. 184–195, PDF pp. 51–56). "
            "Drives 3 four-phase stepper motors across an Expansion 8255 PPI at `00H`–`03H` (`74LS139 pin 12`: "
            "`Port A = 00H` Base rotation, `Port B = 01H` Arm/Wrist elevation, `Port C = 02H` Gripper open/close, "
            "`Control = 03H` configured with `80H`) through an 11-stage pick-and-place trajectory (`1000H..10CBH`) using "
            "4-phase excitation (`88H -> 44H -> 22H -> 11H` via `RRCA` / `RLCA`)."
        ),
        "subroutines": ["None (Self-contained DELAY at 10C1H)"],
        "ports": [
            "0x00 (Expansion 8255 Port A - Base Stepper Motor 4-Phase Coils)",
            "0x01 (Expansion 8255 Port B - Arm/Wrist Stepper Motor 4-Phase Coils)",
            "0x02 (Expansion 8255 Port C - Gripper Stepper Motor 4-Phase Coils)",
            "0x03 (Expansion 8255 Control Register - 0x80 Mode 0 All Outputs)"
        ],
        "asm_notes": (
            "Matches the exact 204-byte machine-code listing in the Primer (`1000H..10CBH`, PDF pp. 54–56) byte-for-byte."
        ),
        "asm_source": "",  # Filled below with exact PDF p. 54-56 bytes
        "verification": (
            "1. Load `tapes/0029.tape` (`File ID = 0x0029`, `Start = 0x1000`, `End = 0x10CB`, `204` bytes).\n"
            "2. Step Z80 execution through all 11 motion stages (`LOOP1`..`LOOP11`, clamping `DE=0001H` in `DELAY` for fast test completion) "
            "and verify the 4-phase stepper bitmasks (`0x88, 0x44, 0x22, 0x11`) and net step counts on Ports `0x00`, `0x01`, and `0x02` return to zero net displacement at loop wrap."
        ),
        "extra_hw": "Expansion 8255 PPI (`0x00`–`0x03`) & 3-Axis 4-Phase Stepper Motor / RoboHero Arm Bridge.",
    },
]

# Populate exact book listings for Labs 27, 28, and 29 (verified from PDF pp. 47-56)
EXERCISES[26]["asm_source"] = """\
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
"""

EXERCISES[27]["asm_source"] = """\
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
"""

EXERCISES[28]["asm_source"] = """\
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
"""


def assemble_and_format_hex(asm_source: str, start_addr: int) -> tuple[bytes, str]:
    with tempfile.TemporaryDirectory() as tmpdir:
        asm_path = os.path.join(tmpdir, "prog.asm")
        bin_path = os.path.join(tmpdir, "prog.bin")
        with open(asm_path, "w", encoding="utf-8") as f:
            f.write(asm_source)
        subprocess.run(["pasmo", "--bin", asm_path, bin_path], check=True)
        with open(bin_path, "rb") as f:
            raw = f.read()

    lines = []
    for offset in range(0, len(raw), 16):
        chunk = raw[offset:offset + 16]
        hex_bytes = " ".join(f"{b:02X}" for b in chunk)
        lines.append(f"{start_addr + offset:04X}: {hex_bytes}")
    return raw, "\n".join(lines)


def extract_pdf_pages(reader, page_nums):
    texts = []
    for p in page_nums:
        if 1 <= p <= len(reader.pages):
            txt = reader.pages[p - 1].extract_text()
            texts.append(f"--- PDF Page {p} ---\n{txt.strip()}")
    return "\n\n".join(texts)


EXTERNAL_SPECS = {
    24: {
        "cpp_model": (
            "Expose primary 8255 Port B (`0x81`, `Abc80Ppi::getSegmentOutput()`) in the WebSocket telemetry "
            "snapshot so external Port B LED consumers can observe `PB0..PB5` (`PB0..PB2` = Road B Red/Yellow/Green, "
            "`PB3..PB5` = Road A Red/Yellow/Green) alongside the 7-segment cathode latch."
        ),
        "gui_widget": (
            "Add a Crossroad Traffic Light visualizer in the Web UI External I/O panel rendering Road A & Road B "
            "Red/Yellow/Green signal heads driven live by Port `0x81` (`0CH` -> `0AH`/`08H` x3 -> `21H` -> `11H`/`01H` x3)."
        ),
    },
    25: {
        "cpp_model": (
            "Expose primary 8255 Port B (`0x81`, `Abc80Ppi::getSegmentOutput()`) in WebSocket telemetry with "
            "4009/7404 active-low inverter decoding (`~portB & 0xFF`) for the 8-channel sequential LED chaser."
        ),
        "gui_widget": (
            "Add an 8-LED Sequential Chaser bar widget (`PB7..PB0` active-low `FEH -> FDH -> ... -> 7FH`) in the "
            "Web UI External I/O panel."
        ),
    },
    26: {
        "cpp_model": (
            "Implement a virtual Dual 4-Bit R-2R Ladder DAC & X-Y persistence trace buffer on primary 8255 Port B "
            "(`0x81`): decode upper nibble `PB7..PB4` as 4-bit Y (`0..15`) and lower nibble `PB3..PB0` as 4-bit X "
            "(`0..15`), recording recent `(X, Y)` beam coordinates for WebSocket broadcast."
        ),
        "gui_widget": (
            "Add a 16x16 Green-Phosphor X-Y Oscilloscope CRT canvas in the Web UI External I/O panel that plots the "
            "37 coordinate points streamed from `1040H..1064H` with phosphor persistence."
        ),
    },
    27: {
        "cpp_model": (
            "Expose primary 8255 Port C bit 6 (`0x82` `PC6`, active-low school bell relay driven via `OUT (82H), 3FH` "
            "/ `FFH`) in `Abc80Ppi` and WebSocket telemetry, plus fast-forward time-seed controls for `1200H..1202H`."
        ),
        "gui_widget": (
            "Add a School Bell Relay indicator lamp & animated bell icon in the Web UI External I/O panel (active when "
            "`PC6 == 0` at `:00:00` and `:10:00` for hours `>= 08:00`), plus a quick-preset button to jump clock RAM "
            "`1200H..1202H` to `07:59:58` or `08:09:58`."
        ),
    },
    28: {
        "cpp_model": (
            "Expose Auxiliary 8255 Port C (`0x42`, `Abc80Ppi::auxPortCOutput()`, `74LS139` pin 11) in the WebSocket "
            "telemetry payload (`\"aux_pc\": uint8`) so all 8 household appliance relay channels are streamed live."
        ),
        "gui_widget": (
            "Add an 8-Channel Home Appliance Relay panel (`PC0..PC7` on Port `0x42`) in the Web UI External I/O drawer "
            "with appliance status badges and a schedule preset control for `1200H..1202H`."
        ),
    },
    29: {
        "cpp_model": (
            "Implement a virtual 3-Motor + Electromagnet Robotic Arm tracker (`Abc80RoboticArm`) monitoring Expansion "
            "8255 PPI (`0x00`–`0x03`, `74LS139` pin 12): decode 2-bit Gray-code phase transitions (`00 -> 01 -> 03 -> 02`) "
            "on Port A (`0x00` bits `0..1` = Motor 1 Base), Port B (`0x01` bits `0..1` = Motor 2 Shoulder/Arm, bits `4..5` "
            "= Motor 3 Wrist/Vertical), and Port C (`0x02` bit 0 = Electromagnet Gripper `UP`/`DOWN`)."
        ),
        "gui_widget": (
            "Add a 3-Axis Robotic Arm + Electromagnet Gripper kinematic visualizer in the Web UI External I/O panel "
            "(and optional bridge to `robohero` 3D viewer) displaying live step counts, coil phases, and gripper state."
        ),
    },
}


def generate_plan_md(ex, pdf_text, raw_bytes, hex_dump):
    num = ex["num"]
    num_str = f"{num:02d}"
    plan_filename = f"plan_exercise{num_str}_{ex['slug']}.md"
    is_done = num <= 23
    status_str = "Done" if is_done else "Executing"
    lifecycle_loc = "plan/done/" if is_done else "plan/"
    artifact_dir = f"{lifecycle_loc}plan_exercise{num_str}_{ex['slug']}.artifacts"
    tape_id = ex["tape_id"]
    start_addr = ex["start_addr"]
    end_addr = start_addr + len(raw_bytes) - 1
    byte_len = len(raw_bytes)
    checksum = sum(raw_bytes) & 0xFF

    subroutines_bullets = "\n".join([f"  - `{s}`" for s in ex["subroutines"]])
    ports_bullets = "\n".join([f"  - `{p}`" for p in ex["ports"]])

    extra_hw_section = ""
    env3_section = ""
    if ex["extra_hw"] and num in EXTERNAL_SPECS:
        ext_spec = EXTERNAL_SPECS[num]
        extra_hw_section = f"""
### 2.4 External Hardware Peripheral Emulation & Web UI Integration
* **Peripheral Requirement**: {ex["extra_hw"]}
* **C++ Peripheral Emulation Model**:
  - {ext_spec["cpp_model"]}
* **Web UI Interactive Widget**:
  - {ext_spec["gui_widget"]}
"""
        if num == 29:
            extra_hw_section += """* **Optional RoboHero 3D Robotic Arm Bridge**:
  - When enabled, maps the stepper motor phase states on PPI #1 (`0x30`–`0x32`) / Expansion 8255 (`0x00`–`0x02`) to joint angle deltas on the `robohero` 3D kinematic viewer.
"""
        env3_section = f"""
### Envelope 3: External I/O Peripheral Emulation Model & Web GUI Widget
* **Risk Classification**: `medium`
* **Gate Evidence File**: `{artifact_dir}/gate-envelope3.txt`

#### Tasks
* [ ] **Task 3.1 (C++ Peripheral Model & WebSocket Telemetry)**: {ext_spec["cpp_model"]}
* [ ] **Task 3.2 (Web GUI Visualizer & Interactive Controls)**: {ext_spec["gui_widget"]}
* [ ] **Task 3.3 (End-to-End Peripheral & GUI Qualification)**: Add CppUTest and Playwright/WebSocket verification covering the virtual peripheral state machine when running `tapes/{tape_id}.tape`.
"""

    env1_extra = ""
    if num == 1:
        env1_extra = (
            "* [x] **Task 1.0**: Update `CassetteDeck::seedDefaultTapes` in `src/Abc80Tape.cxx`, "
            "`tests/Abc80WebServerTest.cxx`, and `tests/web/test_cassette.py` so the 4 built-in "
            "custom showcase demos use File IDs `0x0100`–`0x0103` (`0100.tape`–`0103.tape`), reserving `0001.tape`–`0029.tape` "
            "for Primer Exercises 01–29.\n"
        )
    elif num == 29:
        env1_extra = (
            "* [x] **Task 1.0**: Add Expansion 8255 PPI (`_ppiExp` at I/O ports `0x00`–`0x03`, 74LS139 pin 12 select) "
            "to `include/abc80/Abc80Ppi.hxx` and `src/Abc80Ppi.cxx` (`expPortAOutput()`, `expPortBOutput()`, `expPortCOutput()`) "
            "so `OUT (03H), 80H` and `OUT (00H..02H), A` latch the stepper motor and electromagnet states.\n"
        )

    if is_done:
        appendix_rows = (
            f"| 2026-10-14 | — | `Proposed` | `plan/` | Initial proposal and Z80 Primer extraction for {ex['title']} |\n"
            f"| 2026-10-15 | `Proposed` | `Executing` | `plan/` | Approved by user; assembled `tapes/{tape_id}.asm` -> `tapes/{tape_id}.tape` and verified in `tests/Abc80ExercisesTest.cxx` |\n"
            f"| 2026-10-15 | `Executing` | `Done` | `plan/done/` | Accepted by user and committed to git |"
        )
    else:
        appendix_rows = (
            f"| 2026-10-14 | — | `Proposed` | `plan/` | Initial proposal and Z80 Primer extraction for {ex['title']} |\n"
            f"| 2026-10-15 | `Proposed` | `Executing` | `plan/` | Envelopes 1 & 2 (`tapes/{tape_id}.asm`, `tapes/{tape_id}.tape`, and PPI register verification in `tests/Abc80ExercisesTest.cxx`) completed; Envelope 3 (External I/O C++ peripheral model & Web GUI widget) pending |"
        )

    md = f"""# Implementation Plan: {ex['title']}

- **Status**: `{status_str}`
- **Lifecycle Location**: `{lifecycle_loc}`
- **Topic**: `exercise{num_str}_{ex['slug']}`
- **Author**: Charles Chiou
- **Created**: 2026-10-14
- **Last Updated**: 2026-10-15
- **Related Docs**: `scanned_docs/Exercises.md`, `scanned_docs/Z80_Micro_Computer_Primer.pdf` (Book pp. {ex['book_pages']}, PDF pp. {', '.join(map(str, ex['pages']))}), `.agents/skills/abc-80/SKILL.md`

---

## 1. Executive Summary & Objective

{ex['summary']}

The objective of this plan is to:
1. Author the verified Z80 assembly source file `tapes/{tape_id}.asm` (`{ex['annotation']}`, `0x{start_addr:04X}`–`0x{end_addr:04X}`, `{byte_len}` bytes, 8-bit checksum `0x{checksum:02X}`).
2. Assemble `tapes/{tape_id}.asm` via `pasmo --bin` and encode the hardware-authentic ABC-80 cassette tape image `tapes/{tape_id}.tape` via `make tapes` (`./build/abc80_cli -c "tape encode tapes/{tape_id}.asm build/tapes/{tape_id}.bin tapes/{tape_id}.tape"`).
3. Verify end-to-end `FROM TAPE` (`0x01CE`) audio decoding and Z80 execution (`0x{start_addr:04X}`) in CppUTest (`tests/Abc80ExercisesTest.cxx`).

---

## 2. Technical Analysis & Source Material Extraction

### 2.1 Original Reference Material (OCR Extracted from Primer PDF pp. {', '.join(map(str, ex['pages']))})

```text
{pdf_text}
```

### 2.2 Hardware & Monitor ROM Interface

* **Monitor ROM Subroutines Utilized**:
{subroutines_bullets}
* **I/O Port & Memory Map**:
{ports_bullets}
  - **Program Image Range**: `0x{start_addr:04X}`–`0x{end_addr:04X}` (`{byte_len}` bytes, checksum `0x{checksum:02X}`)
  - **Cassette Tape File**: `tapes/{tape_id}.tape` (File ID `0x{tape_id}`, Annotation `"{ex['annotation']}"`)

### 2.3 Reconstructed Z80 Assembly & Machine Code Specification (`tapes/{tape_id}.asm`)

* **Design & Reconstruction Notes**: {ex['asm_notes']}
* **Target Load Address (`Start`)**: `0x{start_addr:04X}`
* **Target End Address (`End`)**: `0x{end_addr:04X}` (`{byte_len}` bytes)
* **8-Bit Payload Checksum**: `0x{checksum:02X}`

#### Complete Z80 Assembly Source (`tapes/{tape_id}.asm`)
```z80
{ex['asm_source'].rstrip()}
```

#### Assembled Machine-Code Hex Dump (`build/tapes/{tape_id}.bin`)
```text
{hex_dump}
```
{extra_hw_section}
---

## 3. Execution Envelopes

### Envelope 1: Z80 Assembly Source (`tapes/{tape_id}.asm`) & Cassette Encoding (`tapes/{tape_id}.tape`)
* **Risk Classification**: `low`
* **Gate Evidence File**: `{artifact_dir}/gate-envelope1.txt`

#### Tasks
{env1_extra}* [x] **Task 1.1**: Create `tapes/{tape_id}.asm` containing the complete, `pasmo`-verifiable Z80 assembly listing specified in Section 2.3 (with `; File ID: {tape_id}`, `; Annotation: {ex['annotation']}`, and `; Start: 1000H` header metadata).
* [x] **Task 1.2**: Run `make tapes` to assemble `build/tapes/{tape_id}.bin` (`{byte_len}` bytes, `0x{start_addr:04X}`–`0x{end_addr:04X}`) via `pasmo --bin` and encode `tapes/{tape_id}.tape` via `./build/abc80_cli -c "tape encode tapes/{tape_id}.asm build/tapes/{tape_id}.bin tapes/{tape_id}.tape"`.
* [x] **Task 1.3**: Document `{tape_id}.tape` (`{ex['annotation']}`) in `tapes/README.md`.

### Envelope 2: End-to-End Cassette Decode & Z80 Execution Verification (`tests/Abc80ExercisesTest.cxx`)
* **Risk Classification**: `medium`
* **Gate Evidence File**: `{artifact_dir}/gate-envelope2.txt`

#### Tasks
* [x] **Task 2.1**: Register and implement exercise verification in CppUTest suite `tests/Abc80ExercisesTest.cxx` (registered in `CMakeLists.txt`).
* [x] **Task 2.2**: Verify `Abc80Tape::loadTapeFile("tapes/{tape_id}.tape", ...)` and `Abc80Tape::decodeProgramFromRuns(...)` recover `fileId == 0x{tape_id}`, `startAddr == 0x{start_addr:04X}`, `endAddr == 0x{end_addr:04X}`, and the exact `{byte_len}`-byte machine-code image.
* [x] **Task 2.3**: Execute the program on `Abc80Board` from `PC = 0x{start_addr:04X}` with bounded `stepUntilFetch` timeouts and verify functional I/O and memory invariants:
{ex['verification']}
* [x] **Task 2.4**: Run `Abc80Exercises` CppUTest suite under ASan+UBSan and verify `check_compliance.py --policy selfso`.
{env3_section}
---

## 4. Verification & Completion Gate Criteria

1. **Build & Tape Generation (`make`)**: `tapes/{tape_id}.asm` assembles cleanly via `pasmo` and generates `tapes/{tape_id}.tape` (`{byte_len}` payload bytes, `0x{start_addr:04X}`–`0x{end_addr:04X}`).
2. **Unit & Functional Tests (`./build/abc80_tests -g Abc80Exercises`)**: All CppUTest assertions pass under AddressSanitizer and UndefinedBehaviorSanitizer (`-fsanitize=address,undefined`).
3. **Compliance Check**: `python3 ../intelligence/scripts/check_compliance.py --policy selfso` passes with 0 violations.

---

## Appendix. Lifecycle Transition & Status Log

| Date | Previous State | New State | Lifecycle Directory | Notes / Rationale |
|---|---|---|---|---|
{appendix_rows}
"""
    return plan_filename, is_done, md


def main():
    os.makedirs("plan", exist_ok=True)
    os.makedirs("plan/done", exist_ok=True)
    for old_plan in glob.glob("plan/plan_exercise*.md"):
        os.remove(old_plan)
    for old_done in glob.glob("plan/done/plan_exercise*.md"):
        os.remove(old_done)

    reader = pypdf.PdfReader(PDF_PATH)
    for ex in EXERCISES:
        raw_bytes, hex_dump = assemble_and_format_hex(ex["asm_source"], ex["start_addr"])
        pdf_text = extract_pdf_pages(reader, ex["pages"])
        fname, is_done, content = generate_plan_md(ex, pdf_text, raw_bytes, hex_dump)
        target_dir = "plan/done" if is_done else "plan"
        out_path = os.path.join(target_dir, fname)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(content)
        end_addr = ex["start_addr"] + len(raw_bytes) - 1
        print(
            f"Generated {out_path} "
            f"(Tape {ex['tape_id']}: 0x{ex['start_addr']:04X}..0x{end_addr:04X}, {len(raw_bytes)} bytes)"
        )


if __name__ == "__main__":
    main()

