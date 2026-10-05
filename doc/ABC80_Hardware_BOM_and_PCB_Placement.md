# ABC-80 Microcomputer Hardware Bill of Materials (BOM) & PCB Placement Guide

Copyright (C) 2026, Charles Chiou

This document provides the authoritative, comprehensive Bill of Materials (BOM), geometric footprints, pinout orientations, and physical coordinates for all discrete components, integrated circuits, and electromechanical assemblies on the **ABC-80 Z80 Single-Board Microcomputer Learning Kit**.

All component specifications and topological locations have been verified against the original hardware assembly guide (*《Z-80 微電腦製作 — 硬體分析・ABC-80製作及監督程式詳解》* Chapter 2, pp. 92–107), Figure 2-2 (*各零件的位置圖*), Figure 2-3 (*印刷電路板正反面圖*), and the golden reference photograph.

---

## 1. Visual Reference Diagrams

* **Hardware Reference Board**: [ABC-80 Reference Photo](./assets/abc80_board.png)

```text
+---------------------------------------------------------------------------------------------+
|                                  ABC-80 HARDWARE OVERVIEW MAP                                |
|                                                                                             |
| [100uF] INT      [7404/u9] [139/u8]   50p 101x2 10uF [R] [M] [DC]   100uF 203 [LM7805/HS]  |
|                                         10k 330 203 10kx3                                   |
| [10k x 18]       [2732/u2] [2732/u3]        [6116/u4]                                       |
|                  (ROM 0)   (ROM 1)          (SRAM)                                          |
| [Z80A CPU/u1]                                                                               |
| (DIP-40)         [8255/u6] 33R Q17(NPN)     [EP LED]                                        |
|                  (Burner)  [7404/u10]       [1.8k]                                          |
| [3.3k x 8]                 [LM741/u11]                                                      |
| [Q9..Q16 (PNP)]  [8255/u5] 3.3k 1.8k 560 10k 47k    [ABC-80 EPROM Prototyping Breadboard]   |
| [120R x 8]       (Sys I/O) [EPROM Socket/u7]                                                |
|                                                                                             |
|                  [Q1..Q8 (PNP)] + [R_a..R_dp (120R/220R x 8)]                               |
|                  +------------------------------------------+    [330R x 2] [33R]           |
|                  |   6-DIGIT 7-SEGMENT LED DISPLAY (RED)    |    [Q18..Q19] [SP/Buzzer]     |
|                  +------------------------------------------+    [Audio LED]                |
|                                                                                             |
|                  +------------------------------------------+                               |
|                  |       DUAL 3x4 KEYPAD MATRIX (24 KEYS)   |                               |
|                  +------------------------------------------+                               |
+---------------------------------------------------------------------------------------------+
```

---

## 2. Comprehensive Bill of Materials (BOM)

### 2.1 Integrated Circuits (ICs) & Sockets (11 ICs, 5 Sockets)

| Ref ID | Part Number | Package | Description / Function | Pin 1 Orientation | Socket Used |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **u1** | Z80A CPU | DIP-40 | 8-bit Central Processing Unit (2.500 MHz) | Notch facing UP | 40-Pin Dual-Wipe |
| **u2** | 2732 (or 2716) | DIP-24 | 4KB (or 2KB) UV-EPROM Monitor Firmware | Notch facing UP | 24-Pin Dual-Wipe |
| **u3** | 2732 | DIP-24 | 4KB Expansion ROM / User Utility Firmware | Notch facing UP | 24-Pin Dual-Wipe |
| **u4** | HM6116P-3 | DIP-24 | 2KB High-Speed CMOS Static RAM | Notch facing UP | 24-Pin Dual-Wipe |
| **u5** | 8255A / PPI #1 | DIP-40 | System Parallel I/O (Keypad, Display, Cassette) | Notch facing UP | 40-Pin Dual-Wipe |
| **u6** | 8255A / PPI #2 | DIP-40 | EPROM Programmer Dedicated Parallel I/O | Notch facing UP | 40-Pin Dual-Wipe |
| **u7** | 2716/2732 Socket | DIP-24/28 | Target EPROM Burning Socket (Textool/DIP) | Notch facing UP | 24/28-Pin DIP |
| **u8** | 74LS139 | DIP-16 | Dual 2-to-4 Line Decoder / Demultiplexer | Notch facing UP | Direct Solder |
| **u9** | 7404 / 74LS04 | DIP-14 | Hex Inverter (Clock oscillator & glue logic) | Notch facing UP | Direct Solder |
| **u10** | 7404 / 74LS14 | DIP-14 | Hex Inverter / Schmitt Trigger | Notch facing UP | Direct Solder |
| **u11** | LM741 / $\mu$A741 | DIP-8 | Single Operational Amplifier (Cassette EAR input) | Notch/Dot facing UP | Direct Solder |
| **VR1** | LM7805 | TO-220 | +5V Linear Voltage Regulator (1.0A) | Tab down / Heatsink | Heatsink + Screw |

