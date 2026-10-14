# ABC-80 Cassette Tape Library (`tapes/`)

This directory contains Z80 assembly source programs (`0001.asm`–`0004.asm`) and their pre-modulated ABC-80 audio cassette counterparts (`0001.tape`–`0004.tape`), built automatically during `make`:

1. `pasmo --bin tapes/<xxxx>.asm build/tapes/<xxxx>.bin` assembles each Z80 source program into raw machine code.
2. `./build/abc80_cli -c "tape encode tapes/<xxxx>.asm build/tapes/<xxxx>.bin tapes/<xxxx>.tape"` reads the `; Annotation:` and `; Start:` header comments from the `.asm` source and encodes the assembled machine code into a hardware-authentic `.tape` file.

Each `.tape` file stores a 1 kHz / 2 kHz square-wave pulse stream matching the ABC-80 Monitor ROM's `TO TAPE` (`ewt` at `0193H`) and `FROM TAPE` (`ert` at `01CEH`) cassette routines over the Intel 8255 PPI (`PC7` speaker/MIC output and `PA7` EAR input).

---

## Installing / Restoring Tapes to Your User Library

When `abc80_server` boots, it uses `$HOME/.config/abc-80/tapes/` as the active cassette library (and automatically seeds `0001.tape`–`0004.tape` if that directory is empty).

To manually copy or restore these pre-built tapes into your local user library at any time:

```bash
mkdir -p ~/.config/abc-80/tapes
cp tapes/*.tape ~/.config/abc-80/tapes/
```

To assemble and encode all `tapes/*.asm` files into `tapes/*.tape`:

```bash
make tapes
```

---

## Loading and Running a Cassette Program on the ABC-80

1. **Select the Tape Slot**: On the Cassette Deck UI, select the 4-digit hex File ID (`0001`–`0004`) and press **PLAY** (the reels begin spinning and the 1 kHz leader tone plays).
2. **Load into RAM (`FROM TAPE`)**: On the ABC-80 keypad, enter:
   ```text
   FROM TAPE  0  0  0  1  GO
   ```
   *(Replace `0 0 0 1` with `0 0 0 2`, `0 0 0 3`, or `0 0 0 4`.)*
   - The Monitor ROM detects 1,000 cycles of the 1 kHz leader tone, reads the 7-byte header, displays the 4-digit filename (`0001`–`0004`) on the 7-segment display for ~1.5 seconds, streams the program payload into RAM (`1000H`..end), verifies the 8-bit checksum, and stops at the program's end address in Data Mode.
3. **Execute the Program (`GO`)**: Re-arm the start address `1000H` and run:
   ```text
   ADRS  1  0  0  0  GO
   ```
4. **Return to Monitor**: Press **RST** on the ABC-80 keypad at any time to stop the running program and return to the `ABC-80` prompt.

---

## Included Cassette Programs

### 1. `0001.tape` — `BCD Up-Counter (1000H)`
- **File ID**: `0001`
- **Address Range**: `1000H`–`1018H` (`25` bytes)
- **Description**: Continuously increments a 2-digit BCD counter in RAM at `1030H` using decimal adjust (`ADD A, 01H` / `DAA`) and displays the count (`00`–`99`) on the right two 7-segment digits via Monitor ROM subroutines `dadp` (`0511H`) and `scd_k1` (`04CDH`).

```z80
1000: AF          XOR  A            ; Initialize counter = 00H
1001: 32 30 10    LD   (1030H), A   ; Store current BCD count
1004: 3A 30 10    LD   A, (1030H)   ; Load current BCD count
1007: CD 11 05    CALL 0511H        ; DADP: format A into right 2 display digits
100A: 06 20       LD   B, 20H       ; 32 display scan passes (~0.32s per count)
100C: CD CD 04    CALL 04CDH        ; SCD_K1: multiplex 6-digit 7-segment display
100F: 10 FB       DJNZ 100CH        ; Hold display
1011: 3A 30 10    LD   A, (1030H)   ; Reload BCD count
1014: C6 01       ADD  A, 01H       ; Increment by 1
1016: 27          DAA               ; Decimal adjust for BCD (00..99)
1017: 18 E8       JR   1001H        ; Loop indefinitely
```

---

### 2. `0002.tape` — `Segment Chaser (1000H)`
- **File ID**: `0002`
- **Address Range**: `1000H`–`1015H` (`22` bytes)
- **Description**: Drives the Intel 8255 PPI segment bus (`Port B = 81H`) and digit cathodes (`Port C = 82H`) directly, rotating an active-low segment bit (`RLC A`) across all 6 LED digits with a software delay loop.

