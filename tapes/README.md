# ABC-80 Cassette Tape Library (`tapes/`)

This directory contains Z80 assembly source programs (`*.asm`) and their pre-modulated ABC-80 audio cassette counterparts (`*.tape`), built automatically during `make`:

- **`0001.tape`–`0029.tape`**: All 29 programming exercises (`實習 1`–`實習 29`) from *《Z80 微電腦入門 — 原理與應用程式》* (`scanned_docs/Z80_Micro_Computer_Primer.pdf`).
- **`0100.tape`–`0103.tape`**: Custom ABC-80 interactive showcase demos (`BCD Up-Counter`, `Segment Chaser`, `Dual-Tone Chime`, `Fibonacci Display`).

During the build:
1. `pasmo --bin tapes/<xxxx>.asm build/tapes/<xxxx>.bin` assembles each Z80 source program into raw machine code.
2. `./build/abc80_cli -c "tape encode tapes/<xxxx>.asm build/tapes/<xxxx>.bin tapes/<xxxx>.tape"` reads the `; Annotation:` and `; Start:` header comments from the `.asm` source and encodes the assembled machine code into a hardware-authentic `.tape` file.

Each `.tape` file stores a 1 kHz / 2 kHz square-wave pulse stream matching the ABC-80 Monitor ROM's `TO TAPE` (`ewt` at `0193H`) and `FROM TAPE` (`ert` at `01CEH`) cassette routines over the Intel 8255 PPI (`PC7` speaker/MIC output and `PA7` EAR input).

---

## Installing / Restoring Tapes to Your User Library

When `abc80_server` boots, it uses `$HOME/.config/abc-80/tapes/` as the active cassette library.

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

1. **Select the Tape Slot**: On the Cassette Deck UI, select the 4-digit hex File ID (e.g., `0001`–`0029` or `0100`–`0103`) and press **PLAY** (the reels begin spinning and the 1 kHz leader tone plays).
2. **Load into RAM (`FROM TAPE`)**: On the ABC-80 keypad, enter:
   ```text
   FROM TAPE  0  0  0  1  GO
   ```
   *(Replace `0 0 0 1` with any File ID `0 0 0 1`–`0 0 2 9` or `0 1 0 0`–`0 1 0 3`.)*
   - The Monitor ROM detects 1,000 cycles of the 1 kHz leader tone, reads the 7-byte header, displays the 4-digit filename on the 7-segment display for ~1.5 seconds, streams the program payload into RAM (`1000H`..end), verifies the 8-bit checksum, and stops at the program's end address in Data Mode.
3. **Execute the Program (`GO`)**: Re-arm the start address `1000H` and run:
   ```text
   ADRS  1  0  0  0  GO
   ```
4. **Return to Monitor**: Press **RST** on the ABC-80 keypad at any time to stop the running program and return to the `ABC-80` prompt.

---

## Primer Textbook Exercise Cassette Programs (`0001`–`0029`)