---

### 2.2 Transistors (17 Total: 16 PNP + 1 NPN)

All small-signal transistors are encapsulated in standard **TO-92** plastic packages with pinout **1: Emitter (E), 2: Base (B), 3: Collector (C)** (CS9012 / CS9013 pinout).

| Ref ID | Part Type | Polarity | Package | Function / Circuit | Placement Location | Orientation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Q1** | CS9012 | PNP | TO-92 | Digit A3 Display Drive Cathode/Anode | Above Display Bezel (Col 1) | Flat face DOWN, E-B-C |
| **Q2** | CS9012 | PNP | TO-92 | Digit A2 Display Drive Cathode/Anode | Above Display Bezel (Col 2) | Flat face DOWN, E-B-C |
| **Q3** | CS9012 | PNP | TO-92 | Digit A1 Display Drive Cathode/Anode | Above Display Bezel (Col 3) | Flat face DOWN, E-B-C |
| **Q4** | CS9012 | PNP | TO-92 | Digit A0 Display Drive Cathode/Anode | Above Display Bezel (Col 4) | Flat face DOWN, E-B-C |
| **Q5** | CS9012 | PNP | TO-92 | Digit D1 Display Drive Cathode/Anode | Above Display Bezel (Col 5) | Flat face DOWN, E-B-C |
| **Q6** | CS9012 | PNP | TO-92 | Digit D0 Display Drive Cathode/Anode | Above Display Bezel (Col 6) | Flat face DOWN, E-B-C |
| **Q7** | CS9012 | PNP | TO-92 | Spare / Decimal Point Drive Transistor | Above Display Bezel (Col 7) | Flat face DOWN, E-B-C |
| **Q8** | CS9012 | PNP | TO-92 | Display Multiplex Driver Transistor | Above Display Bezel (Col 8) | Flat face DOWN, E-B-C |
| **Q9** | CS9012 | PNP | TO-92 | Keypad Column 0 / Bus Scan Line 0 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q10** | CS9012 | PNP | TO-92 | Keypad Column 1 / Bus Scan Line 1 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q11** | CS9012 | PNP | TO-92 | Keypad Column 2 / Bus Scan Line 2 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q12** | CS9012 | PNP | TO-92 | Keypad Column 3 / Bus Scan Line 3 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q13** | CS9012 | PNP | TO-92 | Keypad Column 4 / Bus Scan Line 4 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q14** | CS9012 | PNP | TO-92 | Keypad Column 5 / Bus Scan Line 5 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q15** | CS9012 | PNP | TO-92 | Keypad Column 6 / Bus Scan Line 6 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q16** | CS9012 | PNP | TO-92 | Keypad Column 7 / Bus Scan Line 7 | Left Column (along 3.3k) | Flat face RIGHT, E-B-C |
| **Q17** | CS9013 | NPN | TO-92 | EPROM Programmer +21V Programming Switch | Middle (between u6 & u10) | Flat face LEFT, C-B-E |
| **Q18** | CS9012 | PNP | TO-92 | Audio Speaker Pre-amplifier Driver | Right of Display (near SP) | Flat face LEFT, E-B-C |
| **Q19** | CS9013 | NPN | TO-92 | Audio Speaker Push-Pull Power Output | Right of Display (near SP) | Flat face LEFT, E-B-C |

