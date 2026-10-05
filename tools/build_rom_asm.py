#!/usr/bin/env python3
"""
Generate complete ABC-80 Z80 Monitor ROM assembly source file (src/rom.asm)
from pathlib import Path
from authentic scanned book listings in "Z-80 Microcomputer Building" (pp. 166-204),
translating all Chinese comments to English,
using all lowercase for opcodes, directives, registers, labels, equates, and hex constants.
"""

import os
import re

OUTPUT_FILE = os.path.join(str(Path(__file__).resolve().parent.parent), "src/rom.asm")

HEADER = """; ==============================================================================
; Copyright (C) 2026, Charles Chiou
;
; ABC-80 Microcomputer Learning Kit - 2KB Monitor ROM Disassembly Listing
; Target Architecture: Zilog Z80 CPU @ 2.5 MHz
; Hardware Components:
;   - CPU: Zilog Z80A
;   - ROM: 2732 / 2716 EPROM at 0x0000 - 0x07ff (2048 bytes)
;   - RAM: 6116 2KB Static RAM at 0x1000 - 0x17ff
;   - PPI: Intel 8255A Programmable Peripheral Interface (Ports 80h-83h)
;   - Display: 6-Digit 7-Segment LED Display (4-digit Address, 2-digit Data)
;   - Keypad: 24 Tactile Push-Button Switches in 4x6 Matrix
;   - Audio: Cassette Tape FSK Modulator / Demodulator & Speaker
;
; Translated from original reference documentation:
;   "Z80 Microcomputer Building" (Z-80 Microcomputer Construction, Chuan-Hwa Book Co.)
;   Part 3, Chapter 3: ABC-80 Monitor Program Analysis (Pages 166 - 204)
; ==============================================================================

; ------------------------------------------------------------------------------
; 8255 PPI I/O Port Definitions (Primary System PPI)
; ------------------------------------------------------------------------------
keypad      equ 80h         ; port a (input) : keypad matrix row inputs
kin         equ 80h         ; port a bit 7   : cassette audio input / earphone jack
segment     equ 81h         ; port b (output): 7-segment led segment drivers (a..g, dp)
digit       equ 82h         ; port c (output): display digit select & cassette / speaker
p8255       equ 83h         ; control register: 90h = mode 0 (pa=in, pb=out, pc=out)

; ------------------------------------------------------------------------------
; Auxiliary / EPROM Programmer 8255 I/O Port Definitions (Secondary PPI)
; ------------------------------------------------------------------------------
addlow      equ 40h         ; eprom address bus low (a0-a7)
daot        equ 41h         ; eprom data bus output
dain        equ 41h         ; eprom data bus input
addhig      equ 42h         ; eprom address bus high (a8-a11) & control (ce/oe/vpp)
cont        equ 42h         ; eprom programming control
q8255       equ 43h         ; auxiliary 8255 control register

; ------------------------------------------------------------------------------
; System RAM Workspace & Register Save Area (in 6116 SRAM, 0x1000 - 0x17ff)
; ------------------------------------------------------------------------------
reg_f       equ 13cch       ; saved register f
reg_a       equ 13cdh       ; saved register a
reg_c       equ 13ceh       ; saved register c
reg_b       equ 13cfh       ; saved register b
reg_e       equ 13d0h       ; saved register e
reg_d       equ 13d1h       ; saved register d
reg_l       equ 13d2h       ; saved register l
reg_h       equ 13d3h       ; saved register h
reg_f_prime equ 13d4h       ; saved register f'
reg_a_prime equ 13d5h       ; saved register a'
reg_c_prime equ 13d6h       ; saved register c'
reg_b_prime equ 13d7h       ; saved register b'
reg_e_prime equ 13d8h       ; saved register e'
reg_d_prime equ 13d9h       ; saved register d'
reg_l_prime equ 13dah       ; saved register l'
reg_h_prime equ 13dbh       ; saved register h'
reg_ixl     equ 13dch       ; saved register ixl (low byte)
reg_ixh     equ 13ddh       ; saved register ixh (high byte)
reg_iyl     equ 13deh       ; saved register iyl (low byte)
reg_iyh     equ 13dfh       ; saved register iyh (high byte)
spsave      equ 13e0h       ; saved stack pointer (sp)
pcsave      equ 13e2h       ; saved program counter (pc)
temp_reg    equ 13f8h       ; temporary storage for register hl during break trap

; ------------------------------------------------------------------------------
; Operating System Workspace (0x17af - 0x17fa)
; ------------------------------------------------------------------------------
usestk      equ 17afh       ; user stack initial top address
sysstk      equ 17bfh       ; system stack initial top address (16 bytes reserved)
stepbf      equ 17bfh       ; single-step execution trap buffer (7 bytes)
dispbf      equ 17c6h       ; 6-digit display buffer (led segments, 6 bytes)
adsave      equ 17eeh       ; current memory address displayed on screen (2 bytes)
stmoni      equ 17f3h       ; parameter counter for tape/monitor input
state       equ 17f4h       ; keypad state machine:
                            ;   0 = fix   (abc-80 banner mode)
                            ;   1 = adrs  (address entry mode)
                            ;   2 = data  (data entry mode)
                            ;   3 = to tape (tape save mode)
                            ;   4 = from tape (tape load mode)
pwup        equ 17f5h       ; power-on flag (80h = warm start / reset, other = cold start)
pwcode      equ 80h         ; warm start signature value
test        equ 17f6h       ; state flags: bit 0 = function key pressed, bit 7 = input error
atemp       equ 17f7h       ; temporary storage for accumulator (a)
hltemp      equ 17f8h       ; temporary storage for register pair hl
temp        equ 17fah       ; general temporary storage byte

; ------------------------------------------------------------------------------
; Audio & Modulation Constants
; ------------------------------------------------------------------------------
midpd       equ 2ah         ; period threshold between 1khz and 2khz pulses ((56+28)/2 = 42 = 2ah)

; ------------------------------------------------------------------------------
; Start of Monitor ROM (0x0000)
; ------------------------------------------------------------------------------
            org 0000h

"""

FOOTER = """
; ==============================================================================
; End of ABC-80 Monitor ROM (07ffh / 2048 bytes)
; ==============================================================================

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
"""