| File ID | Tape File | Source | Annotation | Address Range | Size | Book Exercise & Description |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| `0001` | `0001.tape` | `0001.asm` | `Lab01: Data Transfer (1000H)` | `1000H`–`1055H` | `86 B` | **實習 1：資料傳送練習** — 8-bit (`1050H..1054H`) & 16-bit (`1060H..1065H`) transfers + live `[Addr][Data]` 7-seg & speaker sweep. |
| `0002` | `0002.tape` | `0002.asm` | `Lab02: Logic AND & OR (1000H)` | `1000H`–`1052H` | `83 B` | **實習 2：邏輯指令練習 (一)** — 3-byte bitwise `AND` (`590760`) & `OR` (`FF4FF3`) alternating on the 6-digit display with speaker ticks. |
| `0003` | `0003.tape` | `0003.asm` | `Lab03: XOR CPL & NEG (1000H)` | `1000H`–`1053H` | `84 B` | **實習 3：邏輯指令練習 (二)** — `XOR` (`0FH`), `CPL` (`A4H`), `NEG` (`A5H`) cycling `A5A40F` and per-address views on the 7-seg display. |
| `0004` | `0004.tape` | `0004.asm` | `Lab04: BCD Addition (1000H)` | `1000H`–`1062H` | `99 B` | **實習 4：算術加法練習** — 6-digit BCD addition `473256 + 422344 = 895600` cycling operands and sum on the 7-seg display. |
| `0005` | `0005.tape` | `0005.asm` | `Lab05: BCD Subtraction (1000H)` | `1000H`–`1062H` | `99 B` | **實習 5：算術減法練習** — 6-digit BCD subtraction `796825 - 335836 = 460989` cycling minuend, subtrahend, and difference. |
| `0006` | `0006.tape` | `0006.asm` | `Lab06: Loop & Jump Delay (1000H)` | `1000H`–`1011H` | `18 B` | **實習 6：迴轉及跳越練習** — `SQWAV` square-wave generator toggling `8255 Port C (82H)` bit 7 with nested `B`/`C` delay loops. |
| `0007` | `0007.tape` | `0007.asm` | `Lab07: Max/Min Search (1000H)` | `1000H`–`1056H` | `87 B` | **實習 7：找尋最大值、最小值** — Finds max `9AH` (`1050H`) from `1052H..1056H` and animates candidate scan + winner flash. |
| `0008` | `0008.tape` | `0008.asm` | `Lab08: Shift & Rotate (1000H)` | `1000H`–`1052H` | `83 B` | **實習 8：移位與旋轉練習** — Computes `SLA A` (`DEH`) and `RRCA` (`9FH`), then animates rotating bit patterns on the 7-seg display. |
| `0009` | `0009.tape` | `0009.asm` | `Lab09: Monitor Calls (1000H)` | `1000H`–`104BH` | `76 B` | **實習 9：呼叫副程式** — Displays `" HELLO"` via `SCD&K` (`04A6H`); `-` (`11H`) blanks display, `+` (`10H`) scrolls `" HELLO"` in. |
| `0010` | `0010.tape` | `0010.asm` | `Lab10: Hex/BCD Display (1000H)` | `1000H`–`1052H` | `83 B` | **實習 10：資料顯示** — Decimal-adjusts (`DAA`) `1050H..1052H` (`"354071"`) and converts to 7-segment font via `2BYSEG` (`0525H`). |
| `0011` | `0011.tape` | `0011.asm` | `Lab11: Subroutines (1000H)` | `1000H`–`104BH` | `76 B` | **實習 11：副程式練習** — Scrolls `"FEDCBA"` across the 6-digit display (`CLRBF` at `1030H`) and waits for keypress via `SCD&K`. |
| `0012` | `0012.tape` | `0012.asm` | `Lab12: Blinking Display (1000H)` | `1000H`–`102BH` | `44 B` | **實習 12：交互顯示** — Alternates `"StOP"` (`1020H..1025H`) and `"  GO  "` (`1026H..102BH`) every 0.5 seconds. |
| `0013` | `0013.tape` | `0013.asm` | `Lab13: Scrolling Marquee (1000H)` | `1000H`–`1065H` | `102 B` | **實習 13：移動顯示** — Scrolls the 26-letter 7-segment alphabet (`"A..Z"`) continuously from right to left. |
| `0014` | `0014.tape` | `0014.asm` | `Lab14: Telephone Ring (1000H)` | `1000H`–`1035H` | `54 B` | **實習 14：電話聲響** — Synthesizes dual-frequency telephone ringing bursts (`320 Hz` / `480 Hz`) via `SOUND` (`047CH`). |
| `0015` | `0015.tape` | `0015.asm` | `Lab15: Police Siren (1000H)` | `1000H`–`1011H` | `18 B` | **實習 15：警車鳴聲** — Alternates `265 Hz` (`C=00H`) and `352 Hz` (`C=0C0H`) emergency siren tones via `SOUND` (`047CH`). |
| `0016` | `0016.tape` | `0016.asm` | `Lab16: Roulette Game (1000H)` | `1000H`–`1060H` | `97 B` | **實習 16：輪盤遊戲** — Spins a single lit segment around the 16-segment outer perimeter; holding `+` (`pos 03H`) stops the wheel. |
| `0017` | `0017.tape` | `0017.asm` | `Lab17: Pac-Man Sprite (1000H)` | `1000H`–`108EH` | `143 B` | **實習 17：小精靈** — Animates a chomping Pac-Man sprite across the 6 digits with `1 kHz` / `2 kHz` audio chirps (`MS1K`/`MS2K`). |
| `0018` | `0018.tape` | `0018.asm` | `Lab18: Computer Music (1000H)` | `1000H`–`103CH` | `61 B` | **實習 18：電腦音樂** — Plays the 53-byte encoded musical score at `1008H..103CH` via Monitor ROM `MUSIC` (`0411H`). |
| `0019` | `0019.tape` | `0019.asm` | `Lab19: Electronic Organ (1000H)` | `1000H`–`1032H` | `51 B` | **實習 19：電子琴** — Live 16-key musical instrument mapping hex keys `0`–`F` to the pitch table at `1023H..1032H`. |
| `0020` | `0020.tape` | `0020.asm` | `Lab20: Digital Stopwatch (1000H)` | `1000H`–`1027H` | `40 B` | **實習 20：馬錶** — 1/100th-second BCD stopwatch (`00-00` to `99-99`) paused while any keypad button is held. |
| `0021` | `0021.tape` | `0021.asm` | `Lab21: 24-Hour Clock (1000H)` | `1000H`–`1108H` | `265 B` | **實習 21：時鐘** — 24-hour BCD digital clock (`HH.MM.SS` at `1200H..1208H`) with decimal points and hourly chime (`1100H..1108H`). |
| `0022` | `0022.tape` | `0022.asm` | `Lab22: Signal Generator (1000H)` | `1000H`–`1017H` | `24 B` | **實習 22：信號產生器** — Continuous `1 kHz` calibrated square-wave generator on `8255 Port C (82H)` bit 7. |
| `0023` | `0023.tape` | `0023.asm` | `Lab23: Pulse Counter (1000H)` | `1000H`–`110AH` | `267 B` | **實習 23：計數器** — 6-digit BCD pulse/event counter (`000000`–`999999`) polling cassette EAR (`Port A bit 7`). |
| `0024` | `0024.tape` | `0024.asm` | `Lab24: Traffic Light (1000H)` | `1000H`–`1056H` | `87 B` | **實習 24：紅綠燈** — 2-way traffic light controller (`0CH, 0AH, 08H, 21H, 11H, 01H` with 3x yellow blink) on `8255 Port B (81H)`. |
| `0025` | `0025.tape` | `0025.asm` | `Lab25: 8-LED Chaser (1000H)` | `1000H`–`1018H` | `25 B` | **實習 25：燈光順序控制** — Rotates an active-low bit (`FEH` + `RLCA`) across 8 LEDs on `8255 Port B (81H)`. |
| `0026` | `0026.tape` | `0026.asm` | `Lab26: X-Y Scope Pattern (1000H)` | `1000H`–`1065H` | `102 B` | **實習 26：顯示圖案** — Streams 37 `(Y, X)` 4-bit R-2R DAC coordinate bytes (`1040H..1064H`) to `8255 Port B (81H)`. |
| `0027` | `0027.tape` | `0027.asm` | `Lab27: School Bell Clock (1000H)` | `1000H`–`10D6H` | `215 B` | **實習 27：校鐘** — 24-hour BCD clock triggering external bell relay (`PC6` low) and 8-note chime at `:00` and `:10` for hours $\ge 08:00$. |
| `0028` | `0028.tape` | `0028.asm` | `Lab28: Appliance Timer (1000H)` | `1000H`–`1124H` | `293 B` | **實習 28：家庭電器電腦化控制** — 24-hour BCD clock indexing `1100H..1124H` to drive 8 relays on **Auxiliary 8255 Port C (`42H`)**. |
| `0029` | `0029.tape` | `0029.asm` | `Lab29: Robotic Arm (1000H)` | `1000H`–`1173H` | `372 B` | **實習 29：模擬機械手** — Table-driven 3-axis 4-phase stepper motor + solenoid controller on **Expansion 8255 (`00H`–`03H`)**. |

---

## Custom Showcase Cassette Programs (`0100`–`0103`)

### 1. `0100.tape` — `BCD Up-Counter (1000H)`
- **File ID**: `0100`
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

### 2. `0101.tape` — `Segment Chaser (1000H)`
- **File ID**: `0101`
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

### 3. `0102.tape` — `Dual-Tone Chime (1000H)`
- **File ID**: `0102`
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

### 4. `0103.tape` — `Fibonacci Display (1000H)`
- **File ID**: `0103`
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