---

### 2.3 Resistors (49 Total: 1/4W 5% Carbon Film)

| Value | Qty | Color Bands | Circuit Function | Topological Placement on Board |
| :--- | :---: | :--- | :--- | :--- |
| **10 k$\Omega$** | 18 | Brown-Black-Orange-Gold | Z80 Bus pull-up ladder | Left column, vertical row alongside u1 (Z80) |
| **10 k$\Omega$** | 3 | Brown-Black-Orange-Gold | Audio bias / Power input filter | Top header, below DC jack & MIC jack |
| **10 k$\Omega$** | 1 | Brown-Black-Orange-Gold | Reset pull-up resistor | Top header, adjacent to Reset switch |
| **10 k$\Omega$** | 1 | Brown-Black-Orange-Gold | Op-amp feedback bias | Next to LM741 (u11) in Column 3 |
| **3.3 k$\Omega$** | 8 | Orange-Orange-Red-Gold | Keypad matrix column pull-ups | Left column, vertical row alongside u5 & Q9..Q16 |
| **3.3 k$\Omega$** | 1 | Orange-Orange-Red-Gold | Transistor base drive / tape filter | Adjacent to LM741 / Q17 |
| **120 $\Omega$** | 8 | Brown-Red-Brown-Gold | 7-Segment segment current limiters | Horizontally positioned above display bezel (with Q1..Q8) |
| **1.8 k$\Omega$** | 1 | Brown-Gray-Red-Gold | EPROM 21V indicator LED current limit | Next to "EP" LED above Proto Area |
| **1.8 k$\Omega$** | 1 | Brown-Gray-Red-Gold | LM741 audio pre-amplifier input resistor | Next to u11 (LM741) |
| **4.7 k$\Omega$** | 1 | Yellow-Violet-Red-Gold | Audio circuit pull-up / tone generator | Between u11 (LM741) and u7 (EPROM socket) |
| **560 $\Omega$** | 1 | Green-Blue-Brown-Gold | Transistor bias resistor | Below LM741 (u11) |
| **330 $\Omega$** | 2 | Orange-Orange-Brown-Gold | Speaker driver current limiters | Right of display, adjacent to Speaker (SP) |
| **330 $\Omega$** | 1 | Orange-Orange-Brown-Gold | DC input conditioning resistor | Top header, next to DC jack |
| **33 $\Omega$** | 1 | Orange-Orange-Black-Gold | 28V / 21V EPROM Burner supply dropper | Above Q17 (NPN) burner switch |
| **33 $\Omega$** | 1 | Orange-Orange-Black-Gold | Speaker output series damping resistor | Right of display, series connected to SP |
| **47 k$\Omega$** | 1 | Yellow-Violet-Orange-Gold | LM741 audio feedback resistor | Right of LM741 (u11) |

---

### 2.4 Capacitors (11 Total)

| Ref ID | Value | Voltage | Type | Appearance / Package | Circuit Role | Placement on PCB |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **C1** | 220 $\mu$F | 16V | Electrolytic | Blue radial, $\varnothing$ 6.3mm | DC input main filter | Top right, at DC barrel jack |
| **C2** | 100 $\mu$F | 16V | Electrolytic | Blue radial, $\varnothing$ 5.0mm | +5V VCC bulk decoupling | Top left, above Z80 CPU (u1) |
| **C3** | 100 $\mu$F | 16V | Electrolytic | Blue radial, $\varnothing$ 5.0mm | Prototyping breadboard rail filter | Column 4, top of Proto Area |
| **C4** | 10 $\mu$F | 16V | Electrolytic | Blue radial, $\varnothing$ 4.0mm | Reset delay / debounce filter | Top header, next to 10k pull-up |
| **C5** | 10 $\mu$F | 16V | Electrolytic | Blue radial, $\varnothing$ 4.0mm | Cassette EAR audio AC coupling | Near LM741 (u11) audio input |
| **C6** | 0.1 $\mu$F (104) | 50V | Ceramic Disc | Ochre/yellow disc, 5mm pitch | High-frequency logic decoupling | Top header, near 7805 regulator |
| **C7** | 0.1 $\mu$F (104) | 50V | Ceramic Disc | Ochre/yellow disc, 5mm pitch | High-frequency logic decoupling | Center column, bridging u2 & u3 |
| **C8** | 0.02 $\mu$F (203) | 50V | Ceramic Disc | Ochre/yellow disc, 5mm pitch | 7805 output ripple bypass | Directly next to 7805 regulator |
| **C9** | 0.02 $\mu$F (203) | 50V | Ceramic Disc | Ochre/yellow disc, 5mm pitch | Audio bandpass filter | Top header, between R and M jacks |
| **C10** | 100 pF (101) | 50V | Ceramic Disc | Small orange disc, 2.5mm pitch | Clock oscillator filter | Near 7404 (u9) clock section |
| **C11** | 50 pF (50p) | 50V | Ceramic Disc | Small orange disc, 2.5mm pitch | Clock crystal fine-tuning capacitor | Top of Column 2, next to 101 cap |