```z80
1000: 3E FE       LD   A, 0FEH      ; Initial active-low segment mask (11111110b)
1002: D3 81       OUT  (81H), A     ; Drive 8255 Port B (7-segment bus)
1004: 47          LD   B, A         ; Save current segment mask in B
1005: 3E C0       LD   A, 0C0H      ; Enable all 6 digit cathodes (PC0..PC5 low)
1007: D3 82       OUT  (82H), A     ; Drive 8255 Port C
1009: 21 00 30    LD   HL, 3000H    ; Delay counter
100C: 2B          DEC  HL           ; Decrement delay counter
100D: 7C          LD   A, H
100E: B5          OR   L
100F: 20 FB       JR   NZ, 100CH    ; Wait until HL == 0
1011: 78          LD   A, B         ; Restore segment mask
1012: CB 07       RLC  A            ; Rotate active segment bit left
1014: 18 EC       JR   1002H        ; Repeat chaser loop
```

---

### 3. `0003.tape` — `Dual-Tone Chime (1000H)`
- **File ID**: `0003`
- **Address Range**: `1000H`–`100EH` (`15` bytes)
- **Description**: Alternates between `250` cycles of a 1 kHz tone (`ms1k` at `0470H`, ~0.25s) and `500` cycles of a 2 kHz tone (`ms2k` at `0475H`, ~0.25s) on the ABC-80 speaker (`8255 Port C bit 7`) in a continuous loop.

```z80
1000: 21 FA 00    LD   HL, 00FAH    ; 250 cycles @ 1 kHz (~0.25s)
1003: CD 70 04    CALL 0470H        ; MS1K: generate 1 kHz square wave on PC7
1006: 21 F4 01    LD   HL, 01F4H    ; 500 cycles @ 2 kHz (~0.25s)
1009: CD 75 04    CALL 0475H        ; MS2K: generate 2 kHz square wave on PC7
100C: C3 00 10    JP   1000H        ; Repeat dual-tone chime
```

---

### 4. `0004.tape` — `Fibonacci Display (1000H)`
- **File ID**: `0004`
- **Address Range**: `1000H`–`1023H` (`36` bytes)
- **Description**: Computes the decimal (BCD) Fibonacci sequence (`01, 01, 02, 03, 05, 08, 13, 21, 34, 55, 89`), stores each term into RAM at `1040H`–`104AH`, shows the destination RAM address (`1040`–`104A`) on the left 4 address digits via `adrsdp` (`0504H`) and the Fibonacci value on the right 2 data digits via `dadp` (`0511H`), emits a 3 kHz speaker tick via `ms3k17` (`0464H`) on each term, and wraps back to `1040 01` after `89`.

```z80
1000: 11 40 10    LD   DE, 1040H    ; DE = destination table address (1040H..104AH)
1003: 01 01 00    LD   BC, 0001H    ; B = F(n-1) = 00H, C = F(n) = 01H
1006: 79          LD   A, C         ; A = current BCD Fibonacci term
1007: 12          LD   (DE), A      ; Store term in RAM table at (DE)
1008: CD 11 05    CALL 0511H        ; DADP: format A onto right 2 data digits
100B: CD 04 05    CALL 0504H        ; ADRSDP: format DE onto left 4 address digits
100E: CD 64 04    CALL 0464H        ; MS3K17: emit 3 kHz speaker tick
1011: C5          PUSH BC           ; Save Fibonacci pair (B, C)
1012: 06 28       LD   B, 28H       ; 40 display scan passes (~0.4s)
1014: CD CD 04    CALL 04CDH        ; SCD_K1: multiplex 6-digit 7-segment display
1017: 10 FB       DJNZ 1014H        ; Hold display
1019: C1          POP  BC           ; Restore Fibonacci pair (B, C)
101A: 79          LD   A, C         ; A = F(n)
101B: 80          ADD  A, B         ; A = F(n) + F(n-1)
101C: 27          DAA               ; BCD adjust (01,01,02,03,05,08,13,21,34,55,89)
101D: 38 E1       JR   C, 1000H     ; Restart at 1000H when 2-digit BCD overflows
101F: 41          LD   B, C         ; B = old F(n)
1020: 4F          LD   C, A         ; C = new F(n+1)
1021: 13          INC  DE           ; Advance table pointer
1022: 18 E2       JR   1006H        ; Next term
```

---

## Binary `.tape` File Format Specification

Each `.tape` file consists of a 48-byte `TapeFileHeader` followed by `runCount` `TapePulseRun` records (`8` bytes each) representing alternating high/low half-periods at the 1.79 MHz Z80 CPU clock:

| Offset | Size | Type | Field | Description |
| :--- | :---: | :---: | :--- | :--- |
| `0x00` | `8 B` | `char[8]` | `magic` | `"ABC80TAP"` |
| `0x08` | `2 B` | `uint16_t` | `version` | Format version (`1`, little-endian) |
| `0x0A` | `2 B` | `uint16_t` | `fileId` | 16-bit File ID (`0x0000`–`0xFFFF`, little-endian) |
| `0x0C` | `32 B` | `char[32]` | `annotation` | 32-character ASCII label (null-padded) |
| `0x2C` | `4 B` | `uint32_t` | `runCount` | Number of `TapePulseRun` records (`uint32_t halfPeriodTStates`, `uint32_t halfCycles`) |