def generate():
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    print(f"[*] Generating {OUTPUT_FILE} with strictly lowercase assembly words...")
    
    sections = []
    
    # 1. Reset and Interrupt Vectors (0000 - 006f, pp. 166-167)
    sections.append("""; ==============================================================================
; SECTION 1: RESET AND INTERRUPT VECTORS (0000h - 006fh)
; ==============================================================================

; Power-On Reset Entry Point (RST 0)
; Stabilizes power supply and hardware lines before CPU initialization
            org     0000h\n0000: rst0: ld      b,00h           ; [06 00] power-on stabilization delay loop
0002:       djnz    $               ; [10 fe] loop 256 iterations until power rails settle
0004:       jp      begin           ; [c3 70 00] jump to main system initialization

; User Restart Vectors (Jump to user RAM dispatch table at 1210h - 1260h)
            org     0008h
0008: rst1: jp      1210h           ; [c3 10 12] rst 1 (08h) user service routine
            org     0010h
0010: rst2: jp      1220h           ; [c3 20 12] rst 2 (10h) user service routine
            org     0018h
0018: rst3: jp      1230h           ; [c3 30 12] rst 3 (18h) user service routine
            org     0020h
0020: rst4: jp      1240h           ; [c3 40 12] rst 4 (20h) user service routine
            org     0028h
0028: rst5: jp      1250h           ; [c3 50 12] rst 5 (28h) user service routine
            org     0030h
0030: rst6: jp      1260h           ; [c3 60 12] rst 6 (30h) user service routine

; Breakpoint / Single-Step Trap / Mode 1 Interrupt Handler (RST 38H / 0038h)
; Triggered by hardware single-step trap or RST 38H (0xff) instruction
            org     0038h
0038: break_handler:
0038:       ld      (13f8h),hl      ; [22 f8 13] preserve hl in user register dump (temp)
003b:       pop     hl              ; [e1] pop saved pc from stack
003c:       ld      (13eeh),hl      ; [22 ee 13] save pc to 13eeh
003f:       ld      (13e2h),hl      ; [22 e2 13] save pc to user register dump pcsave
0042:       ld      hl,(13f8h)      ; [2a f8 13] restore original hl
0045:       ld      (13e0h),sp      ; [ed 73 e0 13] save sp to spsave
0049:       ld      sp,13e0h        ; [31 e0 13] point sp to user register dump
004c:       push    iy              ; [fd e5] save iy to 13deh
004e:       push    ix              ; [dd e5] save ix to 13dch
0050:       exx                     ; [d9] swap to alternate register set
0051:       push    hl              ; [e5] save hl' to 13dah
0052:       push    de              ; [d5] save de' to 13d8h
0053:       push    bc              ; [c5] save bc' to 13d6h
0054:       exx                     ; [d9] swap back to main registers
0055:       ex      af,af'          ; [08] swap to alternate af
0056:       push    af              ; [f5] save af' to 13d4h
0057:       ex      af,af'          ; [08] swap back to main af
0058:       push    hl              ; [e5] save hl to 13d2h
0059:       push    de              ; [d5] save de to 13d0h
005a:       push    bc              ; [c5] save bc to 13ceh
005b:       push    af              ; [f5] save af to 13cch
005c:       jp      rst0            ; [c3 00 00] restart monitor loop

; Non-Maskable Interrupt Handler (NMI / 0066h)
            org     0066h
0066: nmi:  jp      1280h           ; [c3 80 12] jump to user nmi handler
""")

    # 2. System Initialization & Main Executive Loop (0070 - 00a8, pp. 167-168)
    sections.append("""; ==============================================================================
; SECTION 2: SYSTEM INITIALIZATION & MAIN EXECUTIVE LOOP (0070h - 00a8h)
; ==============================================================================

            org     0070h
0070: begin:
0070:       ld      a,90h           ; [3e 90] configure 8255 ppi: port a=in, port b=out, port c=out
0072:       out     (p8255),a       ; [d3 83] write 8255 control word
0074:       ld      a,0ffh          ; [3e ff] blank 7-segment displays
0076:       out     (digit),a       ; [d3 82] port c = 0ffh (all digit cathode transistors off)
0078:       ld      sp,sysstk       ; [31 bf 17] initialize system stack pointer (17bfh)
007b:       ld      a,(pwup)        ; [3a f5 17] read power-up magic flag (17f5h)
007e:       cp      pwcode          ; [fe 80] test for warm reset flag (80h)
0080:       call    nz,init         ; [c4 4c 05] if cold boot (not 80h), run memory/ram initialization
0083:       call    rstmu           ; [cd 44 05] play reset chime melody
0086:       ld      hl,1000h        ; [21 00 10] default display address = 1000h (start of user ram)
0089:       ld      (adsave),hl     ; [22 ee 17] store into adsave (17eeh)
008c:       xor     a               ; [af] clear accumulator
008d:       ld      (test),a        ; [32 f6 17] clear test flag (17f6h)
0090:       ld      ix,disp         ; [dd 21 bf 07] point ix to "abc-80" display font pattern (07bfh)
0094: setst0:
0094:       xor     a               ; [af] clear a
0095:       ld      (state),a       ; [32 f4 17] set system state = 0 (fix mode)
0098:       nop                     ; [00]
0099:       nop                     ; [00]
009a:       nop                     ; [00]
009b: main: call    scd_k           ; [cd a6 04] scan keyboard and refresh display
009e:       call    ms3k17          ; [cd 64 04] 3 khz key click tone (page 168: CALL MS3K1')
00a1:       call    chkey           ; [cd a9 00] keyboard decode & branch dispatch
00a4:       jr      main            ; [18 f5] loop indefinitely
""")

    # 3. Keyboard Input Decoding & Dispatch (00a9 - 00fe, pp. 168-169)
    sections.append("""; ==============================================================================
; SECTION 3: KEYBOARD INPUT DECODING & DISPATCH (00a9h - 00feh)
; ==============================================================================

            org     00a9h
00a9: chkey:
00a9:       cp      10h             ; [fe 10] test if key code is hex (0..f) or func (10..16)
00ab:       jr      c,khex          ; [38 27] if key < 10h, branch to hex key handler (00d4h)
00ad:       ld      hl,test         ; [21 f6 17] point hl to test flag (17f6h)
00b0:       set     0,(hl)          ; [cb c6] set bit 0 (function key active)
00b2:       sub     10h             ; [d6 10] subtract 10h (function key index: 0..6)
00b4:       cp      04h             ; [fe 04] compare with 4 ('+', '-', 'exec', 'data')
00b6:       ld      hl,subfun       ; [21 25 07] load base address of subfun table (0725h)
00b9:       jp      c,branch        ; [da 6a 03] if index < 4, dispatch via subfun table
00bc:       ld      ix,dispbf       ; [dd 21 c6 17] ix points to display buffer (17c6h)
00c0:       sub     02h             ; [d6 02] adjust index
00c2:       ld      hl,state        ; [21 f4 17] point to state byte (17f4h)
00c5:       ld      (hl),a          ; [77] update state with new state
00c6:       ld      hl,stmoni       ; [21 f3 17] parameter counter (17f3h)
00c9:       ld      (hl),00h        ; [36 00] clear parameter counter = 0
00cb:       ld      hl,func         ; [21 2b 07] load base address of func table (072bh)
00ce:       sub     02h             ; [d6 02] adjust index (0, 1, 2)
00d0:       jp      branch          ; [c3 6a 03] dispatch via branch routine (036ah)

; Subfunction Jump Handlers (Pages 168 - 169)
            org     00d4h\n00d4: khex: ld      c,a             ; [4f] save hex key in register c
00d5:       ld      hl,htab         ; [21 30 07] point hl to hex jump table (0730h)
00d8: prebr:
00d8:       ld      a,(state)       ; [3a f4 17] load current state (0..4) into a
00db:       jp      branch          ; [c3 6a 03] branch per state
            org     00deh\n00de: kinc: ld      hl,itab         ; [21 37 07] point hl to '+' key jump table (0737h)
00e1:       jr      prebr           ; [18 f5] branch per state
            org     00e3h\n00e3: kdec: ld      hl,dtab         ; [21 3e 07] point hl to '-' key jump table (073eh)
00e6:       jr      prebr           ; [18 f0] branch per state
            org     00e8h\n00e8: kexec:
00e8:       ld      hl,etab         ; [21 45 07] point hl to 'exec' key jump table (0745h)
00eb:       jr      prebr           ; [18 eb] branch per state
            org     00edh\n00ed: kdata:
00ed:       call    testm           ; [cd 37 05] verify display mode (err if not state 1 or 2)
00f0:       call    dform2          ; [cd 9b 03] display address & data, state = 2
00f3:       ret                     ; [c9]
            org     00f4h\n00f4: kadrs:
00f4:       call    dform1          ; [cd 92 03] display address format x.x.x.x.x x, state = 1
00f7:       ret                     ; [c9]
            org     00f8h\n00f8: ktapwr:
00f8:       call    stepdp          ; [cd b9 03] display tape parameter format x.x.x.x.- n
00fb:       ret                     ; [c9]
""")

    # 4. Hex Digit Key Handlers (00ff - 0136, pp. 170-171)
    sections.append("""; ==============================================================================
; SECTION 4: HEX DIGIT KEY HANDLERS PER STATE (00ffh - 0136h)
; ==============================================================================

            org     00ffh
00ff: hfix: jp      errdis          ; [c3 75 03] state 0 (fix mode): hex key is invalid
            org     0102h\n0102: hda:  ld      hl,(adsave)     ; [2a ee 17] state 2 (data mode): load current address
0105:       call    ramchk          ; [cd 9d 04] verify address is writable ram
0108:       jp      nz,errdis       ; [c2 75 03] if rom/unmapped, error
010b:       call    cl1byt          ; [cd 7c 03] clear data byte if test flag set
010e:       ld      a,c             ; [79] get hex digit from c
010f:       rld                     ; [ed 6f] rotate digit into low nibble of (hl)
0111:       call    dform2          ; [cd 9b 03] display address and data, state = 2
0114:       ret                     ; [c9]
            org     0116h\n0116: hda1: ld      hl,adsave       ; [21 ee 17] state 1 (adrs mode): point hl to adsave
0119:       call    cl2byt          ; [cd 89 03] clear address bytes if test flag set
011c:       ld      a,c             ; [79] get entered hex digit
011d:       rld                     ; [ed 6f] rotate into low nibble of (hl)
011f:       inc     hl              ; [23] advance to high byte of adsave
0120:       rld                     ; [ed 6f] rotate into high byte
0122:       call    dform1          ; [cd 92 03] display address format, state = 1
0125:       ret                     ; [c9]
            org     0127h\n0127: htapwr:
0127:       call    gtpalc          ; [cd d5 03] state 3/4 (tape mode): get parameter address
012a:       call    cl2byt          ; [cd 89 03] clear parameter if test flag set
012d:       ld      a,c             ; [79] get hex digit
012e:       rld                     ; [ed 6f] rotate into low byte
0130:       inc     hl              ; [23] advance to high byte
0131:       rld                     ; [ed 6f] rotate into high byte
0133:       call    stepdp          ; [cd b9 03] display tape parameter format
0136:       ret                     ; [c9]
""")

    # 5. '+' and '-' Function Key Handlers (013a - 017c, pp. 171-172)
    sections.append("""; ==============================================================================
; SECTION 5: '+' AND '-' FUNCTION KEY HANDLERS (013ah - 017ch)
; ==============================================================================

; '+' Key Handlers (ITAB dispatch)
            org     013ah
013a: ifix: jp      errdis          ; [c3 75 03] state 0 (fix mode): '+' is invalid
            org     013dh\n013d: adradd:
013d:       ld      hl,(adsave)     ; [2a ee 17] state 1/2: increment current memory address
0140:       inc     hl              ; [23] hl = hl + 1
0141:       ld      (adsave),hl     ; [22 ee 17] update adsave
0144:       call    dform2          ; [cd 9b 03] display updated address and data
0147:       ret                     ; [c9]
            org     0149h\n0149: tpfun:
0149:       ld      hl,stmoni       ; [21 f3 17] state 3/4: parameter counter (17f3h)
014c:       inc     (hl)            ; [34] increment parameter counter
014d:       call    gtpana          ; [cd e0 03] check if parameter index within range
0150:       jr      nz,istep        ; [20 04] if valid, display parameter
0152:       dec     (hl)            ; [35] out of range: restore counter
0153:       jp      errdis          ; [c3 75 03] show error
            org     0156h\n0156: istep:
0156:       call    stepdp          ; [cd b9 03] display parameter format
0159:       ret                     ; [c9]

; '-' Key Handlers (DTAB dispatch)
            org     015dh
015d: defix:
015d:       jp      errdis          ; [c3 75 03] state 0 (fix mode): '-' is invalid
            org     0160h\n0160: adrdec:
0160:       ld      hl,(adsave)     ; [2a ee 17] state 1/2: decrement current memory address
0163:       dec     hl              ; [2b] hl = hl - 1
0164:       ld      (adsave),hl     ; [22 ee 17] update adsave
0167:       call    dform2          ; [cd 9b 03] display updated address and data
016a:       ret                     ; [c9]
            org     016ch\n016c: tprun2:
016c:       ld      hl,stmoni       ; [21 f3 17] state 3/4: parameter counter
016f:       dec     (hl)            ; [35] decrement parameter counter
0170:       call    gtpana          ; [cd e0 03] check if parameter index within range
0173:       jr      nz,dstep        ; [20 04] if valid, display parameter
0175:       inc     (hl)            ; [34] out of range: restore counter
0176:       jp      errdis          ; [c3 75 03] show error
            org     0179h\n0179: dstep:
0179:       call    stepdp          ; [cd b9 03] display parameter format
017c:       ret                     ; [c9]
""")

    # 6. Cassette Tape File Management (0180 - 0226, pp. 173-176)
    sections.append("""; ==============================================================================
; SECTION 6: CASSETTE TAPE FILE MANAGEMENT & TAPE HEADERS (0180h - 0226h)
; ==============================================================================

; Execute Key Handler Dispatch Stubs (Page 173)
            org     0180h
0180: efix:   jp      errdis          ; [c3 75 03] state 0 (banner mode): exec is invalid -> error
0183: adrexc: push    hl              ; [e5] state 1: jump to address displayed on 7-segment leds
0184:       ld      hl,(adsave)     ; [2a ee 17] load currently displayed memory address
0187:       ex      (sp),hl         ; [e3] push target address onto stack
0188:       ret                     ; [c9] jump to target by return
            org     018ah
018a: endfun:
018a:       ld      (adsave),de     ; [ed 53 ee 17] save final address to adsave
018e:       call    dform2          ; [cd 9b 03] display address & data, transition state = 2
0191:       ret                     ; [c9]

; Tape Save Routine (TO TAPE / EWT, Pages 173 - 174)
; Writes leader tone, header block, inter-block gap, payload data block, and trailer tone
            org     0193h
0193: ewt:  call    sum             ; [cd 8f 04] calculate 8-bit checksum and verify range
0196:       jr      c,error         ; [38 2b] if block invalid, branch to error
0198:       ld      (17c5h),a       ; [32 c5 17] save checksum in tape buffer (dispbf-1)
019b:       ld      hl,0fa0h        ; [21 a0 0f] 4000 cycles of 1 khz leader tone
019e:       call    ms1k            ; [cd 70 04] generate 1 khz leader tone
01a1:       ld      hl,stepbf       ; [21 bf 17] point to header block in stepbf
01a4:       ld      bc,0007h        ; [01 07 00] 7 header bytes (filename, start, end, chksum)
01a7:       call    tapout          ; [cd 30 03] write header to tape
01aa:       ld      hl,0fa0h        ; [21 a0 0f] 4000 cycles of 2 khz inter-block tone
01ad:       call    ms2k            ; [cd 75 04] generate 2 khz carrier
01b0:       call    getptr          ; [cd bb 02] get data start (hl), end (de), length (bc)
01b3:       call    tapout          ; [cd 30 03] write payload data block to tape
01b6:       ld      hl,0fa0h        ; [21 a0 0f] 4000 cycles of 2 khz trailer tone
01b9:       call    ms2k            ; [cd 75 04] generate 2 khz finish tone
            org     01bch
01bc: endtap:
01bc:       ld      de,(17c3h)      ; [ed 5b c3 17] load end address
01c0:       jr      endfun          ; [18 c8] display completion and finish

            org     01c3h
01c3: error:
01c3:       ld      ix,errtab       ; [dd 21 d9 07] point ix to "-error" display pattern
01c7:       jp      setst0          ; [c3 94 00] restore state = 0

; Tape Load Routine (FROM TAPE / ERT, Pages 174 - 176)
            org     01ceh
01ce: ert:
01ce:       ld      hl,(stepbf)     ; [2a bf 17] load requested filename from stepbf (17bfh)
01d1:       ld      (temp),hl       ; [22 fa 17] preserve target filename in temp (17fah)
01d4: lead:
01d4:       ld      a,0bfh          ; [3e bf] display '-' character
01d6:       out     (segment),a     ; [d3 81] blank displays during search
01d8:       ld      hl,03e8h        ; [21 e8 03] wait for 1000 cycles (03e8h) of 1 khz tone
01db: lead1:
01db:       call    period          ; [cd 14 03] measure carrier cycle
01de:       jr      c,lead          ; [38 f4] if not 1 khz, restart search
01e0:       dec     hl              ; [2b] decrement cycle counter
01e1:       ld      a,h             ; [7c]
01e2:       or      l               ; [b5]
01e3:       jr      nz,lead1        ; [20 f6] wait until 1000 cycles pass
01e5: lead2:
01e5:       call    period          ; [cd 14 03] measure carrier cycle
01e8:       jr      nc,lead2        ; [30 fb] wait until carrier finishes
01ea:       ld      hl,stepbf       ; [21 bf 17] load header into stepbf
01ed:       ld      bc,0007h        ; [01 07 00] 7 header bytes
01f0:       call    tapein          ; [cd cf 02] read 7 header bytes from tape
01f3:       jr      c,lead          ; [38 df] if error, retry search
01f5:       ld      de,(stepbf)     ; [ed 5b bf 17] read file name
01f9:       call    adrsdp          ; [cd 04 05] format filename for display
01fc:       ld      b,96h           ; [06 96] display filename for 150 cycles (~1.5 seconds)
01fe: filedp:
01fe:       call    scd_k1          ; [cd cd 04] refresh display
0201:       djnz    filedp          ; [10 fb]
0203:       ld      hl,(temp)       ; [2a fa 17] compare requested filename
0206:       or      a               ; [b7] clear carry
0207:       sbc     hl,de           ; [ed 52] check if found requested file
0209:       jr      nz,lead         ; [20 c9] if mismatch, search for next file
020b:       ld      a,0fdh          ; [3e fd] found file: display indicator
020d:       out     (segment),a     ; [d3 81]
020f:       call    getptr          ; [cd bb 02] calculate target buffer address and length
0212:       jr      c,error         ; [38 af] if invalid parameters, jump to error
0214:       call    tapein          ; [cd cf 02] read data block into ram
0217:       jr      c,error         ; [38 aa] if read failed, error
0219:       call    sum             ; [cd 8f 04] calculate checksum of loaded data
021c:       ld      hl,17c5h        ; [21 c5 17] point to stored checksum
021f:       cp      (hl)            ; [be] compare checksum
0220:       jr      nz,error        ; [20 a1] if mismatch, error
0222:       jr      endtap          ; [18 98] success: display finish address
""")

    # 7. EPROM Programmer Routines (0227 - 02ce, pp. 176-180)
    sections.append("""; ==============================================================================
; SECTION 7: 2716 / 2732 EPROM PROGRAMMER INTERFACE (0227h - 02ceh)
; ==============================================================================

            org     0227h
0227: wr2716:
0227:       ld      ix,dispbf       ; [dd 21 c6 17] ix points to display buffer
022b:       push    bc              ; [c5] preserve parameters
022c:       push    hl              ; [e5]
022d:       ex      de,hl           ; [eb] calculate length
022e:       or      a               ; [b7] clear carry
022f:       sbc     hl,bc           ; [ed 42] hl = end - start
0231:       ld      c,l             ; [4d] length into bc
0232:       ld      b,h             ; [44]
0233:       ld      de,0f000h       ; [11 00 f0] test if length exceeds eprom capacity (4kb)
0236:       add     hl,de           ; [19]
0237:       jr      c,err_prg       ; [38 06] exceeded capacity -> error
0239:       pop     hl              ; [e1] restore start address
023a:       push    hl              ; [e5]
023b:       add     hl,de           ; [19] check end address boundary
023c:       jr      c,err_prg       ; [38 01]
023e:       add     hl,bc           ; [09] check overall length
023f: err_prg:
023f:       jp      c,error         ; [da c3 01] exceeded eprom address space
0242:       inc     bc              ; [03] normalize byte count
0243:       pop     de              ; [d1] eprom destination address
0244:       pop     hl              ; [e1] abc-80 source ram address
0245:       dec     a               ; [3d] check programming mode option (1=blank, 2=prog, 3=verify)
0246:       jr      z,tblank        ; [28 05] option 1: run blank check on eprom
0248:       dec     a               ; [3d]
0249:       jr      z,prog          ; [28 14] option 2: execute eprom burn
024b:       jr      verify          ; [18 21] option 3: verify eprom against ram

; EPROM Blank Check Loop (Ensure all bytes are 0xff)
            org     024dh\n024d: tblank:
024d:       push    de              ; [d5] preserve target address
024e:       push    bc              ; [c5] preserve byte count
024f: loop1:
024f:       call    datain          ; [cd 7e 02] read byte from eprom socket
0252:       cp      0ffh            ; [fe ff] compare with erased byte (0xff)
0254:       jr      z,testne        ; [28 01] if blank, continue to next address
0256:       halt                    ; [76] not blank: halt cpu!
0257: testne:
0257:       inc     de              ; [13] increment eprom address
0258:       dec     bc              ; [0b] decrement byte counter
0259:       ld      a,b             ; [78] check if bc == 0
025a:       or      c               ; [b1]
025b:       jr      nz,loop1        ; [20 f2] loop until all bytes verified blank
025d:       pop     bc              ; [c1] restore parameters
025e:       pop     de              ; [d1]
            org     025fh\n025f: prog: push    hl              ; [e5] save ram source
0260:       push    de              ; [d5] save eprom target
0261:       push    bc              ; [c5] save byte count
0262: loop2:
0262:       call    burn            ; [cd 90 02] apply 50ms programming pulse to current byte
0265:       inc     de              ; [13] advance eprom address
0266:       cpi                     ; [ed a1] increment hl, decrement bc, compare
0268:       jp      pe,loop2        ; [ea 62 02] loop until block complete
026b:       pop     bc              ; [c1] restore parameters for verification pass
026c:       pop     de              ; [d1]
026d:       pop     hl              ; [e1]

; Verify Programmed EPROM against RAM
            org     026eh\n026e: verify:
026e:       call    datain          ; [cd 7e 02] read byte from eprom socket
0271:       cpi                     ; [ed a1] compare with ram source byte (hl)
0273:       jr      z,vnext         ; [28 01] match -> advance
0275:       halt                    ; [76] verify mismatch -> halt cpu!
0276: vnext:
0276:       inc     de              ; [13] advance eprom address
0277:       jp      pe,verify       ; [ea 6e 02] loop until all bytes verified
027a:       rst     0               ; [c7] programming verified successful -> restart monitor

; Read Byte from EPROM Socket (Ports 40h-43h)
            org     027eh
027e: datain:
027e:       ld      a,82h           ; [3e 82] configure secondary 8255: pa=out, pb=in, pc=out
0280:       out     (q8255),a       ; [d3 43] port 43h
0282:       ld      a,e             ; [7b] output low address byte (a0-a7)
0283:       out     (addlow),a      ; [d3 40] port 40h
0285:       ld      a,d             ; [7a] output high address byte (a8-a11)
0286:       and     07h             ; [e6 07] mask 3 bits for 2716/2732
0288:       set     7,a             ; [cb ff] assert /ce high
028a:       out     (addhig),a      ; [d3 42] port 42h
028c:       in      a,(dain)        ; [db 41] read data byte from port 41h (pb)
028e:       ret                     ; [c9]

; Burn Single Byte into EPROM (50ms Vpp Programming Pulse)
            org     0290h
0290: burn:
0290:       push    bc              ; [c5] preserve registers
0291:       push    hl              ; [e5]
0292:       ld      a,80h           ; [3e 80] configure secondary 8255: all ports output
0294:       out     (q8255),a       ; [d3 43]
0296:       ld      a,(hl)          ; [7e] fetch data byte from source ram
0297:       out     (daot),a        ; [d3 41] output data byte to eprom pins
0299:       ld      a,e             ; [7b] output low address (a0-a7)
029a:       out     (addlow),a      ; [d3 40]
029c:       ld      a,d             ; [7a] output high address (a8-a11)
029d:       and     07h             ; [e6 07]
029f:       out     (addhig),a      ; [d3 42]
02a1:       ld      c,a             ; [4f] save address high bits
02a2:       or      60h             ; [f6 60] assert programming pulses (pc5, pc6 high)
02a4:       out     (cont),a        ; [d3 42]
02a6:       ld      a,(hl)          ; [7e] display burned data on 7-segment leds
02a7:       call    dadp            ; [cd 11 05]
02aa:       call    adrsdp          ; [cd 04 05] display burning address
02ad:       ld      b,05h           ; [06 05] 50ms pulse duration (5 * 10ms scan cycles)
02af: de50ms:
02af:       call    scd_k1          ; [cd cd 04] refresh display during burn pulse
02b2:       djnz    de50ms          ; [10 fb] loop 50ms
02b4:       ld      a,c             ; [79] deassert programming pulse (restore pc5/pc6 low)
02b5:       out     (addhig),a      ; [d3 42]
02b7:       pop     hl              ; [e1] restore registers
02b8:       pop     bc              ; [c1]
02b9:       ret                     ; [c9]

; Calculate Tape Block Length & Setup Pointers (Page 180)
; Inputs:  17c1h = start address, 17c3h = end address
; Outputs: HL = start address, DE = end address, BC = byte length, Carry = 0 (valid) / 1 (err)
            org     02bbh
02bb: getptr:
02bb:       ld      hl,17c1h        ; [21 c1 17] point hl to tape parameter start address
02be: getp:
02be:       ld      e,(hl)          ; [5e] load start address low byte
02bf:       inc     hl              ; [23]
02c0:       ld      d,(hl)          ; [56] load start address high byte
02c1:       inc     hl              ; [23]
02c2:       ld      c,(hl)          ; [4e] load end address low byte
02c3:       inc     hl              ; [23]
02c4:       ld      h,(hl)          ; [66] load end address high byte
02c5:       ld      l,c             ; [69] hl = end address
02c6:       or      a               ; [b7] clear carry flag
02c7:       sbc     hl,de           ; [ed 52] calculate difference: end - start
02c9:       ld      c,l             ; [4d] store length into bc
02ca:       ld      b,h             ; [44]
02cb:       inc     bc              ; [03] bc = length + 1
02cc:       ex      de,hl           ; [eb] restore hl = start address
02cd:       ret                     ; [c9] return with carry flag indicating validity
""")

    # 8. Cassette FSK Audio Interface (02cf - 0368, pp. 181-185)
    sections.append("""; ==============================================================================
; SECTION 8: CASSETTE TAPE FSK AUDIO MODULATOR / DEMODULATOR (02cfh - 0368h)
; ==============================================================================

; Read Byte Block from Cassette Audio (Page 181)
; Inputs: HL = destination RAM address, BC = byte count
; Output: Carry flag: 0 = success, 1 = read/parity error
            org     02cfh
02cf: tapein:
02cf:       xor     a               ; [af] clear carry flag (clean start)
02d0:       ex      af,af'          ; [08] save status in alternate af
02d1: tloop:
02d1:       call    gtbyte          ; [cd dd 02] read 1 byte from audio stream into e
02d4:       ld      (hl),e          ; [73] store received byte into ram destination
02d5:       cpi                     ; [ed a1] advance hl, decrement bc, test if bc==0
02d7:       jp      pe,tloop        ; [ea d1 02] loop until all bytes read
02da:       ex      af,af'          ; [08] retrieve error status
02db:       ret                     ; [c9]

; Read Single Byte from Cassette Audio (Page 181)
; Output: E = received data byte, Carry flag: 0 = valid, 1 = error
            org     02ddh\n02dd: gtbyte:
02dd:       call    getbit          ; [cd ef 02] synchronize with start bit
02e0:       ld      d,08h           ; [16 08] read 8 data bits
02e2: gloop:
02e2:       call    getbit          ; [cd ef 02] read next bit into carry flag
02e5:       rr      e               ; [cb 1b] rotate carry bit into register e
02e7:       dec     d               ; [15] decrement bit counter
02e8:       jr      nz,gloop        ; [20 f8] loop 8 bits
02ea:       call    getbit          ; [cd ef 02] read stop bit
02ed:       ret                     ; [c9]

; Read Single Bit from Audio Pulse Stream (Page 182)
; Output: Carry flag = demodulated bit value (0 or 1), Carry = 1 on timeout error
            org     02efh\n02ef: getbit:
02ef:       exx                     ; [d9] swap to alternate register set
02f0:       ld      hl,0000h        ; [21 00 00] initialize cycle accumulator
02f3: count:
02f3:       call    period          ; [cd 14 03] measure length of half-wave audio cycle
02f6:       inc     d               ; [14] check timeout in d register
02f7:       dec     d               ; [15]
02f8:       jr      nz,terr         ; [20 12] timeout -> error
02fa:       jr      c,shortp        ; [38 06] if short cycle (2 khz), jump to shortp
02fc:       dec     l               ; [2d] long cycle (1 khz): decrement l counter by 2
02fd:       dec     l               ; [2d]
02fe:       set     0,h             ; [cb c4] mark that 1 khz half has been seen
0300:       jr      count           ; [18 f1] loop for remaining cycle
0302: shortp:
0302:       inc     l               ; [2c] short cycle (2 khz): increment l counter
0303:       bit     0,h             ; [cb 44] check if full bit cycle complete
0305:       jr      z,count         ; [28 ec] loop until bit period complete
0307:       rl      l               ; [cb 15] shift bit sign into carry flag
0309:       exx                     ; [d9] swap back to main registers
030a:       ret                     ; [c9]
            org     030ch\n030c: terr: ex      af,af'          ; [08] set carry error flag
030d:       scf                     ; [37]
030e:       ex      af,af'          ; [08]
030f:       exx                     ; [d9] restore main registers
0310:       ret                     ; [c9]

; Measure Audio Waveform Cycle Duration (Page 183)
; Inputs: Port A bit 7 = cassette audio input (kin)
; Outputs: DE = cycle timing duration, Carry flag: 0 = 1khz (long), 1 = 2khz (short)
            org     0314h\n0314: period:
0314:       ld      de,0000h        ; [11 00 00] reset cycle timer
0317: chk0: in      a,(kin)         ; [db 80] read audio input from 8255 port a bit 7
0319:       inc     de              ; [13] increment timing counter
031a:       rla                     ; [17] shift bit 7 into carry
031b:       jr      c,chk0          ; [38 fa] wait while input is high
031d:       ld      a,01000000b     ; [3e 40] route audio feedback to speaker (bit 6)
031f:       out     (digit),a       ; [d3 82] play audio click through onboard speaker!
0321: chk1: in      a,(kin)         ; [db 80] read audio input
0323:       inc     de              ; [13] increment timer
0324:       rla                     ; [17]
0325:       jr      nc,chk1         ; [30 fa] wait while input is low
0327:       ld      a,0c0h          ; [3e c0] speaker feedback phase 2
0329:       out     (digit),a       ; [d3 82]
032b:       ld      a,e             ; [7b] compare measured half-period duration
032c:       cp      midpd           ; [fe 2a] compare against threshold (2ah = 42)
032e:       ret                     ; [c9] carry=1 if < 2ah (2khz short), carry=0 if >= 2ah (1khz long)

; Write Byte Block to Cassette Audio (Page 184)
; Inputs: HL = source RAM address, BC = byte length
            org     0330h\n0330: tapout:
0330:       ld      e,(hl)          ; [5e] fetch data byte from source buffer
0331:       call    otbyte          ; [cd 3b 03] modulate and write byte to tape
0334:       cpi                     ; [ed a1] advance hl, decrement bc
0336:       jp      pe,tapout       ; [ea 30 03] loop until block complete
0339:       ret                     ; [c9]

; Write Single Byte to Cassette Audio (Page 184)
            org     033bh\n033b: otbyte:
033b:       ld      d,08h           ; [16 08] 8 data bits per byte
033d:       or      a               ; [b7] clear carry flag (start bit = 0)
033e:       call    outbit          ; [cd 4f 03] write start bit (0)
0341: oloop:
0341:       rr      e               ; [cb 1b] shift lowest bit into carry flag
0343:       call    outbit          ; [cd 4f 03] modulate bit to tape
0346:       dec     d               ; [15] decrement bit count
0347:       jr      nz,oloop        ; [20 f8] loop 8 bits
0349:       scf                     ; [37] set carry flag = 1 (stop bit = 1)
034a:       call    outbit          ; [cd 4f 03] write stop bit (1)
034d:       ret                     ; [c9]

; Write Single Bit to Cassette Audio (Pages 184 - 185)
; Bit 0: 12 cycles of 2 khz tone + 3 cycles of 1 khz tone
; Bit 1:  6 cycles of 2 khz tone + 6 cycles of 1 khz tone
            org     034fh\n034f: outbit:
034f:       exx                     ; [d9] swap to alternate registers
0350:       ld      h,00h           ; [26 00]
0352:       jr      c,out1          ; [38 09] if bit == 1, jump to out1
0354: out0: ld      l,0ch           ; [2e 0c] bit 0: 12 cycles of 2 khz carrier
0356:       call    ms2k            ; [cd 75 04] generate 2 khz burst
0359:       ld      l,03h           ; [2e 03] bit 0: 3 cycles of 1 khz carrier
035b:       jr      bitend          ; [18 07]
035d: out1: ld      l,06h           ; [2e 06] bit 1: 6 cycles of 2 khz carrier
035f:       call    ms2k            ; [cd 75 04] generate 2 khz burst
0362:       ld      l,06h           ; [2e 06] bit 1: 6 cycles of 1 khz carrier
0364: bitend:
0364:       call    ms1k            ; [cd 70 04] generate 1 khz burst
0367:       exx                     ; [d9] restore main registers
0368:       ret                     ; [c9]
""")

    # 9. Branch Dispatch, Error Display & Formatters (036a - 03f5, pp. 185-188)
    sections.append("""; ==============================================================================
; SECTION 9: BRANCH DISPATCH, ERROR DISPLAY & FORMATTERS (036ah - 03f5h)
; ==============================================================================

; Indexed Subfunction Table Dispatch (Page 185)
; Inputs: HL = base address pointer table, A = state index (0..4)
            org     036ah\n036a: branch:
036a:       ld      e,(hl)          ; [5e] load low byte of base routine address
036b:       inc     hl              ; [23]
036c:       ld      d,(hl)          ; [56] load high byte of base routine address
036d:       inc     hl              ; [23] point hl to offset table
036e:       add     a,l             ; [85] add state index to pointer low byte
036f:       ld      l,a             ; [6f]
0370:       ld      l,(hl)          ; [6e] load 1-byte offset from table
0371:       ld      h,00h           ; [26 00]
0373:       add     hl,de           ; [19] target address = base address + offset
0374:       jp      (hl)            ; [e9] jump to indexed subfunction handler!

; Display "-ERROR" on 7-Segment LEDs (Page 185)
            org     0375h\n0375: errdis:
0375:       ld      hl,test         ; [21 f6 17] point to test status byte (17f6h)
0378:       set     7,(hl)          ; [cb fe] set bit 7 (scd_k displays -error automatically!)
037a:       ret                     ; [c9]

; Clear 1 Byte in Memory if Test Flag Set (Page 186)
            org     037ch\n037c: cl1byt:
037c:       ld      a,(test)        ; [3a f6 17] check test flag (bit 0 = first key press)
037f:       or      a               ; [b7]
0380:       ret     z               ; [c8] if zero, do not clear
0381:       ld      a,00h           ; [3e 00] clear accumulator
0383:       ld      (hl),a          ; [77] zero target memory byte
0384:       ld      (test),a        ; [32 f6 17] reset test flag to 0
0387:       ret                     ; [c9]

; Clear 2 Bytes (Address Word) in Memory (Page 186)
            org     0389h\n0389: cl2byt:
0389:       call    cl1byt          ; [cd 7c 03] clear low byte
038c:       ret     z               ; [c8] if not first key, done
038d:       inc     hl              ; [23] advance to high byte
038e:       ld      (hl),a          ; [77] zero high byte
038f:       dec     hl              ; [2b] restore pointer
0390:       ret                     ; [c9]

; Format Display Buffer: Address Mode x.x.x.x.x x (Page 186)
            org     0392h\n0392: dform1:
0392:       ld      a,01h           ; [3e 01] state = 1 (address entry mode)
0394:       ld      b,04h           ; [06 04] set 4 decimal points on address digits
0396:       ld      hl,17c8h        ; [21 c8 17] address digit buffer
0399:       jr      sav12           ; [18 07]

; Format Display Buffer: Data Mode xx x x.x. (Page 186)
            org     039bh\n039b: dform2:
039b:       ld      a,02h           ; [3e 02] state = 2 (data entry mode)
039d:       ld      b,02h           ; [06 02] set 2 decimal points on data digits
039f:       ld      hl,dispbf       ; [21 c6 17] data digit buffer (17c6h)
03a2: sav12:
03a2:       ld      (state),a       ; [32 f4 17] update state byte
03a5:       exx                     ; [d9] preserve registers
03a6:       ld      de,(adsave)     ; [ed 5b ee 17] load current address into de
03aa:       call    adrsdp          ; [cd 04 05] convert address to 4 7-segment digits
03ad:       ld      a,(de)          ; [1a] load data byte from memory (de)
03ae:       call    dadp            ; [cd 11 05] convert data byte to 2 7-segment digits
03b1:       exx                     ; [d9] restore registers
03b2: setpt:
03b2:       set     6,(hl)          ; [cb f6] turn on decimal point segment
03b4:       inc     hl              ; [23]
03b5:       djnz    setpt           ; [10 fb] loop for b decimal points
03b7:       ret                     ; [c9]

; Format Display Buffer: Tape Parameter Mode x.x.x.x.- n (Page 187)
            org     03b9h\n03b9: stepdp:
03b9:       call    gtpalc          ; [cd d5 03] get parameter address
03bc:       ld      e,(hl)          ; [5e] load parameter word into de
03bd:       inc     hl              ; [23]
03be:       ld      d,(hl)          ; [56]
03bf:       call    adrsdp          ; [cd 04 05] format parameter address
03c2:       ld      hl,17c8h        ; [21 c8 17] set 4 decimal points
03c5:       ld      b,04h           ; [06 04]
03c7:       call    setpt           ; [cd b2 03]
03ca:       call    gtpana          ; [cd e0 03] get parameter identifier
03cd:       ld      l,a             ; [6f]
03ce:       ld      h,02h           ; [26 02] '-' separator character
03d0:       ld      (dispbf),hl     ; [22 c6 17] display parameter label
03d3:       ret                     ; [c9]

; Get Tape Parameter Address in Memory (Page 188)
; Address = stepbf (17bfh) + stmoni * 2
            org     03d5h\n03d5: gtpalc:
03d5:       ld      a,(stmoni)      ; [3a f3 17] read parameter counter (0..2)
03d8:       add     a,a             ; [87] multiply by 2 (word offset)
03d9:       ld      hl,stepbf       ; [21 bf 17] base address = 17bfh
03dc:       add     a,l             ; [85]
03dd:       ld      l,a             ; [6f] hl = stepbf + stmoni * 2
03de:       ret                     ; [c9]

; Get Tape Parameter Label (Page 188)
            org     03e0h\n03e0: gtpana:
03e0:       ld      a,(state)       ; [3a f4 17] read state (3=to tape, 4=from tape)
03e3:       sub     01h             ; [d6 01]
03e5:       add     a,a             ; [87] multiply by 4
03e6:       add     a,a             ; [87]
03e7:       ld      de,blank        ; [11 e0 07] point to parameter label table (07e0h)
03ea:       add     a,e             ; [83]
03eb:       ld      e,a             ; [5f]
03ec:       ld      a,(stmoni)      ; [3a f3 17] add parameter counter
03ef:       add     a,e             ; [83]
03f0:       ld      e,a             ; [5f]
03f1:       ld      a,(de)          ; [1a] load label character
03f2:       or      a               ; [b7] test if zero (end of parameters)
03f3:       ret                     ; [c9]
""")

    # 10. Display Initialization & Music Subroutines (03f6 - 0503, pp. 189-196)
    sections.append("""; ==============================================================================
; SECTION 10: BANNER DISPLAY & MUSIC SYNTHESIZER (03f6h - 0503h)
; ==============================================================================

; Display Power-On Scrolling Banner ("HELLO THIS IS ABC-80")
            org     03f6h
03f6: inidp:
03f6:       ld      ix,initab+1     ; [dd 21 d3 07] point ix to banner start + 1
03fa:       ld      c,15h           ; [0e 15] 21 scroll steps
03fc: inidp1:
03fc:       ld      b,20h           ; [06 20] display multiplex delay counter (32 cycles)
03fe: inidp2:
03fe:       call    scd_k1          ; [cd cd 04] scan display
0401:       djnz    inidp2          ; [10 fb] loop delay
0403:       dec     ix              ; [dd 2b] scroll banner left by 1 digit
0405:       call    ms3k17          ; [cd 64 04] sound click/beep while scrolling
0408:       dec     c               ; [0d] decrement scroll counter
0409:       jr      nz,inidp1       ; [20 f1] loop until banner fully scrolled across
040b:       ret                     ; [c9] done

; Play Opening Melody (Pages 189 - 190)
            org     040dh
040d: monsou:
040d:       ld      iy,song         ; [fd 21 58 05] point to opening melody score (0558h)
0411: music:
0411:       push    iy              ; [fd e5]
0413:       pop     ix              ; [dd e1] ix points to current note pair (pitch, duration)
0415:       ld      a,(ix+00h)      ; [dd 7e 00] load pitch index / control byte
0418:       add     a,a             ; [87] word offset (2 bytes per note)
0419:       jr      c,stop          ; [38 30] bit 7 set (80h) -> stop / melody complete
041b:       jp      m,music         ; [fa 11 04] bit 6 set (repeat marker) -> loop from start
041e:       ld      c,00h           ; [0e 00] initialize c = 00h (silent rest)
0420:       bit     6,a             ; [cb 77] test rest bit (bit 5 in original note, shifted to bit 6 by add a,a)
0422:       jr      nz,play         ; [20 02] if rest (20h), leave c = 00h (silent)
0424:       set     7,c             ; [cb f9] not rest -> set bit 7 of c (toggle speaker pin)
0426: play:
0426:       and     3fh             ; [e6 3f] mask pitch table index
0428:       ld      hl,frqtab       ; [21 4c 07] point to frequency table (074ch)
042b:       add     a,l             ; [85]
042c:       ld      l,a             ; [6f]
042d:       ld      e,(hl)          ; [5e] e = half-period timer constant
042e:       inc     hl              ; [23]
042f:       ld      d,(hl)          ; [56] d = half-period cycle count per duration unit
0430:       inc     ix              ; [dd 23] advance ix to duration byte
0432:       ld      h,(ix+00h)      ; [dd 66 00] h = note duration
0435:       ld      a,0ffh          ; [3e ff] initial speaker output state
0437: tone:
0437:       ld      l,d             ; [6a] l = half-period cycle counter
0438: tonout:
0438:       out     (digit),a       ; [d3 82] output to speaker (port c bit 7)
043a:       ld      b,e             ; [43] b = half-period delay constant
043b: delay:
043b:       nop                     ; [00]
043c:       nop                     ; [00]
043d:       nop                     ; [00]
043e:       djnz    delay           ; [10 fb] half-period delay
0440:       xor     c               ; [a9] toggle bit 7 if c has bit 7 set; stay constant if rest
0441:       dec     l               ; [2d] decrement half-period cycle counter
0442:       jr      nz,tonout       ; [20 f4]
0444:       dec     h               ; [25] decrement duration counter
0445:       jr      nz,tone         ; [20 f0]
0447:       inc     ix              ; [dd 23] advance ix to next note pair
0449:       jr      0415h           ; [18 ca] fetch next note
044b: stop:
044b:       ret                     ; [c9]

; Tone Generator Auxiliary Wrappers (Pages 190 - 191)
            org     0450h
0450: ms1k17:
0450:       exx                     ; [d9] save alternate registers
0451:       ld      hl,0050h        ; [21 50 00] duration cycle count
0454:       call    ms1k            ; [cd 70 04] generate 1 khz tone
0457:       exx                     ; [d9] restore alternate registers
0458:       ret                     ; [c9]

            org     045ah
045a: ms2k17:
045a:       exx                     ; [d9]
045b:       ld      hl,00a0h        ; [21 a0 00] duration cycle count
045e:       call    ms2k            ; [cd 75 04] generate 2 khz tone
0461:       exx                     ; [d9]
0462:       ret                     ; [c9]

            org     0464h
0464: ms3k17:
0464:       ex      af,af'          ; [08] preserve flags
0465:       exx                     ; [d9] preserve registers
0466:       ld      hl,00e0h        ; [21 e0 00] duration cycle count
0469:       call    ms3k            ; [cd 7a 04] generate 3 khz tone
046c:       exx                     ; [d9] restore registers
046d:       ex      af,af'          ; [08] restore flags
046e:       ret                     ; [c9]

; Tone Generators: ms1k, ms2k, ms3k (Pages 191 - 192)
            org     0470h
0470: ms1k:
0470:       ld      c,41h           ; [0e 41] 1 khz half-period constant
0472:       jr      sound           ; [18 08]

            org     0475h
0475: ms2k:
0475:       ld      c,1fh           ; [0e 1f] 2 khz half-period constant
0477:       jr      sound           ; [18 03]

            org     047ah
047a: ms3k:
047a:       ld      c,0ch           ; [0e 0c] 3 khz half-period constant
047c: sound:
047c:       add     hl,hl           ; [29] convert full periods to half periods
047d:       ld      de,0001h        ; [11 01 00]
0480:       ld      a,0ffh          ; [3e ff]
0482: sqwave:
0482:       out     (digit),a       ; [d3 82] toggle speaker pin (port c bit 7)
0484:       ld      b,c             ; [41] load half-period timer
0485:       djnz    $               ; [10 fe] delay
0487:       xor     80h             ; [ee 80] toggle bit 7
0489:       sbc     hl,de           ; [ed 52] decrement period counter
048b:       jr      nz,sqwave       ; [20 f5]
048d:       ret                     ; [c9]

; Calculate 8-Bit Checksum over Memory Block (Page 192)
; Inputs:  HL = start address, BC = length
; Outputs: A = 8-bit cumulative checksum
            org     048fh
048f: sum:
048f:       call    getptr          ; [cd bb 02] get start/end pointers from step buffer
0492:       ret     c               ; [d8] error if invalid parameters
0493:       xor     a               ; [af] clear checksum accumulator
0494: sumcal:
0494:       add     a,(hl)          ; [86] add memory byte
0495:       cpi                     ; [ed a1] advance hl, decrement bc, compare
0497:       jp      pe,sumcal       ; [ea 94 04] loop while bc > 0
049a:       or      a               ; [b7] clear carry flag
049b:       ret                     ; [c9]

; Dynamic RAM Detection Routine (Pages 192 - 193)
; Returns: Z = RAM writeable, NZ = ROM / unmapped
            org     049dh
049d: ramchk:
049d:       ld      a,(hl)          ; [7e] read original byte
049e:       cpl                     ; [2f] invert bits
049f:       ld      (hl),a          ; [77] write inverted byte
04a0:       ld      a,(hl)          ; [7e] read back
04a1:       cpl                     ; [2f] invert back
04a2:       ld      (hl),a          ; [77] restore original byte
04a3:       cp      (hl)            ; [be] verify memory accepted write
04a4:       ret                     ; [c9]

; Display Multiplexing & Keyboard Scanner (Pages 193 - 194)
; Continuously multiplexes 6-digit display until valid key pressed
            org     04a6h
04a6: scd_k:
04a6:       push    ix              ; [dd e5] save display buffer pointer
04a8:       ld      hl,test         ; [21 f6 17] check error flag
04ab:       bit     7,(hl)          ; [cb 7e] test bit 7 (input error)
04ad:       jr      z,scpre         ; [28 04] no error -> normal display
04af:       ld      ix,errtab       ; [dd 21 d9 07] point ix to error message ("-ERROR")
04b3: scpre:
04b3:       ld      b,04h           ; [06 04] wait 4 passes (40ms debounce) for key release
04b5: scnx:
04b5:       call    scd_k1          ; [cd cd 04] scan display & keyboard
04b8:       jr      nc,scpre        ; [30 f9] if key still pressed, restart debounce
04ba:       djnz    scnx            ; [10 f9] debounce delay
04bc:       res     7,(hl)          ; [cb be] clear error flag
04be:       pop     ix              ; [dd e1] restore original display buffer pointer
04c0: scloop:
04c0:       call    scd_k1          ; [cd cd 04] scan display & keyboard
04c3:       jr      c,scloop        ; [38 fb] wait until key is pressed (c = 0)
04c5: keymap:
04c5:       ld      hl,keytab       ; [21 8c 07] point hl to key translation table
04c8:       add     a,l             ; [85] index by matrix position code
04c9:       ld      l,a             ; [6f]
04ca:       ld      a,(hl)          ; [7e] load internal monitor key code
04cb:       ret                     ; [c9]

; Single Display Refresh & Keyboard Matrix Scan Pass (Pages 194 - 196)
; Inputs:  IX = display buffer pointer (dispbf at 17c6h)
; Outputs: Carry = 1 (no key pressed), Carry = 0 (key pressed, A = position code)
            org     04cdh
04cd: scd_k1:
04cd:       scf                     ; [37] default: carry = 1 (no key pressed)
04ce:       ex      af,af'          ; [08] save default carry flag
04cf:       exx                     ; [d9] save registers
04d0:       ld      c,00h           ; [0e 00] initialize position code counter
04d2:       ld      e,0feh          ; [1e fe] digit 0 select bitmask (active low, port c bit 0)
04d4: kcol:
04d4:       ld      a,(ix+00h)      ; [dd 7e 00] load 7-segment bitmask for current digit
04d7:       cpl                     ; [2f] invert for active-low cathode driver
04d8:       out     (segment),a     ; [d3 81] output to port b (segment bus)
04da:       ld      a,e             ; [7b] load digit select bitmask
04db:       out     (digit),a       ; [d3 82] output to port c (digit select)
04dd:       ld      b,09h           ; [06 09] digit dwell delay loop (9 iterations)
04df:       djnz    $               ; [10 fe]
04e1:       ld      b,06h           ; [06 06] 6 key rows to scan
04e3:       in      a,(keypad)      ; [db 80] read row inputs from port a
04e5:       ld      d,a             ; [57]
04e6: krow:
04e6:       rr      d               ; [cb 1a] shift row bit into carry
04e8:       jr      c,nokey         ; [38 02] 1 = not pressed -> next row
04ea:       ld      a,c             ; [79] 0 = pressed -> captured position code
04eb:       ex      af,af'          ; [08] store position code and carry = 0 in af'
04ec: nokey:
04ec:       inc     c               ; [0c] advance position code
04ed:       djnz    krow            ; [10 f7] test all 6 rows in column
04ef:       inc     ix              ; [dd 23] advance to next display digit
04f1:       rlc     e               ; [cb 03] rotate digit select bit to next column
04f3:       ld      a,0ffh          ; [3e ff] blank display between digits (prevents ghosting)
04f5:       out     (digit),a       ; [d3 82]
04f7:       bit     6,e             ; [cb 73] test if all 6 digits completed (bit 6 active)
04f9:       jr      nz,kcol         ; [20 d9] loop until 6 digits scanned
04fb:       ld      de,-6           ; [11 fa ff] restore ix back to start of display buffer
04fe:       add     ix,de           ; [dd 19]
0500:       exx                     ; [d9] restore alternate registers
0501:       ex      af,af'          ; [08] restore carry flag and captured key code in a
0502:       ret                     ; [c9]
""")

    # 11. Font Converters & System Initialization (0504 - 0557, pp. 196-198)
    sections.append("""; ==============================================================================
; SECTION 11: DISPLAY FONT ENCODERS & BOOTSTRAP INITIALIZATION (0504h - 0557h)
; ==============================================================================

; Convert 16-Bit Address in DE to 4 7-Segment Display Digits (Pages 196 - 197)
; Writes 4 segment bytes to dispbf+2 .. dispbf+5 (17c8h - 17cbh)
            org     0504h
0504: adrsdp:
0504:       ld      hl,17c8h        ; [21 c8 17] point to address display buffer (dispbf+2)
0507:       ld      a,e             ; [7b] convert low byte (e)
0508:       call    tobyseg         ; [cd 25 05] convert byte to 2 display digits
050b:       ld      a,d             ; [7a] convert high byte (d)
050c:       call    tobyseg         ; [cd 25 05] convert byte to 2 display digits
050f:       ret                     ; [c9]

; Convert 8-Bit Data in A to 2 7-Segment Display Digits (Page 196)
; Writes 2 segment bytes to dispbf .. dispbf+1 (17c6h - 17c7h)
            org     0511h
0511: dadp:
0511:       ld      hl,dispbf       ; [21 c6 17] point to data display buffer (17c6h)
0514:       call    tobyseg         ; [cd 25 05] convert byte in a to 2 display digits
0517:       ret                     ; [c9]

; Convert Low Nibble in A to 7-Segment LED Bitmask (Page 197)
; Inputs:  A = nibble (0..F)
; Outputs: A = 7-segment bitmask from segtab
            org     0519h
0519: onbyseg:
0519:       push    hl              ; [e5] preserve hl
051a:       ld      hl,segtab       ; [21 f0 07] point hl to hex font table (07f0h)
051d:       and     0fh             ; [e6 0f] isolate lower 4 bits (hex digit 0..f)
051f:       add     a,l             ; [85] index into font table
0520:       ld      l,a             ; [6f]
0521:       ld      a,(hl)          ; [7e] load 7-segment bitmask
0522:       pop     hl              ; [e1] restore hl
0523:       ret                     ; [c9]

; Convert Byte in A into 2 7-Segment Font Digits (Page 197)
; Low nibble -> (HL), High nibble -> (HL+1), advances HL by 2
            org     0525h
0525: tobyseg:
0525:       push    af              ; [f5] preserve byte
0526:       call    onbyseg         ; [cd 19 05] convert low nibble
0529:       ld      (hl),a          ; [77] store in (hl)
052a:       inc     hl              ; [23] advance pointer
052b:       pop     af              ; [f1] restore byte
052c:       rrca                    ; [0f] rotate high nibble into low 4 bits
052d:       rrca                    ; [0f]
052e:       rrca                    ; [0f]
052f:       rrca                    ; [0f]
0530:       call    onbyseg         ; [cd 19 05] convert high nibble
0533:       ld      (hl),a          ; [77] store in (hl+1)
0534:       inc     hl              ; [23] advance pointer (hl now advanced by 2)
0535:       ret                     ; [c9]

; Verify Current System State for 'DATA' Key (Page 198)
; Only valid in State 1 (ADRS mode) or State 2 (DATA mode)
            org     0537h
0537: testm:
0537:       ld      a,(state)       ; [3a f4 17] check current state
053a:       cp      01h             ; [fe 01] test state == 1 (adrs)
053c:       ret     z               ; [c8] state 1 is valid -> return
053d:       cp      02h             ; [fe 02] test state == 2 (data)
053f:       ret     z               ; [c8] state 2 is valid -> return
0540:       pop     hl              ; [e1] invalid state: discard caller return address
0541:       jp      errdis          ; [c3 75 03] jump directly to error display!

; Play System Reset Melody (Page 198)
            org     0544h
0544: rstmu:
0544:       ld      iy,rmusic       ; [fd 21 f2 05] load pointer to 11-byte reset chime (05f2h)
0548:       call    music           ; [cd 11 04] play reset melody
054b:       ret                     ; [c9]

; Cold Boot Hardware & RAM Initialization (Pages 198 - 199)
            org     054ch
054c: init:
054c:       call    inidp           ; [cd f6 03] scroll "hello this is abc-80" banner
054f:       call    monsou          ; [cd 0d 04] play opening song
0552:       ld      a,pwcode        ; [3e 80] load magic signature (80h)
0554:       ld      (pwup),a        ; [32 f5 17] set power-on flag in 17f5h
0557:       ret                     ; [c9]
""")

    # 12. Music Scores & Note Tables (0558 - 05fc, pp. 198-200)
    sections.append("""; ==============================================================================
; SECTION 12: MUSIC SCORES & MELODY DATA TABLES (0558h - 05fch)
; ==============================================================================

; Opening Cold-Start Melody Score ("Song", Pages 198 - 200)
; 154-byte note table: Pairs of (Pitch Index, Duration Factor)
            org     0558h
0558: song:
0558:       db      05h, 08h        ; note 1
055a:       db      05h, 08h        ; note 2
055c:       db      05h, 10h        ; note 3
055e:       db      05h, 08h        ; note 4
0560:       db      05h, 08h        ; note 5
0562:       db      05h, 10h        ; note 6
0564:       db      05h, 08h        ; note 7
0566:       db      08h, 08h        ; note 8
0568:       db      01h, 0ch        ; note 9
056a:       db      03h, 04h        ; note 10
056c:       db      05h, 20h        ; note 11
056e:       db      06h, 08h        ; note 12
0570:       db      06h, 08h        ; note 13
0572:       db      06h, 0ch        ; note 14
0574:       db      06h, 04h        ; note 15
0576:       db      06h, 08h        ; note 16
0578:       db      05h, 08h        ; note 17
057a:       db      05h, 08h        ; note 18
057c:       db      05h, 04h        ; note 19
057e:       db      05h, 04h        ; note 20
0580:       db      05h, 08h        ; note 21
0582:       db      03h, 08h        ; note 22
0584:       db      03h, 08h        ; note 23
0586:       db      05h, 08h        ; note 24
0588:       db      03h, 10h        ; note 25
058a:       db      08h, 10h        ; note 26
058c:       db      05h, 08h        ; note 27
058e:       db      05h, 08h        ; note 28
0590:       db      05h, 10h        ; note 29
0592:       db      05h, 08h        ; note 30
0594:       db      05h, 08h        ; note 31
0596:       db      05h, 10h        ; note 32
0598:       db      05h, 08h        ; note 33
059a:       db      08h, 08h        ; note 34
059c:       db      01h, 0ch        ; note 35
059e:       db      03h, 04h        ; note 36
05a0:       db      05h, 20h        ; note 37
05a2:       db      06h, 08h        ; note 38
05a4:       db      06h, 08h        ; note 39
05a6:       db      06h, 0ch        ; note 40
05a8:       db      06h, 04h        ; note 41
05aa:       db      06h, 08h        ; note 42
05ac:       db      05h, 08h        ; note 43
05ae:       db      05h, 08h        ; note 44
05b0:       db      05h, 04h        ; note 45
05b2:       db      05h, 04h        ; note 46
05b4:       db      08h, 08h        ; note 47
05b6:       db      08h, 08h        ; note 48
05b8:       db      06h, 08h        ; note 49
05ba:       db      03h, 08h        ; note 50
05bc:       db      01h, 20h        ; note 51
05be:       db      08h, 08h        ; note 52
05c0:       db      08h, 08h        ; note 53
05c2:       db      08h, 10h        ; note 54
05c4:       db      06h, 08h        ; note 55
05c6:       db      05h, 08h        ; note 56
05c8:       db      03h, 10h        ; note 57
05ca:       db      08h, 08h        ; note 58
05cc:       db      06h, 08h        ; note 59
05ce:       db      05h, 08h        ; note 60
05d0:       db      03h, 08h        ; note 61
05d2:       db      02h, 20h        ; note 62
05d4:       db      08h, 08h        ; note 63
05d6:       db      08h, 08h        ; note 64
05d8:       db      08h, 10h        ; note 65
05da:       db      06h, 08h        ; note 66
05dc:       db      05h, 08h        ; note 67
05de:       db      03h, 10h        ; note 68
05e0:       db      05h, 08h        ; note 69
05e2:       db      03h, 08h        ; note 70
05e4:       db      02h, 08h        ; note 71
05e6:       db      05h, 08h        ; note 72
05e8:       db      01h, 20h        ; note 73
05ea:       db      00h, 00h        ; note 74
05ec:       db      00h, 00h        ; note 75
05ee:       db      80h, 0ffh       ; end of song score marker (80h)

; Reset Chime Melody Score ("Rmusic", Page 200)
; 11-byte note table played during warm reset
            org     05f2h
05f2: rmusic:
05f2:       db      01h, 08h        ; chime note 1
05f4:       db      03h, 08h        ; chime note 2
05f6:       db      05h, 08h        ; chime note 3
05f8:       db      08h, 0ch        ; chime note 4
05fa:       db      06h, 04h        ; chime note 5
05fc:       db      80h             ; end of reset chime marker (80h)
""")

    # 13. Subfunction Jump Tables & System Data (0725 - 07e7, pp. 201-204)
    sections.append("""; ==============================================================================
; SECTION 13: SUBFUNCTION JUMP TABLES & FONT BITMASKS (0725h - 07e7h)
; ==============================================================================

; Function Key Base & Offset Dispatch Tables (Pages 201 - 202)
; 16-bit base address followed by 1-byte delta offsets per state
            org     0725h
0725: subfun:
0725:       dw      kinc            ; [de 00] '+' key base address (00deh)
0727:       db      00h             ; '+' key offset (00de + 0 = 00deh: kinc)
0728:       db      05h             ; '-' key offset (00de + 5 = 00e3h: kdec)
0729:       db      0ah             ; 'exec' key offset (00de + 0ah = 00e8h: kexec)
072a:       db      0fh             ; 'data' key offset (00de + 0fh = 00edh: kdata)

            org     072bh\n072b: func:
072b:       dw      kadrs           ; [f4 00] 'adrs' key base address (00f4h)
072d:       db      00h             ; 'adrs' key offset (00f4 + 0 = 00f4h: kadrs)
072e:       db      04h             ; 'to tape' key offset (00f4 + 4 = 00f8h: ktapwr)
072f:       db      04h             ; 'from tape' key offset (00f4 + 4 = 00f8h: ktapwr)

            org     0730h\n0730: htab:
0730:       dw      hfix            ; [ff 00] hex key base address (00ffh)
0732:       db      00h             ; state 0 (fix mode)  -> hfix   (00ff + 00h = 00ffh)
0733:       db      17h             ; state 1 (adrs mode) -> hda1   (00ff + 17h = 0116h)
0734:       db      03h             ; state 2 (data mode) -> hda    (00ff + 03h = 0102h)
0735:       db      28h             ; state 3 (to tape)   -> htapwr (00ff + 28h = 0127h)
0736:       db      28h             ; state 4 (from tape) -> htapwr (00ff + 28h = 0127h)

            org     0737h\n0737: itab:
0737:       dw      ifix            ; [3a 01] '+' key base address (013ah)
0739:       db      00h             ; state 0 (fix mode)  -> ifix   (013a + 00h = 013ah)
073a:       db      03h             ; state 1 (adrs mode) -> adradd (013a + 03h = 013dh)
073b:       db      03h             ; state 2 (data mode) -> adradd (013a + 03h = 013dh)
073c:       db      0fh             ; state 3 (to tape)   -> tpfun  (013a + 0fh = 0149h)
073d:       db      0fh             ; state 4 (from tape) -> tpfun  (013a + 0fh = 0149h)

            org     073eh\n073e: dtab:
073e:       dw      defix           ; [5d 01] '-' key base address (015dh)
0740:       db      00h             ; state 0 (fix mode)  -> defix  (015d + 00h = 015dh)
0741:       db      03h             ; state 1 (adrs mode) -> adrdec (015d + 03h = 0160h)
0742:       db      03h             ; state 2 (data mode) -> adrdec (015d + 03h = 0160h)
0743:       db      0fh             ; state 3 (to tape)   -> tprun2 (015d + 0fh = 016ch)
0744:       db      0fh             ; state 4 (from tape) -> tprun2 (015d + 0fh = 016ch)

            org     0745h\n0745: etab:
0745:       dw      efix            ; [80 01] 'exec' key base address (0180h)
0747:       db      00h             ; state 0 (fix mode)  -> efix   (0180 + 00h = 0180h)
0748:       db      03h             ; state 1 (adrs mode) -> adrexc (0180 + 03h = 0183h)
0749:       db      03h             ; state 2 (data mode) -> adrexc (0180 + 03h = 0183h)
074a:       db      13h             ; state 3 (to tape)   -> ewt    (0180 + 13h = 0193h)
074b:       db      4eh             ; state 4 (from tape) -> ert    (0180 + 4eh = 01ceh)

; Frequency Lookup Table (Pages 202 - 203)
; Half-period timer constants for musical notes
            org     074ch\n074c: frqtab:
074c:       dw      18e1h           ; note 1
074e:       dw      1ad4h           ; note 2
0750:       dw      1b08h           ; note 3
0752:       dw      1dbdh           ; note 4
0754:       dw      1eb2h           ; note 5
0756:       dw      20a8h           ; note 6
0758:       dw      229fh           ; note 7
075a:       dw      2496h           ; note 8
075c:       dw      268dh           ; note 9
075e:       dw      2985h           ; note 10
0760:       dw      2b7eh           ; note 11
0762:       dw      2e77h           ; note 12
0764:       dw      3170h           ; note 13
0766:       dw      336ah           ; note 14
0768:       dw      3764h           ; note 15
076a:       dw      3a5eh           ; note 16
076c:       dw      3d59h           ; note 17
076e:       dw      4154h           ; note 18
0770:       dw      454eh           ; note 19
0772:       dw      494ah           ; note 20
0774:       dw      4d46h           ; note 21
0776:       dw      5242h           ; note 22
0778:       dw      573eh           ; note 23
077a:       dw      5c3bh           ; note 24
077c:       dw      6237h           ; note 25
077e:       dw      6734h           ; note 26
0780:       dw      6e31h           ; note 27
0782:       dw      742eh           ; note 28
0784:       dw      7b2ch           ; note 29
0786:       dw      8229h           ; note 30
0788:       dw      8a27h           ; note 31
078a:       dw      9225h           ; note 32

; Key Scancode Translation Table (Page 203)
; Maps 4x6 hardware key matrix switch positions to internal key codes (00h - 16h)
            org     078ch
078c: keytab:
078c:       db      14h             ; 'adrs'
078d:       db      13h             ; 'data'
078e:       db      11h             ; 'dec' ('-')
078f:       db      10h             ; 'inc' ('+')
0790:       db      0ffh            ; unused
0791:       db      0ffh            ; unused
0792:       db      0fh             ; 'f'
0793:       db      0bh             ; 'b'
0794:       db      07h             ; '7'
0795:       db      03h             ; '3'
0796:       db      0ffh            ; unused
0797:       db      0ffh            ; unused
0798:       db      0eh             ; 'e'
0799:       db      0ah             ; 'a'
079a:       db      06h             ; '6'
079b:       db      02h             ; '2'
079c:       db      0ffh            ; unused
079d:       db      0ffh            ; unused
079e:       db      0dh             ; 'd'
079f:       db      09h             ; '9'
07a0:       db      05h             ; '5'
07a1:       db      01h             ; '1'
07a2:       db      0ffh            ; unused
07a3:       db      0ffh            ; unused
07a4:       db      0ch             ; 'c'
07a5:       db      08h             ; '8'
07a6:       db      04h             ; '4'
07a7:       db      00h             ; '0'
07a8:       db      0ffh            ; unused
07a9:       db      0ffh            ; unused
07aa:       db      0ffh            ; unused
07ab:       db      15h             ; 'to tape'
07ac:       db      16h             ; 'from tape'
07ad:       db      12h             ; 'exec'

; 7-Segment LED Font Table & Special Display Patterns (Pages 203 - 204)
            org     07bfh
07bf: disp:
07bf:       db      0bdh            ; '0'
07c0:       db      0bfh            ; '8'
07c1:       db      02h             ; '-'
07c2:       db      8dh             ; 'c'
07c3:       db      0a7h            ; 'b'
07c4:       db      0efh            ; 'a'

; Special Message Bitmask Patterns (Page 204)
07c5:       db      00h             ; ' '
07c6:       db      0aeh            ; 's'
07c7:       db      30h             ; 'i'
07c8:       db      00h             ; ' '
07c9:       db      0aeh            ; 's'
07ca:       db      30h             ; 'i'
07cb:       db      37h             ; 'h'
07cc:       db      87h             ; 't'
07cd:       db      00h             ; ' '
07ce:       db      0bdh            ; 'o'
07cf:       db      85h             ; 'l'
07d0:       db      85h             ; 'l'
07d1:       db      8fh             ; 'e'

; Power-On Display Format ("H      ", Page 204)
            org     07d2h\n07d2: initab:
07d2:       db      37h             ; 'h' (init display format)
07d3:       db      00h             ; ' '
07d4:       db      00h             ; ' '
07d5:       db      00h             ; ' '
07d6:       db      00h             ; ' '
07d7:       db      00h             ; ' '
07d8:       db      00h             ; ' '

; Error Display Format ("-ERROR", Page 204)
            org     07d9h\n07d9: errtab:
07d9:       db      03h             ; 'r' (digit 0)
07da:       db      0a3h            ; 'o' (digit 1)
07db:       db      03h             ; 'r' (digit 2)
07dc:       db      03h             ; 'r' (digit 3)
07dd:       db      8fh             ; 'e' (digit 4)
07de:       db      02h             ; '-' (digit 5)
07df:       db      00h             ; ' '

; Blank Display Format (Page 204)
            org     07e0h
07e0: blank:
07e0:       db      00h             ; ' '
07e1:       db      00h             ; ' '
07e2:       db      00h             ; ' '
07e3:       db      00h             ; ' '
07e4:       db      00h             ; ' '
07e5:       db      00h             ; ' '
07e6:       db      00h             ; ' '
07e7:       db      00h             ; ' '

; Hexadecimal 7-Segment Font Table (07f0h - 07ffh, 16 bytes for digits 0..F)
; Hardware Port B (Active High in Table, Inverted in Driver):
;   bit 7 = segment d
;   bit 6 = segment dp
;   bit 5 = segment c
;   bit 4 = segment b
;   bit 3 = segment a
;   bit 2 = segment f
;   bit 1 = segment g
;   bit 0 = segment e
            org     07f0h
07f0: segtab:
07f0:       db      0bdh            ; '0' : a,b,c,d,e,f
07f1:       db      30h             ; '1' : b,c
07f2:       db      9bh             ; '2' : a,b,d,e,g
07f3:       db      0bah            ; '3' : a,b,c,d,g
07f4:       db      36h             ; '4' : b,c,f,g
07f5:       db      0aeh            ; '5' : a,c,d,f,g
07f6:       db      0afh            ; '6' : a,c,d,e,f,g
07f7:       db      38h             ; '7' : a,b,c
07f8:       db      0bfh            ; '8' : a,b,c,d,e,f,g
07f9:       db      0beh            ; '9' : a,b,c,d,f,g
07fa:       db      0efh            ; 'a' : a,b,c,e,f,g
07fb:       db      0a7h            ; 'b' : c,d,e,f,g
07fc:       db      8dh             ; 'c' : a,d,e,f
07fd:       db      0b3h            ; 'd' : b,c,d,e,g
07fe:       db      8fh             ; 'e' : a,d,e,f,g
07ff:       db      87h             ; 'f' : a,e,f,g
""")

    raw_code = HEADER + "\n".join(sections) + FOOTER

    formatted_lines = []
    for line in raw_code.splitlines():
        m = re.match(r"^([0-9a-f]{4}):\s*(.*)$", line)
        if not m:
            formatted_lines.append(line)
            continue
        addr = m.group(1)
        rest = m.group(2)
        
        comment = ""
        if ";" in rest:
            code_part, comment_part = rest.split(";", 1)
            code_part = code_part.rstrip()
            comment = comment_part.strip()
        else:
            code_part = rest.rstrip()
            
        label = ""
        instruction = ""
        lm = re.match(r"^([a-z0-9_]+:)\s*(.*)$", code_part)
        if lm:
            label = lm.group(1)
            instruction = lm.group(2)
        else:
            instruction = code_part.strip()
            
        full_comment = f"; [{addr}] {comment}".rstrip()
        
        if label and not instruction:
            formatted_lines.append(f"{label:<16}{full_comment}")
        elif label and instruction:
            formatted_lines.append(f"{label:<12}{instruction:<24}{full_comment}")
        elif instruction:
            indent = " " * 12
            formatted_lines.append(f"{indent}{instruction:<24}{full_comment}")
        else:
            indent = " " * 36
            formatted_lines.append(f"{indent}{full_comment}")

    full_code = "\n".join(formatted_lines) + "\n"
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(full_code)
    print(f"[✓] Successfully wrote {OUTPUT_FILE} ({len(formatted_lines)} lines, {len(full_code)} bytes)")

if __name__ == "__main__":
    generate()