---

### 2.5 Diodes & LEDs (4 Diodes, 3 LEDs)

| Ref ID | Part Type | Package | Description | Physical Appearance | Placement on PCB |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **D1** | 1N914 | DO-35 | High-speed switching silicon diode | Amber glass, black cathode band | Top header (polarity protection) |
| **D2** | 1N914 | DO-35 | Cassette EAR input peak detector | Amber glass, black cathode band | Next to LM741 (u11) |
| **D3** | 1N914 | DO-35 | Speaker inductive spike flyback diode | Amber glass, black cathode band | Adjacent to SP speaker |
| **D4** | 1N914 | DO-35 | Bus isolation diode | Amber glass, black cathode band | Below u5 (PIO #1) |
| **LED1** | 3mm Red | T-1 | **HALT Indicator LED** (CPU Halt state) | Diffused red dome, cathode notch | Next to Z80 CPU / Column 1 |
| **LED2** | 3mm Green | T-1 | **EP Indicator LED** (21V Burner Active) | Diffused green dome, cathode notch | Above Proto Area (Column 4) |
| **LED3** | 3mm Red | T-1 | **Audio / Power Active Indicator LED** | Diffused red dome, cathode notch | Next to Speaker (SP) |

---

### 2.6 Electromechanical, Connectors & Acoustic Assemblies

| Ref ID | Component Name | Physical Package / Description | Placement on PCB |
| :--- | :--- | :--- | :--- |
| **DISP1** | 6-Digit 7-Segment Display | 6× Common-Cathode 0.56" LED displays with ruby red optical filter | Center horizontal, $Y \in [425..474]$ |
| **KEY1** | Dual 3x4 Keypad Modules | 24× Tactile square key switches with custard yellow & putty keycaps | Bottom half, $Y \in [480..720]$ |
| **SW1** | Hardware Reset Switch | Miniature tactile pushbutton switch with red plunger ($909) | Top header, $X \approx 348, Y \approx 103$ |
| **J1** | DC Power Barrel Jack | 2.1mm center-positive / 5.5mm outer DC power connector | Top header, right side |
| **J2** | EAR Cassette Audio Jack | 3.5mm mono phone jack (black plastic body) | Top header, center |
| **J3** | MIC Cassette Audio Jack | 3.5mm mono phone jack (black plastic body) | Top header, center-left |
| **J4** | Bus Expansion Socket | 16-pin single-row female header socket strip (CONTO) | Left column, $X \approx 95, Y \approx 200$ |
| **SP1** | Dynamic Speaker / Buzzer | $\varnothing$ 27mm circular ferromagnetic acoustic transducer can | Middle right, directly adjacent to display |
| **L1** | RF Oscillator Coil | Red metal-shielded 10mm tunable RF coil with ferrite core | Adjacent to audio / DC jack |
| **PROTO** | Prototyping Breadboard Grid | Plated through-hole (PTH) breadboard grid (10 columns × 24 rows) | Column 4, bottom right area |

---

## 3. Geometric Coordinates & Physical Footprints

### 3.1 Normalized Board Coordinates (Standardized 600 × 800 Canvas)

In the standardized visual evaluation coordinate space ($W = 600\,\text{px}, H = 800\,\text{px}$):

```text
===================================================================================================
Zone Name                     Y-Range (px)    X-Range (px)    Key Components Enclosed
===================================================================================================
1. Power & Reset Header       0 .. 112        50 .. 550       LM7805, Heatsink, DC Jack, Reset,
                                                              C1 (220uF), C2 (100uF), R, M, DC
---------------------------------------------------------------------------------------------------
2. DIP IC & Discrete Logic    112 .. 424      50 .. 550       u1 (Z80), u5 (8255), u2 (2732),
                                                              u3 (2732), u4 (6116), u6 (8255),
                                                              u8 (139), u9 (04), u10 (04),
                                                              u11 (741), u7 (Burner Socket),
                                                              Q9..Q16 (PNP x 8), Q17 (NPN x 1),
                                                              10k x 18 Ladder, 3.3k x 8 Ladder,
                                                              120R x 8 Ladder, Proto Breadboard
---------------------------------------------------------------------------------------------------
3. 6-Digit LED Display Module 424 .. 536      190 .. 520      Q1..Q8 (PNP x 8), R_a..R_dp (8 Res),
                                                              Ruby Bezel ($X \in [205..403]$),
                                                              SP Speaker ($X \in [420..500]$),
                                                              Q18..Q19, Audio LED
---------------------------------------------------------------------------------------------------
4. Dual 3x4 Keypad Matrix     536 .. 800      60 .. 540       24 Tactile Keys, Ivory Bezels,
                                                              Left 3x4 ($X \in [110..270]$),
                                                              Right 3x4 ($X \in [330..490]$)
===================================================================================================
```

### 3.2 Web Emulator CSS Canvas Coordinates ($W = 500\,\text{px}, H = 700\,\text{px}$)

```css
/* CSS Dimensional Ground Truth */
--pcb-width: 500px;
--pcb-height: 700px;

/* Column Grids */
--col-left-width: 132px;     /* Column 1: Z80 + 8255 + Left Discretes */
--col-center-width: 64px;     /* Column 2: 7404 + 2732 ROM + 8255 Burner */
--col-right-width: 280px;     /* Column 3 & 4: Decoders, 6116 RAM, Op-Amp, Proto Grid */

/* Display Module */
--display-bezel-width: 202px;
--display-bezel-height: 48px;
--display-bezel-left: 104px;
--display-driver-strip-top: -12px;

/* Keypad Bezel Tray */
--keypad-tray-width: 440px;
--keypad-tray-height: 190px;
--keypad-bezel-width: 172px;
--keypad-bezel-height: 176px;
```

---

## 4. Reverse-Engineering Verification Notes for Future PCB Layout

1. **Active-Low Transistor Drivers**:
   - The 8 segment drivers ($Q_1..Q_8$) and 8 digit drivers are PNP transistors (CS9012). The Z80 firmware writes inverted bitmasks (`cpl` / `out (digit), a`) to assert cathode low.
2. **Keypad Matrix Rows and Columns**:
   - 8 column scan outputs drive the bases of $Q_9..Q_{16}$ (PNP), pulling columns to ground sequentially.
   - The $3.3\,\text{k}\Omega$ pull-up resistor ladder under `CANTGO` maintains high-impedance sense lines when keys are unpressed.
3. **EPROM Burner Voltage Generation**:
   - Transistor $Q_{17}$ (CS9013 NPN) switches the high-voltage programming rail (+21V or +25V derived from external transformer/tap) onto pin 1 ($V_{PP}$) of $u_7$.
4. **Cassette Tape Audio Conditioning**:
   - The LM741 ($u_{11}$) operational amplifier operates as a high-gain zero-crossing comparator. Pin 2 (inverting) and pin 3 (non-inverting) receive the low-amplitude audio signal from the EAR jack ($J_2$) via $10\,\mu\text{F}$ coupling capacitor $C_5$, outputting clean TTL-level pulses directly to bit 7 of Port C on $u_5$ (8255 PPI).
