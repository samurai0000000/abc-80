; ==============================================================================
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

; ==============================================================================
; SECTION 1: RESET AND INTERRUPT VECTORS (0000h - 006fh)
; ==============================================================================

; Power-On Reset Entry Point (RST 0)
; Stabilizes power supply and hardware lines before CPU initialization
            org     0000h
rst0:       ld      b,00h           ; [0000] [06 00] power-on stabilization delay loop
            djnz    $               ; [0002] [10 fe] loop 256 iterations until power rails settle
            jp      begin           ; [0004] [c3 70 00] jump to main system initialization

; User Restart Vectors (Jump to user RAM dispatch table at 1210h - 1260h)
            org     0008h
rst1:       jp      1210h           ; [0008] [c3 10 12] rst 1 (08h) user service routine
            org     0010h
rst2:       jp      1220h           ; [0010] [c3 20 12] rst 2 (10h) user service routine
            org     0018h
rst3:       jp      1230h           ; [0018] [c3 30 12] rst 3 (18h) user service routine
            org     0020h
rst4:       jp      1240h           ; [0020] [c3 40 12] rst 4 (20h) user service routine
            org     0028h
rst5:       jp      1250h           ; [0028] [c3 50 12] rst 5 (28h) user service routine
            org     0030h
rst6:       jp      1260h           ; [0030] [c3 60 12] rst 6 (30h) user service routine

; Breakpoint / Single-Step Trap / Mode 1 Interrupt Handler (RST 38H / 0038h)
; Triggered by hardware single-step trap or RST 38H (0xff) instruction
            org     0038h
break_handler:  ; [0038]
            ld      (13f8h),hl      ; [0038] [22 f8 13] preserve hl in user register dump (temp)
            pop     hl              ; [003b] [e1] pop saved pc from stack
            ld      (13eeh),hl      ; [003c] [22 ee 13] save pc to 13eeh
            ld      (13e2h),hl      ; [003f] [22 e2 13] save pc to user register dump pcsave
            ld      hl,(13f8h)      ; [0042] [2a f8 13] restore original hl
            ld      (13e0h),sp      ; [0045] [ed 73 e0 13] save sp to spsave
            ld      sp,13e0h        ; [0049] [31 e0 13] point sp to user register dump
            push    iy              ; [004c] [fd e5] save iy to 13deh
            push    ix              ; [004e] [dd e5] save ix to 13dch
            exx                     ; [0050] [d9] swap to alternate register set
            push    hl              ; [0051] [e5] save hl' to 13dah
            push    de              ; [0052] [d5] save de' to 13d8h
            push    bc              ; [0053] [c5] save bc' to 13d6h
            exx                     ; [0054] [d9] swap back to main registers
            ex      af,af'          ; [0055] [08] swap to alternate af
            push    af              ; [0056] [f5] save af' to 13d4h
            ex      af,af'          ; [0057] [08] swap back to main af
            push    hl              ; [0058] [e5] save hl to 13d2h
            push    de              ; [0059] [d5] save de to 13d0h
            push    bc              ; [005a] [c5] save bc to 13ceh
            push    af              ; [005b] [f5] save af to 13cch
            jp      rst0            ; [005c] [c3 00 00] restart monitor loop

; Non-Maskable Interrupt Handler (NMI / 0066h)
            org     0066h
nmi:        jp      1280h           ; [0066] [c3 80 12] jump to user nmi handler

; ==============================================================================
; SECTION 2: SYSTEM INITIALIZATION & MAIN EXECUTIVE LOOP (0070h - 00a8h)
; ==============================================================================

            org     0070h
begin:          ; [0070]
            ld      a,90h           ; [0070] [3e 90] configure 8255 ppi: port a=in, port b=out, port c=out
            out     (p8255),a       ; [0072] [d3 83] write 8255 control word
            ld      a,0ffh          ; [0074] [3e ff] blank 7-segment displays
            out     (digit),a       ; [0076] [d3 82] port c = 0ffh (all digit cathode transistors off)
            ld      sp,sysstk       ; [0078] [31 bf 17] initialize system stack pointer (17bfh)
            ld      a,(pwup)        ; [007b] [3a f5 17] read power-up magic flag (17f5h)
            cp      pwcode          ; [007e] [fe 80] test for warm reset flag (80h)
            call    nz,init         ; [0080] [c4 4c 05] if cold boot (not 80h), run memory/ram initialization
            call    rstmu           ; [0083] [cd 44 05] play reset chime melody
            ld      hl,1000h        ; [0086] [21 00 10] default display address = 1000h (start of user ram)
            ld      (adsave),hl     ; [0089] [22 ee 17] store into adsave (17eeh)
            xor     a               ; [008c] [af] clear accumulator
            ld      (test),a        ; [008d] [32 f6 17] clear test flag (17f6h)
            ld      ix,disp         ; [0090] [dd 21 bf 07] point ix to "abc-80" display font pattern (07bfh)
setst0:         ; [0094]
            xor     a               ; [0094] [af] clear a
            ld      (state),a       ; [0095] [32 f4 17] set system state = 0 (fix mode)
            nop                     ; [0098] [00]
            nop                     ; [0099] [00]
            nop                     ; [009a] [00]
main:       call    scd_k           ; [009b] [cd a6 04] scan keyboard and refresh display
            call    ms3k17          ; [009e] [cd 64 04] 3 khz key click tone (page 168: CALL MS3K1')
            call    chkey           ; [00a1] [cd a9 00] keyboard decode & branch dispatch
            jr      main            ; [00a4] [18 f5] loop indefinitely

; ==============================================================================
; SECTION 3: KEYBOARD INPUT DECODING & DISPATCH (00a9h - 00feh)
; ==============================================================================

            org     00a9h
chkey:          ; [00a9]
            cp      10h             ; [00a9] [fe 10] test if key code is hex (0..f) or func (10..16)
            jr      c,khex          ; [00ab] [38 27] if key < 10h, branch to hex key handler (00d4h)
            ld      hl,test         ; [00ad] [21 f6 17] point hl to test flag (17f6h)
            set     0,(hl)          ; [00b0] [cb c6] set bit 0 (function key active)
            sub     10h             ; [00b2] [d6 10] subtract 10h (function key index: 0..6)
            cp      04h             ; [00b4] [fe 04] compare with 4 ('+', '-', 'exec', 'data')
            ld      hl,subfun       ; [00b6] [21 25 07] load base address of subfun table (0725h)
            jp      c,branch        ; [00b9] [da 6a 03] if index < 4, dispatch via subfun table
            ld      ix,dispbf       ; [00bc] [dd 21 c6 17] ix points to display buffer (17c6h)
            sub     02h             ; [00c0] [d6 02] adjust index
            ld      hl,state        ; [00c2] [21 f4 17] point to state byte (17f4h)
            ld      (hl),a          ; [00c5] [77] update state with new state
            ld      hl,stmoni       ; [00c6] [21 f3 17] parameter counter (17f3h)
            ld      (hl),00h        ; [00c9] [36 00] clear parameter counter = 0
            ld      hl,func         ; [00cb] [21 2b 07] load base address of func table (072bh)
            sub     02h             ; [00ce] [d6 02] adjust index (0, 1, 2)
            jp      branch          ; [00d0] [c3 6a 03] dispatch via branch routine (036ah)

; Subfunction Jump Handlers (Pages 168 - 169)
            org     00d4h
khex:       ld      c,a             ; [00d4] [4f] save hex key in register c
            ld      hl,htab         ; [00d5] [21 30 07] point hl to hex jump table (0730h)
prebr:          ; [00d8]
            ld      a,(state)       ; [00d8] [3a f4 17] load current state (0..4) into a
            jp      branch          ; [00db] [c3 6a 03] branch per state
            org     00deh
kinc:       ld      hl,itab         ; [00de] [21 37 07] point hl to '+' key jump table (0737h)
            jr      prebr           ; [00e1] [18 f5] branch per state
            org     00e3h
kdec:       ld      hl,dtab         ; [00e3] [21 3e 07] point hl to '-' key jump table (073eh)
            jr      prebr           ; [00e6] [18 f0] branch per state
            org     00e8h
kexec:          ; [00e8]
            ld      hl,etab         ; [00e8] [21 45 07] point hl to 'exec' key jump table (0745h)
            jr      prebr           ; [00eb] [18 eb] branch per state
            org     00edh
kdata:          ; [00ed]
            call    testm           ; [00ed] [cd 37 05] verify display mode (err if not state 1 or 2)
            call    dform2          ; [00f0] [cd 9b 03] display address & data, state = 2
            ret                     ; [00f3] [c9]
            org     00f4h
kadrs:          ; [00f4]
            call    dform1          ; [00f4] [cd 92 03] display address format x.x.x.x.x x, state = 1
            ret                     ; [00f7] [c9]
            org     00f8h
ktapwr:         ; [00f8]
            call    stepdp          ; [00f8] [cd b9 03] display tape parameter format x.x.x.x.- n
            ret                     ; [00fb] [c9]

; ==============================================================================
; SECTION 4: HEX DIGIT KEY HANDLERS PER STATE (00ffh - 0136h)
; ==============================================================================

            org     00ffh
hfix:       jp      errdis          ; [00ff] [c3 75 03] state 0 (fix mode): hex key is invalid
            org     0102h
hda:        ld      hl,(adsave)     ; [0102] [2a ee 17] state 2 (data mode): load current address
            call    ramchk          ; [0105] [cd 9d 04] verify address is writable ram
            jp      nz,errdis       ; [0108] [c2 75 03] if rom/unmapped, error
            call    cl1byt          ; [010b] [cd 7c 03] clear data byte if test flag set
            ld      a,c             ; [010e] [79] get hex digit from c
            rld                     ; [010f] [ed 6f] rotate digit into low nibble of (hl)
            call    dform2          ; [0111] [cd 9b 03] display address and data, state = 2
            ret                     ; [0114] [c9]
            org     0116h
hda1:       ld      hl,adsave       ; [0116] [21 ee 17] state 1 (adrs mode): point hl to adsave
            call    cl2byt          ; [0119] [cd 89 03] clear address bytes if test flag set
            ld      a,c             ; [011c] [79] get entered hex digit
            rld                     ; [011d] [ed 6f] rotate into low nibble of (hl)
            inc     hl              ; [011f] [23] advance to high byte of adsave
            rld                     ; [0120] [ed 6f] rotate into high byte
            call    dform1          ; [0122] [cd 92 03] display address format, state = 1
            ret                     ; [0125] [c9]
            org     0127h
htapwr:         ; [0127]
            call    gtpalc          ; [0127] [cd d5 03] state 3/4 (tape mode): get parameter address
            call    cl2byt          ; [012a] [cd 89 03] clear parameter if test flag set
            ld      a,c             ; [012d] [79] get hex digit
            rld                     ; [012e] [ed 6f] rotate into low byte
            inc     hl              ; [0130] [23] advance to high byte
            rld                     ; [0131] [ed 6f] rotate into high byte
            call    stepdp          ; [0133] [cd b9 03] display tape parameter format
            ret                     ; [0136] [c9]

; ==============================================================================
; SECTION 5: '+' AND '-' FUNCTION KEY HANDLERS (013ah - 017ch)
; ==============================================================================

; '+' Key Handlers (ITAB dispatch)
            org     013ah
ifix:       jp      errdis          ; [013a] [c3 75 03] state 0 (fix mode): '+' is invalid
            org     013dh
adradd:         ; [013d]
            ld      hl,(adsave)     ; [013d] [2a ee 17] state 1/2: increment current memory address
            inc     hl              ; [0140] [23] hl = hl + 1
            ld      (adsave),hl     ; [0141] [22 ee 17] update adsave
            call    dform2          ; [0144] [cd 9b 03] display updated address and data
            ret                     ; [0147] [c9]
            org     0149h
tpfun:          ; [0149]
            ld      hl,stmoni       ; [0149] [21 f3 17] state 3/4: parameter counter (17f3h)
            inc     (hl)            ; [014c] [34] increment parameter counter
            call    gtpana          ; [014d] [cd e0 03] check if parameter index within range
            jr      nz,istep        ; [0150] [20 04] if valid, display parameter
            dec     (hl)            ; [0152] [35] out of range: restore counter
            jp      errdis          ; [0153] [c3 75 03] show error
            org     0156h
istep:          ; [0156]
            call    stepdp          ; [0156] [cd b9 03] display parameter format
            ret                     ; [0159] [c9]

; '-' Key Handlers (DTAB dispatch)
            org     015dh
defix:          ; [015d]
            jp      errdis          ; [015d] [c3 75 03] state 0 (fix mode): '-' is invalid
            org     0160h
adrdec:         ; [0160]
            ld      hl,(adsave)     ; [0160] [2a ee 17] state 1/2: decrement current memory address
            dec     hl              ; [0163] [2b] hl = hl - 1
            ld      (adsave),hl     ; [0164] [22 ee 17] update adsave
            call    dform2          ; [0167] [cd 9b 03] display updated address and data
            ret                     ; [016a] [c9]
            org     016ch
tprun2:         ; [016c]
            ld      hl,stmoni       ; [016c] [21 f3 17] state 3/4: parameter counter
            dec     (hl)            ; [016f] [35] decrement parameter counter
            call    gtpana          ; [0170] [cd e0 03] check if parameter index within range
            jr      nz,dstep        ; [0173] [20 04] if valid, display parameter
            inc     (hl)            ; [0175] [34] out of range: restore counter
            jp      errdis          ; [0176] [c3 75 03] show error
            org     0179h
dstep:          ; [0179]
            call    stepdp          ; [0179] [cd b9 03] display parameter format
            ret                     ; [017c] [c9]

; ==============================================================================
; SECTION 6: CASSETTE TAPE FILE MANAGEMENT & TAPE HEADERS (0180h - 0226h)
; ==============================================================================

; Execute Key Handler Dispatch Stubs (Page 173)
            org     0180h
efix:       jp      errdis          ; [0180] [c3 75 03] state 0 (banner mode): exec is invalid -> error
adrexc:     push    hl              ; [0183] [e5] state 1: jump to address displayed on 7-segment leds
            ld      hl,(adsave)     ; [0184] [2a ee 17] load currently displayed memory address
            ex      (sp),hl         ; [0187] [e3] push target address onto stack
            ret                     ; [0188] [c9] jump to target by return
            org     018ah
endfun:         ; [018a]
            ld      (adsave),de     ; [018a] [ed 53 ee 17] save final address to adsave
            call    dform2          ; [018e] [cd 9b 03] display address & data, transition state = 2
            ret                     ; [0191] [c9]

; Tape Save Routine (TO TAPE / EWT, Pages 173 - 174)
; Writes leader tone, header block, inter-block gap, payload data block, and trailer tone
            org     0193h
ewt:        call    sum             ; [0193] [cd 8f 04] calculate 8-bit checksum and verify range
            jr      c,error         ; [0196] [38 2b] if block invalid, branch to error
            ld      (17c5h),a       ; [0198] [32 c5 17] save checksum in tape buffer (dispbf-1)
            ld      hl,0fa0h        ; [019b] [21 a0 0f] 4000 cycles of 1 khz leader tone
            call    ms1k            ; [019e] [cd 70 04] generate 1 khz leader tone
            ld      hl,stepbf       ; [01a1] [21 bf 17] point to header block in stepbf
            ld      bc,0007h        ; [01a4] [01 07 00] 7 header bytes (filename, start, end, chksum)
            call    tapout          ; [01a7] [cd 30 03] write header to tape
            ld      hl,0fa0h        ; [01aa] [21 a0 0f] 4000 cycles of 2 khz inter-block tone
            call    ms2k            ; [01ad] [cd 75 04] generate 2 khz carrier
            call    getptr          ; [01b0] [cd bb 02] get data start (hl), end (de), length (bc)
            call    tapout          ; [01b3] [cd 30 03] write payload data block to tape
            ld      hl,0fa0h        ; [01b6] [21 a0 0f] 4000 cycles of 2 khz trailer tone
            call    ms2k            ; [01b9] [cd 75 04] generate 2 khz finish tone
            org     01bch
endtap:         ; [01bc]
            ld      de,(17c3h)      ; [01bc] [ed 5b c3 17] load end address
            jr      endfun          ; [01c0] [18 c8] display completion and finish

            org     01c3h
error:          ; [01c3]
            ld      ix,errtab       ; [01c3] [dd 21 d9 07] point ix to "-error" display pattern
            jp      setst0          ; [01c7] [c3 94 00] restore state = 0

; Tape Load Routine (FROM TAPE / ERT, Pages 174 - 176)
            org     01ceh
ert:            ; [01ce]
            ld      hl,(stepbf)     ; [01ce] [2a bf 17] load requested filename from stepbf (17bfh)
            ld      (temp),hl       ; [01d1] [22 fa 17] preserve target filename in temp (17fah)
lead:           ; [01d4]
            ld      a,0bfh          ; [01d4] [3e bf] display '-' character
            out     (segment),a     ; [01d6] [d3 81] blank displays during search
            ld      hl,03e8h        ; [01d8] [21 e8 03] wait for 1000 cycles (03e8h) of 1 khz tone
lead1:          ; [01db]
            call    period          ; [01db] [cd 14 03] measure carrier cycle
            jr      c,lead          ; [01de] [38 f4] if not 1 khz, restart search
            dec     hl              ; [01e0] [2b] decrement cycle counter
            ld      a,h             ; [01e1] [7c]
            or      l               ; [01e2] [b5]
            jr      nz,lead1        ; [01e3] [20 f6] wait until 1000 cycles pass
lead2:          ; [01e5]
            call    period          ; [01e5] [cd 14 03] measure carrier cycle
            jr      nc,lead2        ; [01e8] [30 fb] wait until carrier finishes
            ld      hl,stepbf       ; [01ea] [21 bf 17] load header into stepbf
            ld      bc,0007h        ; [01ed] [01 07 00] 7 header bytes
            call    tapein          ; [01f0] [cd cf 02] read 7 header bytes from tape
            jr      c,lead          ; [01f3] [38 df] if error, retry search
            ld      de,(stepbf)     ; [01f5] [ed 5b bf 17] read file name
            call    adrsdp          ; [01f9] [cd 04 05] format filename for display
            ld      b,96h           ; [01fc] [06 96] display filename for 150 cycles (~1.5 seconds)
filedp:         ; [01fe]
            call    scd_k1          ; [01fe] [cd cd 04] refresh display
            djnz    filedp          ; [0201] [10 fb]
            ld      hl,(temp)       ; [0203] [2a fa 17] compare requested filename
            or      a               ; [0206] [b7] clear carry
            sbc     hl,de           ; [0207] [ed 52] check if found requested file
            jr      nz,lead         ; [0209] [20 c9] if mismatch, search for next file
            ld      a,0fdh          ; [020b] [3e fd] found file: display indicator
            out     (segment),a     ; [020d] [d3 81]
            call    getptr          ; [020f] [cd bb 02] calculate target buffer address and length
            jr      c,error         ; [0212] [38 af] if invalid parameters, jump to error
            call    tapein          ; [0214] [cd cf 02] read data block into ram
            jr      c,error         ; [0217] [38 aa] if read failed, error
            call    sum             ; [0219] [cd 8f 04] calculate checksum of loaded data
            ld      hl,17c5h        ; [021c] [21 c5 17] point to stored checksum
            cp      (hl)            ; [021f] [be] compare checksum
            jr      nz,error        ; [0220] [20 a1] if mismatch, error
            jr      endtap          ; [0222] [18 98] success: display finish address

; ==============================================================================
; SECTION 7: 2716 / 2732 EPROM PROGRAMMER INTERFACE (0227h - 02ceh)
; ==============================================================================

            org     0227h
wr2716:         ; [0227]
            ld      ix,dispbf       ; [0227] [dd 21 c6 17] ix points to display buffer
            push    bc              ; [022b] [c5] preserve parameters
            push    hl              ; [022c] [e5]
            ex      de,hl           ; [022d] [eb] calculate length
            or      a               ; [022e] [b7] clear carry
            sbc     hl,bc           ; [022f] [ed 42] hl = end - start
            ld      c,l             ; [0231] [4d] length into bc
            ld      b,h             ; [0232] [44]
            ld      de,0f000h       ; [0233] [11 00 f0] test if length exceeds eprom capacity (4kb)
            add     hl,de           ; [0236] [19]
            jr      c,err_prg       ; [0237] [38 06] exceeded capacity -> error
            pop     hl              ; [0239] [e1] restore start address
            push    hl              ; [023a] [e5]
            add     hl,de           ; [023b] [19] check end address boundary
            jr      c,err_prg       ; [023c] [38 01]
            add     hl,bc           ; [023e] [09] check overall length
err_prg:        ; [023f]
            jp      c,error         ; [023f] [da c3 01] exceeded eprom address space
            inc     bc              ; [0242] [03] normalize byte count
            pop     de              ; [0243] [d1] eprom destination address
            pop     hl              ; [0244] [e1] abc-80 source ram address
            dec     a               ; [0245] [3d] check programming mode option (1=blank, 2=prog, 3=verify)
            jr      z,tblank        ; [0246] [28 05] option 1: run blank check on eprom
            dec     a               ; [0248] [3d]
            jr      z,prog          ; [0249] [28 14] option 2: execute eprom burn
            jr      verify          ; [024b] [18 21] option 3: verify eprom against ram

; EPROM Blank Check Loop (Ensure all bytes are 0xff)
            org     024dh
tblank:         ; [024d]
            push    de              ; [024d] [d5] preserve target address
            push    bc              ; [024e] [c5] preserve byte count
loop1:          ; [024f]
            call    datain          ; [024f] [cd 7e 02] read byte from eprom socket
            cp      0ffh            ; [0252] [fe ff] compare with erased byte (0xff)
            jr      z,testne        ; [0254] [28 01] if blank, continue to next address
            halt                    ; [0256] [76] not blank: halt cpu!
testne:         ; [0257]
            inc     de              ; [0257] [13] increment eprom address
            dec     bc              ; [0258] [0b] decrement byte counter
            ld      a,b             ; [0259] [78] check if bc == 0
            or      c               ; [025a] [b1]
            jr      nz,loop1        ; [025b] [20 f2] loop until all bytes verified blank
            pop     bc              ; [025d] [c1] restore parameters
            pop     de              ; [025e] [d1]
            org     025fh
prog:       push    hl              ; [025f] [e5] save ram source
            push    de              ; [0260] [d5] save eprom target
            push    bc              ; [0261] [c5] save byte count
loop2:          ; [0262]
            call    burn            ; [0262] [cd 90 02] apply 50ms programming pulse to current byte
            inc     de              ; [0265] [13] advance eprom address
            cpi                     ; [0266] [ed a1] increment hl, decrement bc, compare
            jp      pe,loop2        ; [0268] [ea 62 02] loop until block complete
            pop     bc              ; [026b] [c1] restore parameters for verification pass
            pop     de              ; [026c] [d1]
            pop     hl              ; [026d] [e1]

; Verify Programmed EPROM against RAM
            org     026eh
verify:         ; [026e]
            call    datain          ; [026e] [cd 7e 02] read byte from eprom socket
            cpi                     ; [0271] [ed a1] compare with ram source byte (hl)
            jr      z,vnext         ; [0273] [28 01] match -> advance
            halt                    ; [0275] [76] verify mismatch -> halt cpu!
vnext:          ; [0276]
            inc     de              ; [0276] [13] advance eprom address
            jp      pe,verify       ; [0277] [ea 6e 02] loop until all bytes verified
            rst     0               ; [027a] [c7] programming verified successful -> restart monitor

; Read Byte from EPROM Socket (Ports 40h-43h)
            org     027eh
datain:         ; [027e]
            ld      a,82h           ; [027e] [3e 82] configure secondary 8255: pa=out, pb=in, pc=out
            out     (q8255),a       ; [0280] [d3 43] port 43h
            ld      a,e             ; [0282] [7b] output low address byte (a0-a7)
            out     (addlow),a      ; [0283] [d3 40] port 40h
            ld      a,d             ; [0285] [7a] output high address byte (a8-a11)
            and     07h             ; [0286] [e6 07] mask 3 bits for 2716/2732
            set     7,a             ; [0288] [cb ff] assert /ce high
            out     (addhig),a      ; [028a] [d3 42] port 42h
            in      a,(dain)        ; [028c] [db 41] read data byte from port 41h (pb)
            ret                     ; [028e] [c9]

; Burn Single Byte into EPROM (50ms Vpp Programming Pulse)
            org     0290h
burn:           ; [0290]
            push    bc              ; [0290] [c5] preserve registers
            push    hl              ; [0291] [e5]
            ld      a,80h           ; [0292] [3e 80] configure secondary 8255: all ports output
            out     (q8255),a       ; [0294] [d3 43]
            ld      a,(hl)          ; [0296] [7e] fetch data byte from source ram
            out     (daot),a        ; [0297] [d3 41] output data byte to eprom pins
            ld      a,e             ; [0299] [7b] output low address (a0-a7)
            out     (addlow),a      ; [029a] [d3 40]
            ld      a,d             ; [029c] [7a] output high address (a8-a11)
            and     07h             ; [029d] [e6 07]
            out     (addhig),a      ; [029f] [d3 42]
            ld      c,a             ; [02a1] [4f] save address high bits
            or      60h             ; [02a2] [f6 60] assert programming pulses (pc5, pc6 high)
            out     (cont),a        ; [02a4] [d3 42]
            ld      a,(hl)          ; [02a6] [7e] display burned data on 7-segment leds
            call    dadp            ; [02a7] [cd 11 05]
            call    adrsdp          ; [02aa] [cd 04 05] display burning address
            ld      b,05h           ; [02ad] [06 05] 50ms pulse duration (5 * 10ms scan cycles)
de50ms:         ; [02af]
            call    scd_k1          ; [02af] [cd cd 04] refresh display during burn pulse
            djnz    de50ms          ; [02b2] [10 fb] loop 50ms
            ld      a,c             ; [02b4] [79] deassert programming pulse (restore pc5/pc6 low)
            out     (addhig),a      ; [02b5] [d3 42]
            pop     hl              ; [02b7] [e1] restore registers
            pop     bc              ; [02b8] [c1]
            ret                     ; [02b9] [c9]

; Calculate Tape Block Length & Setup Pointers (Page 180)
; Inputs:  17c1h = start address, 17c3h = end address
; Outputs: HL = start address, DE = end address, BC = byte length, Carry = 0 (valid) / 1 (err)
            org     02bbh
getptr:         ; [02bb]
            ld      hl,17c1h        ; [02bb] [21 c1 17] point hl to tape parameter start address
getp:           ; [02be]
            ld      e,(hl)          ; [02be] [5e] load start address low byte
            inc     hl              ; [02bf] [23]
            ld      d,(hl)          ; [02c0] [56] load start address high byte
            inc     hl              ; [02c1] [23]
            ld      c,(hl)          ; [02c2] [4e] load end address low byte
            inc     hl              ; [02c3] [23]
            ld      h,(hl)          ; [02c4] [66] load end address high byte
            ld      l,c             ; [02c5] [69] hl = end address
            or      a               ; [02c6] [b7] clear carry flag
            sbc     hl,de           ; [02c7] [ed 52] calculate difference: end - start
            ld      c,l             ; [02c9] [4d] store length into bc
            ld      b,h             ; [02ca] [44]
            inc     bc              ; [02cb] [03] bc = length + 1
            ex      de,hl           ; [02cc] [eb] restore hl = start address
            ret                     ; [02cd] [c9] return with carry flag indicating validity

; ==============================================================================
; SECTION 8: CASSETTE TAPE FSK AUDIO MODULATOR / DEMODULATOR (02cfh - 0368h)
; ==============================================================================

; Read Byte Block from Cassette Audio (Page 181)
; Inputs: HL = destination RAM address, BC = byte count
; Output: Carry flag: 0 = success, 1 = read/parity error
            org     02cfh
tapein:         ; [02cf]
            xor     a               ; [02cf] [af] clear carry flag (clean start)
            ex      af,af'          ; [02d0] [08] save status in alternate af
tloop:          ; [02d1]
            call    gtbyte          ; [02d1] [cd dd 02] read 1 byte from audio stream into e
            ld      (hl),e          ; [02d4] [73] store received byte into ram destination
            cpi                     ; [02d5] [ed a1] advance hl, decrement bc, test if bc==0
            jp      pe,tloop        ; [02d7] [ea d1 02] loop until all bytes read
            ex      af,af'          ; [02da] [08] retrieve error status
            ret                     ; [02db] [c9]

; Read Single Byte from Cassette Audio (Page 181)
; Output: E = received data byte, Carry flag: 0 = valid, 1 = error
            org     02ddh
gtbyte:         ; [02dd]
            call    getbit          ; [02dd] [cd ef 02] synchronize with start bit
            ld      d,08h           ; [02e0] [16 08] read 8 data bits
gloop:          ; [02e2]
            call    getbit          ; [02e2] [cd ef 02] read next bit into carry flag
            rr      e               ; [02e5] [cb 1b] rotate carry bit into register e
            dec     d               ; [02e7] [15] decrement bit counter
            jr      nz,gloop        ; [02e8] [20 f8] loop 8 bits
            call    getbit          ; [02ea] [cd ef 02] read stop bit
            ret                     ; [02ed] [c9]

; Read Single Bit from Audio Pulse Stream (Page 182)
; Output: Carry flag = demodulated bit value (0 or 1), Carry = 1 on timeout error
            org     02efh
getbit:         ; [02ef]
            exx                     ; [02ef] [d9] swap to alternate register set
            ld      hl,0000h        ; [02f0] [21 00 00] initialize cycle accumulator
count:          ; [02f3]
            call    period          ; [02f3] [cd 14 03] measure length of half-wave audio cycle
            inc     d               ; [02f6] [14] check timeout in d register
            dec     d               ; [02f7] [15]
            jr      nz,terr         ; [02f8] [20 12] timeout -> error
            jr      c,shortp        ; [02fa] [38 06] if short cycle (2 khz), jump to shortp
            dec     l               ; [02fc] [2d] long cycle (1 khz): decrement l counter by 2
            dec     l               ; [02fd] [2d]
            set     0,h             ; [02fe] [cb c4] mark that 1 khz half has been seen
            jr      count           ; [0300] [18 f1] loop for remaining cycle
shortp:         ; [0302]
            inc     l               ; [0302] [2c] short cycle (2 khz): increment l counter
            bit     0,h             ; [0303] [cb 44] check if full bit cycle complete
            jr      z,count         ; [0305] [28 ec] loop until bit period complete
            rl      l               ; [0307] [cb 15] shift bit sign into carry flag
            exx                     ; [0309] [d9] swap back to main registers
            ret                     ; [030a] [c9]
            org     030ch
terr:       ex      af,af'          ; [030c] [08] set carry error flag
            scf                     ; [030d] [37]
            ex      af,af'          ; [030e] [08]
            exx                     ; [030f] [d9] restore main registers
            ret                     ; [0310] [c9]

; Measure Audio Waveform Cycle Duration (Page 183)
; Inputs: Port A bit 7 = cassette audio input (kin)
; Outputs: DE = cycle timing duration, Carry flag: 0 = 1khz (long), 1 = 2khz (short)
            org     0314h
period:         ; [0314]
            ld      de,0000h        ; [0314] [11 00 00] reset cycle timer
chk0:       in      a,(kin)         ; [0317] [db 80] read audio input from 8255 port a bit 7
            inc     de              ; [0319] [13] increment timing counter
            rla                     ; [031a] [17] shift bit 7 into carry
            jr      c,chk0          ; [031b] [38 fa] wait while input is high
            ld      a,01000000b     ; [031d] [3e 40] route audio feedback to speaker (bit 6)
            out     (digit),a       ; [031f] [d3 82] play audio click through onboard speaker!
chk1:       in      a,(kin)         ; [0321] [db 80] read audio input
            inc     de              ; [0323] [13] increment timer
            rla                     ; [0324] [17]
            jr      nc,chk1         ; [0325] [30 fa] wait while input is low
            ld      a,0c0h          ; [0327] [3e c0] speaker feedback phase 2
            out     (digit),a       ; [0329] [d3 82]
            ld      a,e             ; [032b] [7b] compare measured half-period duration
            cp      midpd           ; [032c] [fe 2a] compare against threshold (2ah = 42)
            ret                     ; [032e] [c9] carry=1 if < 2ah (2khz short), carry=0 if >= 2ah (1khz long)

; Write Byte Block to Cassette Audio (Page 184)
; Inputs: HL = source RAM address, BC = byte length
            org     0330h
tapout:         ; [0330]
            ld      e,(hl)          ; [0330] [5e] fetch data byte from source buffer
            call    otbyte          ; [0331] [cd 3b 03] modulate and write byte to tape
            cpi                     ; [0334] [ed a1] advance hl, decrement bc
            jp      pe,tapout       ; [0336] [ea 30 03] loop until block complete
            ret                     ; [0339] [c9]

; Write Single Byte to Cassette Audio (Page 184)
            org     033bh
otbyte:         ; [033b]
            ld      d,08h           ; [033b] [16 08] 8 data bits per byte
            or      a               ; [033d] [b7] clear carry flag (start bit = 0)
            call    outbit          ; [033e] [cd 4f 03] write start bit (0)
oloop:          ; [0341]
            rr      e               ; [0341] [cb 1b] shift lowest bit into carry flag
            call    outbit          ; [0343] [cd 4f 03] modulate bit to tape
            dec     d               ; [0346] [15] decrement bit count
            jr      nz,oloop        ; [0347] [20 f8] loop 8 bits
            scf                     ; [0349] [37] set carry flag = 1 (stop bit = 1)
            call    outbit          ; [034a] [cd 4f 03] write stop bit (1)
            ret                     ; [034d] [c9]

; Write Single Bit to Cassette Audio (Pages 184 - 185)
; Bit 0: 12 cycles of 2 khz tone + 3 cycles of 1 khz tone
; Bit 1:  6 cycles of 2 khz tone + 6 cycles of 1 khz tone
            org     034fh
outbit:         ; [034f]
            exx                     ; [034f] [d9] swap to alternate registers
            ld      h,00h           ; [0350] [26 00]
            jr      c,out1          ; [0352] [38 09] if bit == 1, jump to out1
out0:       ld      l,0ch           ; [0354] [2e 0c] bit 0: 12 cycles of 2 khz carrier
            call    ms2k            ; [0356] [cd 75 04] generate 2 khz burst
            ld      l,03h           ; [0359] [2e 03] bit 0: 3 cycles of 1 khz carrier
            jr      bitend          ; [035b] [18 07]
out1:       ld      l,06h           ; [035d] [2e 06] bit 1: 6 cycles of 2 khz carrier
            call    ms2k            ; [035f] [cd 75 04] generate 2 khz burst
            ld      l,06h           ; [0362] [2e 06] bit 1: 6 cycles of 1 khz carrier
bitend:         ; [0364]
            call    ms1k            ; [0364] [cd 70 04] generate 1 khz burst
            exx                     ; [0367] [d9] restore main registers
            ret                     ; [0368] [c9]

; ==============================================================================
; SECTION 9: BRANCH DISPATCH, ERROR DISPLAY & FORMATTERS (036ah - 03f5h)
; ==============================================================================

; Indexed Subfunction Table Dispatch (Page 185)
; Inputs: HL = base address pointer table, A = state index (0..4)
            org     036ah
branch:         ; [036a]
            ld      e,(hl)          ; [036a] [5e] load low byte of base routine address
            inc     hl              ; [036b] [23]
            ld      d,(hl)          ; [036c] [56] load high byte of base routine address
            inc     hl              ; [036d] [23] point hl to offset table
            add     a,l             ; [036e] [85] add state index to pointer low byte
            ld      l,a             ; [036f] [6f]
            ld      l,(hl)          ; [0370] [6e] load 1-byte offset from table
            ld      h,00h           ; [0371] [26 00]
            add     hl,de           ; [0373] [19] target address = base address + offset
            jp      (hl)            ; [0374] [e9] jump to indexed subfunction handler!

; Display "-ERROR" on 7-Segment LEDs (Page 185)
            org     0375h
errdis:         ; [0375]
            ld      hl,test         ; [0375] [21 f6 17] point to test status byte (17f6h)
            set     7,(hl)          ; [0378] [cb fe] set bit 7 (scd_k displays -error automatically!)
            ret                     ; [037a] [c9]

; Clear 1 Byte in Memory if Test Flag Set (Page 186)
            org     037ch
cl1byt:         ; [037c]
            ld      a,(test)        ; [037c] [3a f6 17] check test flag (bit 0 = first key press)
            or      a               ; [037f] [b7]
            ret     z               ; [0380] [c8] if zero, do not clear
            ld      a,00h           ; [0381] [3e 00] clear accumulator
            ld      (hl),a          ; [0383] [77] zero target memory byte
            ld      (test),a        ; [0384] [32 f6 17] reset test flag to 0
            ret                     ; [0387] [c9]

; Clear 2 Bytes (Address Word) in Memory (Page 186)
            org     0389h
cl2byt:         ; [0389]
            call    cl1byt          ; [0389] [cd 7c 03] clear low byte
            ret     z               ; [038c] [c8] if not first key, done
            inc     hl              ; [038d] [23] advance to high byte
            ld      (hl),a          ; [038e] [77] zero high byte
            dec     hl              ; [038f] [2b] restore pointer
            ret                     ; [0390] [c9]

; Format Display Buffer: Address Mode x.x.x.x.x x (Page 186)
            org     0392h
dform1:         ; [0392]
            ld      a,01h           ; [0392] [3e 01] state = 1 (address entry mode)
            ld      b,04h           ; [0394] [06 04] set 4 decimal points on address digits
            ld      hl,17c8h        ; [0396] [21 c8 17] address digit buffer
            jr      sav12           ; [0399] [18 07]

; Format Display Buffer: Data Mode xx x x.x. (Page 186)
            org     039bh
dform2:         ; [039b]
            ld      a,02h           ; [039b] [3e 02] state = 2 (data entry mode)
            ld      b,02h           ; [039d] [06 02] set 2 decimal points on data digits
            ld      hl,dispbf       ; [039f] [21 c6 17] data digit buffer (17c6h)
sav12:          ; [03a2]
            ld      (state),a       ; [03a2] [32 f4 17] update state byte
            exx                     ; [03a5] [d9] preserve registers
            ld      de,(adsave)     ; [03a6] [ed 5b ee 17] load current address into de
            call    adrsdp          ; [03aa] [cd 04 05] convert address to 4 7-segment digits
            ld      a,(de)          ; [03ad] [1a] load data byte from memory (de)
            call    dadp            ; [03ae] [cd 11 05] convert data byte to 2 7-segment digits
            exx                     ; [03b1] [d9] restore registers
setpt:          ; [03b2]
            set     6,(hl)          ; [03b2] [cb f6] turn on decimal point segment
            inc     hl              ; [03b4] [23]
            djnz    setpt           ; [03b5] [10 fb] loop for b decimal points
            ret                     ; [03b7] [c9]

; Format Display Buffer: Tape Parameter Mode x.x.x.x.- n (Page 187)
            org     03b9h
stepdp:         ; [03b9]
            call    gtpalc          ; [03b9] [cd d5 03] get parameter address
            ld      e,(hl)          ; [03bc] [5e] load parameter word into de
            inc     hl              ; [03bd] [23]
            ld      d,(hl)          ; [03be] [56]
            call    adrsdp          ; [03bf] [cd 04 05] format parameter address
            ld      hl,17c8h        ; [03c2] [21 c8 17] set 4 decimal points
            ld      b,04h           ; [03c5] [06 04]
            call    setpt           ; [03c7] [cd b2 03]
            call    gtpana          ; [03ca] [cd e0 03] get parameter identifier
            ld      l,a             ; [03cd] [6f]
            ld      h,02h           ; [03ce] [26 02] '-' separator character
            ld      (dispbf),hl     ; [03d0] [22 c6 17] display parameter label
            ret                     ; [03d3] [c9]

; Get Tape Parameter Address in Memory (Page 188)
; Address = stepbf (17bfh) + stmoni * 2
            org     03d5h
gtpalc:         ; [03d5]
            ld      a,(stmoni)      ; [03d5] [3a f3 17] read parameter counter (0..2)
            add     a,a             ; [03d8] [87] multiply by 2 (word offset)
            ld      hl,stepbf       ; [03d9] [21 bf 17] base address = 17bfh
            add     a,l             ; [03dc] [85]
            ld      l,a             ; [03dd] [6f] hl = stepbf + stmoni * 2
            ret                     ; [03de] [c9]

; Get Tape Parameter Label (Page 188)
            org     03e0h
gtpana:         ; [03e0]
            ld      a,(state)       ; [03e0] [3a f4 17] read state (3=to tape, 4=from tape)
            sub     01h             ; [03e3] [d6 01]
            add     a,a             ; [03e5] [87] multiply by 4
            add     a,a             ; [03e6] [87]
            ld      de,blank        ; [03e7] [11 e0 07] point to parameter label table (07e0h)
            add     a,e             ; [03ea] [83]
            ld      e,a             ; [03eb] [5f]
            ld      a,(stmoni)      ; [03ec] [3a f3 17] add parameter counter
            add     a,e             ; [03ef] [83]
            ld      e,a             ; [03f0] [5f]
            ld      a,(de)          ; [03f1] [1a] load label character
            or      a               ; [03f2] [b7] test if zero (end of parameters)
            ret                     ; [03f3] [c9]

; ==============================================================================
; SECTION 10: BANNER DISPLAY & MUSIC SYNTHESIZER (03f6h - 0503h)
; ==============================================================================

; Display Power-On Scrolling Banner ("HELLO THIS IS ABC-80")
            org     03f6h
inidp:          ; [03f6]
            ld      ix,initab+1     ; [03f6] [dd 21 d3 07] point ix to banner start + 1
            ld      c,15h           ; [03fa] [0e 15] 21 scroll steps
inidp1:         ; [03fc]
            ld      b,20h           ; [03fc] [06 20] display multiplex delay counter (32 cycles)
inidp2:         ; [03fe]
            call    scd_k1          ; [03fe] [cd cd 04] scan display
            djnz    inidp2          ; [0401] [10 fb] loop delay
            dec     ix              ; [0403] [dd 2b] scroll banner left by 1 digit
            call    ms3k17          ; [0405] [cd 64 04] sound click/beep while scrolling
            dec     c               ; [0408] [0d] decrement scroll counter
            jr      nz,inidp1       ; [0409] [20 f1] loop until banner fully scrolled across
            ret                     ; [040b] [c9] done

; Play Opening Melody (Pages 189 - 190)
            org     040dh
monsou:         ; [040d]
            ld      iy,song         ; [040d] [fd 21 58 05] point to opening melody score (0558h)
music:          ; [0411]
            push    iy              ; [0411] [fd e5]
            pop     ix              ; [0413] [dd e1] ix points to current note pair (pitch, duration)
            ld      a,(ix+00h)      ; [0415] [dd 7e 00] load pitch index / control byte
            add     a,a             ; [0418] [87] word offset (2 bytes per note)
            jr      c,stop          ; [0419] [38 30] bit 7 set (80h) -> stop / melody complete
            jp      m,music         ; [041b] [fa 11 04] bit 6 set (repeat marker) -> loop from start
            ld      c,00h           ; [041e] [0e 00] initialize c = 00h (silent rest)
            bit     6,a             ; [0420] [cb 77] test rest bit (bit 5 in original note, shifted to bit 6 by add a,a)
            jr      nz,play         ; [0422] [20 02] if rest (20h), leave c = 00h (silent)
            set     7,c             ; [0424] [cb f9] not rest -> set bit 7 of c (toggle speaker pin)
play:           ; [0426]
            and     3fh             ; [0426] [e6 3f] mask pitch table index
            ld      hl,frqtab       ; [0428] [21 4c 07] point to frequency table (074ch)
            add     a,l             ; [042b] [85]
            ld      l,a             ; [042c] [6f]
            ld      e,(hl)          ; [042d] [5e] e = half-period timer constant
            inc     hl              ; [042e] [23]
            ld      d,(hl)          ; [042f] [56] d = half-period cycle count per duration unit
            inc     ix              ; [0430] [dd 23] advance ix to duration byte
            ld      h,(ix+00h)      ; [0432] [dd 66 00] h = note duration
            ld      a,0ffh          ; [0435] [3e ff] initial speaker output state
tone:           ; [0437]
            ld      l,d             ; [0437] [6a] l = half-period cycle counter
tonout:         ; [0438]
            out     (digit),a       ; [0438] [d3 82] output to speaker (port c bit 7)
            ld      b,e             ; [043a] [43] b = half-period delay constant
delay:          ; [043b]
            nop                     ; [043b] [00]
            nop                     ; [043c] [00]
            nop                     ; [043d] [00]
            djnz    delay           ; [043e] [10 fb] half-period delay
            xor     c               ; [0440] [a9] toggle bit 7 if c has bit 7 set; stay constant if rest
            dec     l               ; [0441] [2d] decrement half-period cycle counter
            jr      nz,tonout       ; [0442] [20 f4]
            dec     h               ; [0444] [25] decrement duration counter
            jr      nz,tone         ; [0445] [20 f0]
            inc     ix              ; [0447] [dd 23] advance ix to next note pair
            jr      0415h           ; [0449] [18 ca] fetch next note
stop:           ; [044b]
            ret                     ; [044b] [c9]

; Tone Generator Auxiliary Wrappers (Pages 190 - 191)
            org     0450h
ms1k17:         ; [0450]
            exx                     ; [0450] [d9] save alternate registers
            ld      hl,0050h        ; [0451] [21 50 00] duration cycle count
            call    ms1k            ; [0454] [cd 70 04] generate 1 khz tone
            exx                     ; [0457] [d9] restore alternate registers
            ret                     ; [0458] [c9]

            org     045ah
ms2k17:         ; [045a]
            exx                     ; [045a] [d9]
            ld      hl,00a0h        ; [045b] [21 a0 00] duration cycle count
            call    ms2k            ; [045e] [cd 75 04] generate 2 khz tone
            exx                     ; [0461] [d9]
            ret                     ; [0462] [c9]

            org     0464h
ms3k17:         ; [0464]
            ex      af,af'          ; [0464] [08] preserve flags
            exx                     ; [0465] [d9] preserve registers
            ld      hl,00e0h        ; [0466] [21 e0 00] duration cycle count
            call    ms3k            ; [0469] [cd 7a 04] generate 3 khz tone
            exx                     ; [046c] [d9] restore registers
            ex      af,af'          ; [046d] [08] restore flags
            ret                     ; [046e] [c9]

; Tone Generators: ms1k, ms2k, ms3k (Pages 191 - 192)
            org     0470h
ms1k:           ; [0470]
            ld      c,41h           ; [0470] [0e 41] 1 khz half-period constant
            jr      sound           ; [0472] [18 08]

            org     0475h
ms2k:           ; [0475]
            ld      c,1fh           ; [0475] [0e 1f] 2 khz half-period constant
            jr      sound           ; [0477] [18 03]

            org     047ah
ms3k:           ; [047a]
            ld      c,0ch           ; [047a] [0e 0c] 3 khz half-period constant
sound:          ; [047c]
            add     hl,hl           ; [047c] [29] convert full periods to half periods
            ld      de,0001h        ; [047d] [11 01 00]
            ld      a,0ffh          ; [0480] [3e ff]
sqwave:         ; [0482]
            out     (digit),a       ; [0482] [d3 82] toggle speaker pin (port c bit 7)
            ld      b,c             ; [0484] [41] load half-period timer
            djnz    $               ; [0485] [10 fe] delay
            xor     80h             ; [0487] [ee 80] toggle bit 7
            sbc     hl,de           ; [0489] [ed 52] decrement period counter
            jr      nz,sqwave       ; [048b] [20 f5]
            ret                     ; [048d] [c9]

; Calculate 8-Bit Checksum over Memory Block (Page 192)
; Inputs:  HL = start address, BC = length
; Outputs: A = 8-bit cumulative checksum
            org     048fh
sum:            ; [048f]
            call    getptr          ; [048f] [cd bb 02] get start/end pointers from step buffer
            ret     c               ; [0492] [d8] error if invalid parameters
            xor     a               ; [0493] [af] clear checksum accumulator
sumcal:         ; [0494]
            add     a,(hl)          ; [0494] [86] add memory byte
            cpi                     ; [0495] [ed a1] advance hl, decrement bc, compare
            jp      pe,sumcal       ; [0497] [ea 94 04] loop while bc > 0
            or      a               ; [049a] [b7] clear carry flag
            ret                     ; [049b] [c9]

; Dynamic RAM Detection Routine (Pages 192 - 193)
; Returns: Z = RAM writeable, NZ = ROM / unmapped
            org     049dh
ramchk:         ; [049d]
            ld      a,(hl)          ; [049d] [7e] read original byte
            cpl                     ; [049e] [2f] invert bits
            ld      (hl),a          ; [049f] [77] write inverted byte
            ld      a,(hl)          ; [04a0] [7e] read back
            cpl                     ; [04a1] [2f] invert back
            ld      (hl),a          ; [04a2] [77] restore original byte
            cp      (hl)            ; [04a3] [be] verify memory accepted write
            ret                     ; [04a4] [c9]

; Display Multiplexing & Keyboard Scanner (Pages 193 - 194)
; Continuously multiplexes 6-digit display until valid key pressed
            org     04a6h
scd_k:          ; [04a6]
            push    ix              ; [04a6] [dd e5] save display buffer pointer
            ld      hl,test         ; [04a8] [21 f6 17] check error flag
            bit     7,(hl)          ; [04ab] [cb 7e] test bit 7 (input error)
            jr      z,scpre         ; [04ad] [28 04] no error -> normal display
            ld      ix,errtab       ; [04af] [dd 21 d9 07] point ix to error message ("-ERROR")
scpre:          ; [04b3]
            ld      b,04h           ; [04b3] [06 04] wait 4 passes (40ms debounce) for key release
scnx:           ; [04b5]
            call    scd_k1          ; [04b5] [cd cd 04] scan display & keyboard
            jr      nc,scpre        ; [04b8] [30 f9] if key still pressed, restart debounce
            djnz    scnx            ; [04ba] [10 f9] debounce delay
            res     7,(hl)          ; [04bc] [cb be] clear error flag
            pop     ix              ; [04be] [dd e1] restore original display buffer pointer
scloop:         ; [04c0]
            call    scd_k1          ; [04c0] [cd cd 04] scan display & keyboard
            jr      c,scloop        ; [04c3] [38 fb] wait until key is pressed (c = 0)
keymap:         ; [04c5]
            ld      hl,keytab       ; [04c5] [21 8c 07] point hl to key translation table
            add     a,l             ; [04c8] [85] index by matrix position code
            ld      l,a             ; [04c9] [6f]
            ld      a,(hl)          ; [04ca] [7e] load internal monitor key code
            ret                     ; [04cb] [c9]

; Single Display Refresh & Keyboard Matrix Scan Pass (Pages 194 - 196)
; Inputs:  IX = display buffer pointer (dispbf at 17c6h)
; Outputs: Carry = 1 (no key pressed), Carry = 0 (key pressed, A = position code)
            org     04cdh
scd_k1:         ; [04cd]
            scf                     ; [04cd] [37] default: carry = 1 (no key pressed)
            ex      af,af'          ; [04ce] [08] save default carry flag
            exx                     ; [04cf] [d9] save registers
            ld      c,00h           ; [04d0] [0e 00] initialize position code counter
            ld      e,0feh          ; [04d2] [1e fe] digit 0 select bitmask (active low, port c bit 0)
kcol:           ; [04d4]
            ld      a,(ix+00h)      ; [04d4] [dd 7e 00] load 7-segment bitmask for current digit
            cpl                     ; [04d7] [2f] invert for active-low cathode driver
            out     (segment),a     ; [04d8] [d3 81] output to port b (segment bus)
            ld      a,e             ; [04da] [7b] load digit select bitmask
            out     (digit),a       ; [04db] [d3 82] output to port c (digit select)
            ld      b,09h           ; [04dd] [06 09] digit dwell delay loop (9 iterations)
            djnz    $               ; [04df] [10 fe]
            ld      b,06h           ; [04e1] [06 06] 6 key rows to scan
            in      a,(keypad)      ; [04e3] [db 80] read row inputs from port a
            ld      d,a             ; [04e5] [57]
krow:           ; [04e6]
            rr      d               ; [04e6] [cb 1a] shift row bit into carry
            jr      c,nokey         ; [04e8] [38 02] 1 = not pressed -> next row
            ld      a,c             ; [04ea] [79] 0 = pressed -> captured position code
            ex      af,af'          ; [04eb] [08] store position code and carry = 0 in af'
nokey:          ; [04ec]
            inc     c               ; [04ec] [0c] advance position code
            djnz    krow            ; [04ed] [10 f7] test all 6 rows in column
            inc     ix              ; [04ef] [dd 23] advance to next display digit
            rlc     e               ; [04f1] [cb 03] rotate digit select bit to next column
            ld      a,0ffh          ; [04f3] [3e ff] blank display between digits (prevents ghosting)
            out     (digit),a       ; [04f5] [d3 82]
            bit     6,e             ; [04f7] [cb 73] test if all 6 digits completed (bit 6 active)
            jr      nz,kcol         ; [04f9] [20 d9] loop until 6 digits scanned
            ld      de,-6           ; [04fb] [11 fa ff] restore ix back to start of display buffer
            add     ix,de           ; [04fe] [dd 19]
            exx                     ; [0500] [d9] restore alternate registers
            ex      af,af'          ; [0501] [08] restore carry flag and captured key code in a
            ret                     ; [0502] [c9]

; ==============================================================================
; SECTION 11: DISPLAY FONT ENCODERS & BOOTSTRAP INITIALIZATION (0504h - 0557h)
; ==============================================================================

; Convert 16-Bit Address in DE to 4 7-Segment Display Digits (Pages 196 - 197)
; Writes 4 segment bytes to dispbf+2 .. dispbf+5 (17c8h - 17cbh)
            org     0504h
adrsdp:         ; [0504]
            ld      hl,17c8h        ; [0504] [21 c8 17] point to address display buffer (dispbf+2)
            ld      a,e             ; [0507] [7b] convert low byte (e)
            call    tobyseg         ; [0508] [cd 25 05] convert byte to 2 display digits
            ld      a,d             ; [050b] [7a] convert high byte (d)
            call    tobyseg         ; [050c] [cd 25 05] convert byte to 2 display digits
            ret                     ; [050f] [c9]

; Convert 8-Bit Data in A to 2 7-Segment Display Digits (Page 196)
; Writes 2 segment bytes to dispbf .. dispbf+1 (17c6h - 17c7h)
            org     0511h
dadp:           ; [0511]
            ld      hl,dispbf       ; [0511] [21 c6 17] point to data display buffer (17c6h)
            call    tobyseg         ; [0514] [cd 25 05] convert byte in a to 2 display digits
            ret                     ; [0517] [c9]

; Convert Low Nibble in A to 7-Segment LED Bitmask (Page 197)
; Inputs:  A = nibble (0..F)
; Outputs: A = 7-segment bitmask from segtab
            org     0519h
onbyseg:        ; [0519]
            push    hl              ; [0519] [e5] preserve hl
            ld      hl,segtab       ; [051a] [21 f0 07] point hl to hex font table (07f0h)
            and     0fh             ; [051d] [e6 0f] isolate lower 4 bits (hex digit 0..f)
            add     a,l             ; [051f] [85] index into font table
            ld      l,a             ; [0520] [6f]
            ld      a,(hl)          ; [0521] [7e] load 7-segment bitmask
            pop     hl              ; [0522] [e1] restore hl
            ret                     ; [0523] [c9]

; Convert Byte in A into 2 7-Segment Font Digits (Page 197)
; Low nibble -> (HL), High nibble -> (HL+1), advances HL by 2
            org     0525h
tobyseg:        ; [0525]
            push    af              ; [0525] [f5] preserve byte
            call    onbyseg         ; [0526] [cd 19 05] convert low nibble
            ld      (hl),a          ; [0529] [77] store in (hl)
            inc     hl              ; [052a] [23] advance pointer
            pop     af              ; [052b] [f1] restore byte
            rrca                    ; [052c] [0f] rotate high nibble into low 4 bits
            rrca                    ; [052d] [0f]
            rrca                    ; [052e] [0f]
            rrca                    ; [052f] [0f]
            call    onbyseg         ; [0530] [cd 19 05] convert high nibble
            ld      (hl),a          ; [0533] [77] store in (hl+1)
            inc     hl              ; [0534] [23] advance pointer (hl now advanced by 2)
            ret                     ; [0535] [c9]

; Verify Current System State for 'DATA' Key (Page 198)
; Only valid in State 1 (ADRS mode) or State 2 (DATA mode)
            org     0537h
testm:          ; [0537]
            ld      a,(state)       ; [0537] [3a f4 17] check current state
            cp      01h             ; [053a] [fe 01] test state == 1 (adrs)
            ret     z               ; [053c] [c8] state 1 is valid -> return
            cp      02h             ; [053d] [fe 02] test state == 2 (data)
            ret     z               ; [053f] [c8] state 2 is valid -> return
            pop     hl              ; [0540] [e1] invalid state: discard caller return address
            jp      errdis          ; [0541] [c3 75 03] jump directly to error display!

; Play System Reset Melody (Page 198)
            org     0544h
rstmu:          ; [0544]
            ld      iy,rmusic       ; [0544] [fd 21 f2 05] load pointer to 11-byte reset chime (05f2h)
            call    music           ; [0548] [cd 11 04] play reset melody
            ret                     ; [054b] [c9]

; Cold Boot Hardware & RAM Initialization (Pages 198 - 199)
            org     054ch
init:           ; [054c]
            call    inidp           ; [054c] [cd f6 03] scroll "hello this is abc-80" banner
            call    monsou          ; [054f] [cd 0d 04] play opening song
            ld      a,pwcode        ; [0552] [3e 80] load magic signature (80h)
            ld      (pwup),a        ; [0554] [32 f5 17] set power-on flag in 17f5h
            ret                     ; [0557] [c9]

; ==============================================================================
; SECTION 12: MUSIC SCORES & MELODY DATA TABLES (0558h - 05fch)
; ==============================================================================

; Opening Cold-Start Melody Score ("Song", Pages 198 - 200)
; 154-byte note table: Pairs of (Pitch Index, Duration Factor)
            org     0558h
song:           ; [0558]
            db      05h, 08h        ; [0558] note 1
            db      05h, 08h        ; [055a] note 2
            db      05h, 10h        ; [055c] note 3
            db      05h, 08h        ; [055e] note 4
            db      05h, 08h        ; [0560] note 5
            db      05h, 10h        ; [0562] note 6
            db      05h, 08h        ; [0564] note 7
            db      08h, 08h        ; [0566] note 8
            db      01h, 0ch        ; [0568] note 9
            db      03h, 04h        ; [056a] note 10
            db      05h, 20h        ; [056c] note 11
            db      06h, 08h        ; [056e] note 12
            db      06h, 08h        ; [0570] note 13
            db      06h, 0ch        ; [0572] note 14
            db      06h, 04h        ; [0574] note 15
            db      06h, 08h        ; [0576] note 16
            db      05h, 08h        ; [0578] note 17
            db      05h, 08h        ; [057a] note 18
            db      05h, 04h        ; [057c] note 19
            db      05h, 04h        ; [057e] note 20
            db      05h, 08h        ; [0580] note 21
            db      03h, 08h        ; [0582] note 22
            db      03h, 08h        ; [0584] note 23
            db      05h, 08h        ; [0586] note 24
            db      03h, 10h        ; [0588] note 25
            db      08h, 10h        ; [058a] note 26
            db      05h, 08h        ; [058c] note 27
            db      05h, 08h        ; [058e] note 28
            db      05h, 10h        ; [0590] note 29
            db      05h, 08h        ; [0592] note 30
            db      05h, 08h        ; [0594] note 31
            db      05h, 10h        ; [0596] note 32
            db      05h, 08h        ; [0598] note 33
            db      08h, 08h        ; [059a] note 34
            db      01h, 0ch        ; [059c] note 35
            db      03h, 04h        ; [059e] note 36
            db      05h, 20h        ; [05a0] note 37
            db      06h, 08h        ; [05a2] note 38
            db      06h, 08h        ; [05a4] note 39
            db      06h, 0ch        ; [05a6] note 40
            db      06h, 04h        ; [05a8] note 41
            db      06h, 08h        ; [05aa] note 42
            db      05h, 08h        ; [05ac] note 43
            db      05h, 08h        ; [05ae] note 44
            db      05h, 04h        ; [05b0] note 45
            db      05h, 04h        ; [05b2] note 46
            db      08h, 08h        ; [05b4] note 47
            db      08h, 08h        ; [05b6] note 48
            db      06h, 08h        ; [05b8] note 49
            db      03h, 08h        ; [05ba] note 50
            db      01h, 20h        ; [05bc] note 51
            db      08h, 08h        ; [05be] note 52
            db      08h, 08h        ; [05c0] note 53
            db      08h, 10h        ; [05c2] note 54
            db      06h, 08h        ; [05c4] note 55
            db      05h, 08h        ; [05c6] note 56
            db      03h, 10h        ; [05c8] note 57
            db      08h, 08h        ; [05ca] note 58
            db      06h, 08h        ; [05cc] note 59
            db      05h, 08h        ; [05ce] note 60
            db      03h, 08h        ; [05d0] note 61
            db      02h, 20h        ; [05d2] note 62
            db      08h, 08h        ; [05d4] note 63
            db      08h, 08h        ; [05d6] note 64
            db      08h, 10h        ; [05d8] note 65
            db      06h, 08h        ; [05da] note 66
            db      05h, 08h        ; [05dc] note 67
            db      03h, 10h        ; [05de] note 68
            db      05h, 08h        ; [05e0] note 69
            db      03h, 08h        ; [05e2] note 70
            db      02h, 08h        ; [05e4] note 71
            db      05h, 08h        ; [05e6] note 72
            db      01h, 20h        ; [05e8] note 73
            db      00h, 00h        ; [05ea] note 74
            db      00h, 00h        ; [05ec] note 75
            db      80h, 0ffh       ; [05ee] end of song score marker (80h)

; Reset Chime Melody Score ("Rmusic", Page 200)
; 11-byte note table played during warm reset
            org     05f2h
rmusic:         ; [05f2]
            db      01h, 08h        ; [05f2] chime note 1
            db      03h, 08h        ; [05f4] chime note 2
            db      05h, 08h        ; [05f6] chime note 3
            db      08h, 0ch        ; [05f8] chime note 4
            db      06h, 04h        ; [05fa] chime note 5
            db      80h             ; [05fc] end of reset chime marker (80h)

; ==============================================================================
; SECTION 13: SUBFUNCTION JUMP TABLES & FONT BITMASKS (0725h - 07e7h)
; ==============================================================================

; Function Key Base & Offset Dispatch Tables (Pages 201 - 202)
; 16-bit base address followed by 1-byte delta offsets per state
            org     0725h
subfun:         ; [0725]
            dw      kinc            ; [0725] [de 00] '+' key base address (00deh)
            db      00h             ; [0727] '+' key offset (00de + 0 = 00deh: kinc)
            db      05h             ; [0728] '-' key offset (00de + 5 = 00e3h: kdec)
            db      0ah             ; [0729] 'exec' key offset (00de + 0ah = 00e8h: kexec)
            db      0fh             ; [072a] 'data' key offset (00de + 0fh = 00edh: kdata)

            org     072bh
func:           ; [072b]
            dw      kadrs           ; [072b] [f4 00] 'adrs' key base address (00f4h)
            db      00h             ; [072d] 'adrs' key offset (00f4 + 0 = 00f4h: kadrs)
            db      04h             ; [072e] 'to tape' key offset (00f4 + 4 = 00f8h: ktapwr)
            db      04h             ; [072f] 'from tape' key offset (00f4 + 4 = 00f8h: ktapwr)

            org     0730h
htab:           ; [0730]
            dw      hfix            ; [0730] [ff 00] hex key base address (00ffh)
            db      00h             ; [0732] state 0 (fix mode)  -> hfix   (00ff + 00h = 00ffh)
            db      17h             ; [0733] state 1 (adrs mode) -> hda1   (00ff + 17h = 0116h)
            db      03h             ; [0734] state 2 (data mode) -> hda    (00ff + 03h = 0102h)
            db      28h             ; [0735] state 3 (to tape)   -> htapwr (00ff + 28h = 0127h)
            db      28h             ; [0736] state 4 (from tape) -> htapwr (00ff + 28h = 0127h)

            org     0737h
itab:           ; [0737]
            dw      ifix            ; [0737] [3a 01] '+' key base address (013ah)
            db      00h             ; [0739] state 0 (fix mode)  -> ifix   (013a + 00h = 013ah)
            db      03h             ; [073a] state 1 (adrs mode) -> adradd (013a + 03h = 013dh)
            db      03h             ; [073b] state 2 (data mode) -> adradd (013a + 03h = 013dh)
            db      0fh             ; [073c] state 3 (to tape)   -> tpfun  (013a + 0fh = 0149h)
            db      0fh             ; [073d] state 4 (from tape) -> tpfun  (013a + 0fh = 0149h)

            org     073eh
dtab:           ; [073e]
            dw      defix           ; [073e] [5d 01] '-' key base address (015dh)
            db      00h             ; [0740] state 0 (fix mode)  -> defix  (015d + 00h = 015dh)
            db      03h             ; [0741] state 1 (adrs mode) -> adrdec (015d + 03h = 0160h)
            db      03h             ; [0742] state 2 (data mode) -> adrdec (015d + 03h = 0160h)
            db      0fh             ; [0743] state 3 (to tape)   -> tprun2 (015d + 0fh = 016ch)
            db      0fh             ; [0744] state 4 (from tape) -> tprun2 (015d + 0fh = 016ch)

            org     0745h
etab:           ; [0745]
            dw      efix            ; [0745] [80 01] 'exec' key base address (0180h)
            db      00h             ; [0747] state 0 (fix mode)  -> efix   (0180 + 00h = 0180h)
            db      03h             ; [0748] state 1 (adrs mode) -> adrexc (0180 + 03h = 0183h)
            db      03h             ; [0749] state 2 (data mode) -> adrexc (0180 + 03h = 0183h)
            db      13h             ; [074a] state 3 (to tape)   -> ewt    (0180 + 13h = 0193h)
            db      4eh             ; [074b] state 4 (from tape) -> ert    (0180 + 4eh = 01ceh)

; Frequency Lookup Table (Pages 202 - 203)
; Half-period timer constants for musical notes
            org     074ch
frqtab:         ; [074c]
            dw      18e1h           ; [074c] note 1
            dw      1ad4h           ; [074e] note 2
            dw      1b08h           ; [0750] note 3
            dw      1dbdh           ; [0752] note 4
            dw      1eb2h           ; [0754] note 5
            dw      20a8h           ; [0756] note 6
            dw      229fh           ; [0758] note 7
            dw      2496h           ; [075a] note 8
            dw      268dh           ; [075c] note 9
            dw      2985h           ; [075e] note 10
            dw      2b7eh           ; [0760] note 11
            dw      2e77h           ; [0762] note 12
            dw      3170h           ; [0764] note 13
            dw      336ah           ; [0766] note 14
            dw      3764h           ; [0768] note 15
            dw      3a5eh           ; [076a] note 16
            dw      3d59h           ; [076c] note 17
            dw      4154h           ; [076e] note 18
            dw      454eh           ; [0770] note 19
            dw      494ah           ; [0772] note 20
            dw      4d46h           ; [0774] note 21
            dw      5242h           ; [0776] note 22
            dw      573eh           ; [0778] note 23
            dw      5c3bh           ; [077a] note 24
            dw      6237h           ; [077c] note 25
            dw      6734h           ; [077e] note 26
            dw      6e31h           ; [0780] note 27
            dw      742eh           ; [0782] note 28
            dw      7b2ch           ; [0784] note 29
            dw      8229h           ; [0786] note 30
            dw      8a27h           ; [0788] note 31
            dw      9225h           ; [078a] note 32

; Key Scancode Translation Table (Page 203)
; Maps 4x6 hardware key matrix switch positions to internal key codes (00h - 16h)
            org     078ch
keytab:         ; [078c]
            db      14h             ; [078c] 'adrs'
            db      13h             ; [078d] 'data'
            db      11h             ; [078e] 'dec' ('-')
            db      10h             ; [078f] 'inc' ('+')
            db      0ffh            ; [0790] unused
            db      0ffh            ; [0791] unused
            db      0fh             ; [0792] 'f'
            db      0bh             ; [0793] 'b'
            db      07h             ; [0794] '7'
            db      03h             ; [0795] '3'
            db      0ffh            ; [0796] unused
            db      0ffh            ; [0797] unused
            db      0eh             ; [0798] 'e'
            db      0ah             ; [0799] 'a'
            db      06h             ; [079a] '6'
            db      02h             ; [079b] '2'
            db      0ffh            ; [079c] unused
            db      0ffh            ; [079d] unused
            db      0dh             ; [079e] 'd'
            db      09h             ; [079f] '9'
            db      05h             ; [07a0] '5'
            db      01h             ; [07a1] '1'
            db      0ffh            ; [07a2] unused
            db      0ffh            ; [07a3] unused
            db      0ch             ; [07a4] 'c'
            db      08h             ; [07a5] '8'
            db      04h             ; [07a6] '4'
            db      00h             ; [07a7] '0'
            db      0ffh            ; [07a8] unused
            db      0ffh            ; [07a9] unused
            db      0ffh            ; [07aa] unused
            db      15h             ; [07ab] 'to tape'
            db      16h             ; [07ac] 'from tape'
            db      12h             ; [07ad] 'exec'

; 7-Segment LED Font Table & Special Display Patterns (Pages 203 - 204)
            org     07bfh
disp:           ; [07bf]
            db      0bdh            ; [07bf] '0'
            db      0bfh            ; [07c0] '8'
            db      02h             ; [07c1] '-'
            db      8dh             ; [07c2] 'c'
            db      0a7h            ; [07c3] 'b'
            db      0efh            ; [07c4] 'a'

; Special Message Bitmask Patterns (Page 204)
            db      00h             ; [07c5] ' '
            db      0aeh            ; [07c6] 's'
            db      30h             ; [07c7] 'i'
            db      00h             ; [07c8] ' '
            db      0aeh            ; [07c9] 's'
            db      30h             ; [07ca] 'i'
            db      37h             ; [07cb] 'h'
            db      87h             ; [07cc] 't'
            db      00h             ; [07cd] ' '
            db      0bdh            ; [07ce] 'o'
            db      85h             ; [07cf] 'l'
            db      85h             ; [07d0] 'l'
            db      8fh             ; [07d1] 'e'

; Power-On Display Format ("H      ", Page 204)
            org     07d2h
initab:         ; [07d2]
            db      37h             ; [07d2] 'h' (init display format)
            db      00h             ; [07d3] ' '
            db      00h             ; [07d4] ' '
            db      00h             ; [07d5] ' '
            db      00h             ; [07d6] ' '
            db      00h             ; [07d7] ' '
            db      00h             ; [07d8] ' '

; Error Display Format ("-ERROR", Page 204)
            org     07d9h
errtab:         ; [07d9]
            db      03h             ; [07d9] 'r' (digit 0)
            db      0a3h            ; [07da] 'o' (digit 1)
            db      03h             ; [07db] 'r' (digit 2)
            db      03h             ; [07dc] 'r' (digit 3)
            db      8fh             ; [07dd] 'e' (digit 4)
            db      02h             ; [07de] '-' (digit 5)
            db      00h             ; [07df] ' '

; Blank Display Format (Page 204)
            org     07e0h
blank:          ; [07e0]
            db      00h             ; [07e0] ' '
            db      00h             ; [07e1] ' '
            db      00h             ; [07e2] ' '
            db      00h             ; [07e3] ' '
            db      00h             ; [07e4] ' '
            db      00h             ; [07e5] ' '
            db      00h             ; [07e6] ' '
            db      00h             ; [07e7] ' '

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
segtab:         ; [07f0]
            db      0bdh            ; [07f0] '0' : a,b,c,d,e,f
            db      30h             ; [07f1] '1' : b,c
            db      9bh             ; [07f2] '2' : a,b,d,e,g
            db      0bah            ; [07f3] '3' : a,b,c,d,g
            db      36h             ; [07f4] '4' : b,c,f,g
            db      0aeh            ; [07f5] '5' : a,c,d,f,g
            db      0afh            ; [07f6] '6' : a,c,d,e,f,g
            db      38h             ; [07f7] '7' : a,b,c
            db      0bfh            ; [07f8] '8' : a,b,c,d,e,f,g
            db      0beh            ; [07f9] '9' : a,b,c,d,f,g
            db      0efh            ; [07fa] 'a' : a,b,c,e,f,g
            db      0a7h            ; [07fb] 'b' : c,d,e,f,g
            db      8dh             ; [07fc] 'c' : a,d,e,f
            db      0b3h            ; [07fd] 'd' : b,c,d,e,g
            db      8fh             ; [07fe] 'e' : a,d,e,f,g
            db      87h             ; [07ff] 'f' : a,e,f,g

; ==============================================================================
; End of ABC-80 Monitor ROM (07ffh / 2048 bytes)
; ==============================================================================

; Local Variables:
; mode: asm
; indent-tabs-mode: nil
; tab-width: 4
; End:
