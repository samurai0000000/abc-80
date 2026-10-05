#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
ABC-80 Professional Hardware Schematic Generator (Matplotlib Engine)
Renders publication-grade, pixel-perfect PNG and SVG schematics with dark-mode styling.
"""

from pathlib import Path
import os
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.patches import FancyBboxPatch, BoxStyle

OUTPUT_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "doc/assets")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Color Palette (Tailwind Dark Palette)
BG_COLOR = "#0b0f19"
PANEL_BG = "#131b2e"
PANEL_BORDER = "#233354"
TEXT_WHITE = "#f8fafc"
TEXT_MUTED = "#94a3b8"
TEXT_DIM = "#64748b"

COLOR_CPU = "#0284c7"       # Sky blue
COLOR_ROM_RAM = "#4f46e5"   # Indigo
COLOR_DECODER = "#0d9488"   # Teal
COLOR_PPI = "#d97706"       # Amber
COLOR_DISPLAY = "#dc2626"   # Red
COLOR_KEYPAD = "#059669"    # Emerald
COLOR_AUDIO = "#7c3aed"     # Purple
COLOR_MISC = "#334155"      # Slate

BUS_ADDR = "#38bdf8"        # Light Sky
BUS_DATA = "#34d399"        # Light Emerald
BUS_CTRL = "#fbbf24"        # Light Amber
BUS_CS   = "#c084fc"        # Light Purple
BUS_AUDIO = "#f43f5e"       # Rose

def draw_rounded_box(ax, x, y, w, h, title="", subtitle="", lines=None, 
                     title_color=COLOR_CPU, bg_color=PANEL_BG, border_color=PANEL_BORDER,
                     title_fontsize=11, body_fontsize=8.5, pad_x=0.012):
    """Draws a clean structured IC or subsystem box with header and body text."""
    # Outer box
    box = FancyBboxPatch((x, y), w, h,
                         boxstyle=BoxStyle("Round", pad=0.0, rounding_size=0.015),
                         facecolor=bg_color, edgecolor=border_color, linewidth=1.5, zorder=2)
    ax.add_patch(box)
    
    # Title banner
    header_h = 0.038 if title else 0
    if title:
        header_box = FancyBboxPatch((x, y + h - header_h), w, header_h,
                                    boxstyle=BoxStyle("Round", pad=0.0, rounding_size=0.015),
                                    facecolor=title_color, edgecolor=title_color, linewidth=1.0, zorder=3)
        ax.add_patch(header_box)
        header_sq = patches.Rectangle((x, y + h - header_h), w, header_h/2,
                                      facecolor=title_color, edgecolor=title_color, zorder=3)
        ax.add_patch(header_sq)
        
        ax.text(x + w/2, y + h - header_h/2, title,
                color="#ffffff", fontsize=title_fontsize, fontweight="bold",
                ha="center", va="center", zorder=4)
    
    # Subtitle
    curr_y = y + h - header_h - 0.018
    if subtitle:
        ax.text(x + w/2, curr_y, subtitle,
                color=TEXT_WHITE, fontsize=body_fontsize+0.5, fontweight="bold",
                ha="center", va="center", zorder=4)
        curr_y -= 0.022
    
    # Body Lines
    if lines:
        for line in lines:
            if isinstance(line, tuple):
                left_txt, right_txt = line
                ax.text(x + pad_x, curr_y, left_txt, color=TEXT_MUTED, fontsize=body_fontsize, ha="left", va="center", zorder=4)
                ax.text(x + w - pad_x, curr_y, right_txt, color=TEXT_WHITE, fontsize=body_fontsize, fontweight="bold", ha="right", va="center", zorder=4)
            else:
                ax.text(x + pad_x, curr_y, line, color=TEXT_MUTED, fontsize=body_fontsize, ha="left", va="center", zorder=4)
            curr_y -= 0.022

def draw_bus(ax, points, label="", color="#38bdf8", lw=2.0, ls="-", label_pos=None, label_color=None, arrow=True):
    """Draws clean orthogonal bus wires with arrows and badges."""
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    ax.plot(xs, ys, color=color, linewidth=lw, linestyle=ls, zorder=5)
    
    if arrow and len(points) >= 2:
        p_prev = points[-2]
        p_last = points[-1]
        dx = p_last[0] - p_prev[0]
        dy = p_last[1] - p_prev[1]
        ax.annotate('', xy=p_last, xytext=(p_last[0] - 0.0001*dx, p_last[1] - 0.0001*dy),
                    arrowprops=dict(arrowstyle="-|>", color=color, lw=lw, mutation_scale=12),
                    zorder=6)
        
    if label:
        lx, ly = label_pos if label_pos else (xs[len(xs)//2], ys[len(ys)//2])
        lcolor = label_color if label_color else color
        ax.text(lx, ly, f" {label} ", color=lcolor, fontsize=8.2, fontweight="bold",
                ha="center", va="center", zorder=7,
                bbox=dict(boxstyle="round,pad=0.25", facecolor="#0b0f19", edgecolor=color, lw=1.0, alpha=0.95))


def render_topology_diagram():
    """Generates the master System Bus & Interconnection Topology Diagram."""
    fig, ax = plt.subplots(figsize=(16, 10), dpi=300)
    fig.patch.set_facecolor(BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    
    # Title Banner
    ax.text(0.5, 0.97, "ABC-80 MICROCOMPUTER — SYSTEM BUS & ARCHITECTURE TOPOLOGY",
            color="#ffffff", fontsize=15, fontweight="bold", ha="center", va="center")
    ax.text(0.5, 0.942, "Zilog Z80A CPU (2.5 MHz) • 4KB EPROM • 2KB SRAM • Intel 8255 PPI • Front Panel & Peripherals",
            color=TEXT_MUTED, fontsize=9.5, ha="center", va="center")

    # 1. CPU Block (Top-Left)
    draw_rounded_box(ax, 0.04, 0.58, 0.22, 0.32,
                     title="Z80A CPU (IC1)", subtitle="2.5 MHz 8-Bit Microprocessor",
                     lines=[
                         ("Clock (Pin 6):", "2.5 MHz Osc"),
                         ("Reset (Pin 26):", "/RESET"),
                         ("Interrupt (Pin 16):", "/INT (Single-Step)"),
                         ("Status (Pin 18):", "/HALT LED"),
                         ("Address Bus:", "A0 – A15"),
                         ("Data Bus:", "D0 – D7 (Bidirectional)"),
                         ("Control Bus:", "/MREQ, /RD, /WR")
                     ],
                     title_color=COLOR_CPU)

    # Clock & Reset Sub-circuits (Left of CPU)
    draw_rounded_box(ax, 0.04, 0.44, 0.105, 0.11,
                     title="Clock Circuit", title_fontsize=9, body_fontsize=7.5,
                     lines=["2.5 MHz Crystal", "+ 74LS04 Inv."],
                     title_color=COLOR_MISC)
    
    draw_rounded_box(ax, 0.155, 0.44, 0.105, 0.11,
                     title="Reset / Single-Step", title_fontsize=9, body_fontsize=7.5,
                     lines=["RC + [RST] Key", "74LS74 /M1 -> /INT"],
                     title_color=COLOR_MISC)

    # Connections to CPU
    draw_bus(ax, [(0.092, 0.55), (0.092, 0.58)], label="", color=BUS_ADDR, lw=1.5)
    draw_bus(ax, [(0.207, 0.55), (0.207, 0.58)], label="", color=BUS_CTRL, lw=1.5)

    # 2. Address Decoder Block (Top-Center)
    draw_rounded_box(ax, 0.33, 0.68, 0.25, 0.22,
                     title="Address Decoders (IC4/5)", subtitle="74LS138 / 74LS139 Decoders",
                     lines=[
                         ("Inputs:", "A12 – A15, /MREQ"),
                         ("/CS0 (0x0000–0x0FFF):", "2732 Monitor ROM"),
                         ("/CS1 (0x1000–0x17FF):", "6116 User RAM"),
                         ("/CS2 (0x2000–0x2FFF):", "8255 PPI (I/O)"),
                         ("/CS3 (0x3000–0x3FFF):", "2732 Burner Socket"),
                         ("/CS4–/CS7:", "System Expansion")
                     ],
                     title_color=COLOR_DECODER)

    # 3. Memory & Bus Devices (Right Column)
    # ROM
    draw_rounded_box(ax, 0.66, 0.77, 0.30, 0.13,
                     title="2732 EPROM (IC2) — Monitor ROM", subtitle="4 KB System Firmware (0x0000 – 0x0FFF)",
                     lines=[
                         ("Chip Enable / OE:", "/CS0, /RD"),
                         ("Bus Interface:", "A0–A11, D0–D7"),
                         ("Firmware Contents:", "Monitor, Keypad, 7-Seg, Cassette")
                     ],
                     title_color=COLOR_ROM_RAM)

    # RAM
    draw_rounded_box(ax, 0.66, 0.61, 0.30, 0.13,
                     title="6116 SRAM (IC3) — User RAM", subtitle="2 KB Static RAM (0x1000 – 0x17FF)",
                     lines=[
                         ("Chip Enable / OE / WE:", "/CS1, /RD, /WR"),
                         ("Bus Interface:", "A0–A10, D0–D7"),
                         ("System Workspace:", "0x1380–0x13FF (Stack & Regs)")
                     ],
                     title_color=COLOR_ROM_RAM)

    # 8255 PPI (Center-Right)
    draw_rounded_box(ax, 0.33, 0.36, 0.25, 0.25,
                     title="Intel 8255 PPI (IC6)", subtitle="Memory-Mapped Peripheral I/O (0x2000)",
                     lines=[
                         ("Chip Select & Regs:", "/CS2, A0, A1, /RD, /WR"),
                         ("Port A (PA0–PA3):", "Keypad Column Inputs (4.7kΩ PU)"),
                         ("Port B (PB0–PB7):", "7-Segment Anode Drivers (a–g, dp)"),
                         ("Port C (PC0–PC5):", "Digit Cathodes & Key Rows (via PNP)"),
                         ("Port C (PC6–PC7):", "Audio Cassette I/O & Buzzer")
                     ],
                     title_color=COLOR_PPI)

    # Target EPROM Socket (Right of 8255)
    draw_rounded_box(ax, 0.66, 0.45, 0.30, 0.13,
                     title="2732 Target EPROM Socket (IC7)", subtitle="Onboard Programmer (0x3000 – 0x3FFF)",
                     lines=[
                         ("Chip Enable / Vpp:", "/CS3, +21V / +25V Pulse"),
                         ("Bus Interface:", "A0–A11, D0–D7, /WR"),
                         ("Application:", "Burn custom 2732/2716 EPROMs")
                     ],
                     title_color=COLOR_MISC)

    # 4. Front Panel & I/O Peripherals (Bottom Row)
    # 7-Segment Display
    draw_rounded_box(ax, 0.04, 0.06, 0.27, 0.23,
                     title="6-Digit 7-Segment Display", subtitle="Common-Cathode LED Multiplexing",
                     lines=[
                         ("Address Display:", "4 Digits (D4, D3, D2, D1)"),
                         ("Data Display:", "2 Digits (D2, D1)"),
                         ("Segment Bus:", "PB0–PB7 via 8x 220Ω Resistors"),
                         ("Cathode Drivers:", "PC0–PC5 via 6x CS9012 PNP Transistors"),
                         ("Multiplex Rate:", "Dynamic scan via Timer/Monitor")
                     ],
                     title_color=COLOR_DISPLAY)

    # Keypad Matrix
    draw_rounded_box(ax, 0.36, 0.06, 0.28, 0.23,
                     title="24-Key Switch Matrix", subtitle="Hex & Monitor Function Keypad",
                     lines=[
                         ("Hex Data Keys (16):", "[ 0 ] – [ F ] (Rows 0–3)"),
                         ("Control Keys (8):", "ADRS, DATA, +, -, GO, STEP, EXEC, RST"),
                         ("Row Drive (Strobes):", "PC0–PC3 (Active-Low Strobe)"),
                         ("Column Sense:", "PA0–PA3 (4.7kΩ Pull-Up to +5V)"),
                         ("Debounce:", "Software debounce in ROM KEYIN")
                     ],
                     title_color=COLOR_KEYPAD)

    # Cassette / Audio
    draw_rounded_box(ax, 0.69, 0.06, 0.27, 0.23,
                     title="Cassette & Audio Subsystem", subtitle="FSK Modulation & Tone Generator",
                     lines=[
                         ("Tone / MIC Out:", "PC6 -> CS9013 NPN -> Speaker & MIC"),
                         ("EAR / Tape In:", "EAR Jack -> LM324 Comp. -> PC7"),
                         ("FSK Encoding:", "1000 Hz ('0') / 2000 Hz ('1')"),
                         ("Tape Format:", "Leader + Name + Addr + Data + Checksum"),
                         ("Baud Rate:", "~600-1200 Baud Audio FSK")
                     ],
                     title_color=COLOR_AUDIO)

    # MAIN BUS WIRING
    # CPU -> Decoders (A12-A15, /MREQ)
    draw_bus(ax, [(0.26, 0.82), (0.33, 0.82)], label="A12-A15, /MREQ", color=BUS_ADDR, label_pos=(0.295, 0.835))

    # Decoders -> /CS lines
    draw_bus(ax, [(0.58, 0.84), (0.66, 0.84)], label="/CS0 (0x0000)", color=BUS_CS, label_pos=(0.62, 0.855))
    draw_bus(ax, [(0.58, 0.73), (0.62, 0.73), (0.62, 0.68), (0.66, 0.68)], label="/CS1 (0x1000)", color=BUS_CS, label_pos=(0.62, 0.745))
    draw_bus(ax, [(0.455, 0.68), (0.455, 0.61)], label="/CS2 (0x2000)", color=BUS_CS, label_pos=(0.455, 0.645))
    draw_bus(ax, [(0.58, 0.70), (0.63, 0.70), (0.63, 0.52), (0.66, 0.52)], label="/CS3 (0x3000)", color=BUS_CS, label_pos=(0.62, 0.535))

    # Master Bus Trunk (CPU to RAM/ROM/PPI/Burner)
    draw_bus(ax, [(0.26, 0.70), (0.30, 0.70), (0.30, 0.915), (0.81, 0.915), (0.81, 0.90)], label="Address & Data Buses (A0-A11, D0-D7, /RD, /WR)", color=BUS_DATA, lw=2.5, label_pos=(0.55, 0.915))
    draw_bus(ax, [(0.81, 0.915), (0.81, 0.77)], color=BUS_DATA, lw=1.5, arrow=False) # to ROM
    draw_bus(ax, [(0.81, 0.77), (0.81, 0.74)], color=BUS_DATA, lw=1.5) # into RAM
    draw_bus(ax, [(0.30, 0.70), (0.30, 0.50), (0.33, 0.50)], label="A0-A1, D0-D7, /RD, /WR", color=BUS_DATA, lw=2.0, label_pos=(0.28, 0.515))

    # 8255 to Peripherals
    # Port B -> 7-Segment (PB0-PB7)
    draw_bus(ax, [(0.33, 0.42), (0.175, 0.42), (0.175, 0.29)], label="PB0–PB7 (Segment Drive via 8x 220Ω)", color=COLOR_DISPLAY, lw=2.0, label_pos=(0.23, 0.435))
    
    # Port C -> Digit Drivers & Keypad Rows (PC0-PC5)
    draw_bus(ax, [(0.38, 0.36), (0.38, 0.33), (0.12, 0.33), (0.12, 0.29)], label="PC0–PC5 (Digit Cathodes)", color=BUS_ADDR, lw=2.0, label_pos=(0.23, 0.345))
    draw_bus(ax, [(0.44, 0.36), (0.44, 0.29)], label="PC0–PC3 (Row Strobes)", color=COLOR_KEYPAD, lw=2.0, label_pos=(0.42, 0.325))
    
    # Keypad Columns -> Port A (PA0-PA3)
    draw_bus(ax, [(0.54, 0.29), (0.54, 0.36)], label="PA0–PA3 (Col Sense)", color=COLOR_KEYPAD, lw=2.0, label_pos=(0.56, 0.325))

    # Port C -> Audio / Cassette (PC6, PC7)
    draw_bus(ax, [(0.58, 0.40), (0.825, 0.40), (0.825, 0.29)], label="PC6 (Tone Out) / PC7 (Tape In)", color=COLOR_AUDIO, lw=2.0, label_pos=(0.70, 0.415))

    plt.tight_layout()
    plt.savefig(f"{OUTPUT_DIR}/abc80_topology.png", dpi=300, facecolor=BG_COLOR, edgecolor="none")
    plt.savefig(f"{OUTPUT_DIR}/abc80_topology.svg", facecolor=BG_COLOR, edgecolor="none")
    plt.close()
    print("Rendered topology diagram (PNG & SVG).")


def render_cpu_8255_diagram():
    """Generates the detailed CPU <-> Intel 8255 PPI Interface Diagram."""
    fig, ax = plt.subplots(figsize=(15, 9), dpi=300)
    fig.patch.set_facecolor(BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    ax.text(0.5, 0.96, "ABC-80 CPU TO INTEL 8255 PPI INTERCONNECTION SCHEMATIC",
            color="#ffffff", fontsize=15, fontweight="bold", ha="center", va="center")
    ax.text(0.5, 0.93, "Direct Pin-to-Pin Bus Routing, Address Selection & Active-High Reset Translation",
            color=TEXT_MUTED, fontsize=9.5, ha="center", va="center")

    # CPU Box (Left)
    draw_rounded_box(ax, 0.06, 0.12, 0.26, 0.76,
                     title="Zilog Z80A CPU (IC1)", subtitle="40-Pin DIP Microprocessor",
                     lines=[
                         ("Pin 14  •  D0", "Data Bit 0"),
                         ("Pin 15  •  D1", "Data Bit 1"),
                         ("Pin 12  •  D2", "Data Bit 2"),
                         ("Pin 8   •  D3", "Data Bit 3"),
                         ("Pin 7   •  D4", "Data Bit 4"),
                         ("Pin 9   •  D5", "Data Bit 5"),
                         ("Pin 10  •  D6", "Data Bit 6"),
                         ("Pin 13  •  D7", "Data Bit 7"),
                         ("-----------------", "-----------------"),
                         ("Pin 30  •  A0", "Address Bit 0"),
                         ("Pin 31  •  A1", "Address Bit 1"),
                         ("-----------------", "-----------------"),
                         ("Pin 21  •  /RD", "Read Strobe"),
                         ("Pin 22  •  /WR", "Write Strobe"),
                         ("Pin 26  •  /RESET", "Active-Low Reset"),
                         ("-----------------", "-----------------"),
                         ("Pins 2–5 • A12–15", "High Address"),
                         ("Pin 19  •  /MREQ", "Memory Request")
                     ],
                     title_color=COLOR_CPU, body_fontsize=8)

    # 8255 PPI Box (Right)
    draw_rounded_box(ax, 0.68, 0.12, 0.26, 0.76,
                     title="Intel 8255 PPI (IC6)", subtitle="40-Pin DIP Programmable Peripheral Interface",
                     lines=[
                         ("D0  •  Pin 34", "Data Bit 0"),
                         ("D1  •  Pin 33", "Data Bit 1"),
                         ("D2  •  Pin 32", "Data Bit 2"),
                         ("D3  •  Pin 31", "Data Bit 3"),
                         ("D4  •  Pin 30", "Data Bit 4"),
                         ("D5  •  Pin 29", "Data Bit 5"),
                         ("D6  •  Pin 28", "Data Bit 6"),
                         ("D7  •  Pin 27", "Data Bit 7"),
                         ("-----------------", "-----------------"),
                         ("A0  •  Pin 9", "Port Select 0"),
                         ("A1  •  Pin 8", "Port Select 1"),
                         ("-----------------", "-----------------"),
                         ("/RD  •  Pin 5", "Read Input"),
                         ("/WR  •  Pin 36", "Write Input"),
                         ("RESET • Pin 35", "Active-High Reset"),
                         ("-----------------", "-----------------"),
                         ("/CS  •  Pin 6", "Chip Select"),
                         ("(0x2000–0x2FFF)", "Decoded Base")
                     ],
                     title_color=COLOR_PPI, body_fontsize=8)

    # Glue Logic (Center)
    draw_rounded_box(ax, 0.38, 0.14, 0.24, 0.16,
                     title="74LS138 / 139 Decoder", subtitle="Address Decoding (0x2000–0x2FFF)",
                     lines=[
                         ("Inputs:", "A12–A15, /MREQ"),
                         ("Output /CS2:", "Active-Low /CS to 8255 Pin 6")
                     ],
                     title_color=COLOR_DECODER, body_fontsize=8)

    draw_rounded_box(ax, 0.40, 0.34, 0.20, 0.12,
                     title="74LS04 Inverter", subtitle="Reset Polarizer",
                     lines=[
                         ("Input:", "Z80 /RESET (Active-Low)"),
                         ("Output:", "8255 RESET (Active-High)")
                     ],
                     title_color=COLOR_MISC, body_fontsize=8)

    # Connectors
    # 8-bit Data Bus Trunk
    draw_bus(ax, [(0.32, 0.72), (0.68, 0.72)], label="8-Bit Bidirectional Data Bus (D0–D7)", color=BUS_DATA, lw=3.0, label_pos=(0.50, 0.745))

    # A0, A1
    draw_bus(ax, [(0.32, 0.54), (0.68, 0.54)], label="Port Select Address Lines (A0, A1)", color=BUS_ADDR, lw=2.0, label_pos=(0.50, 0.565))

    # /RD, /WR
    draw_bus(ax, [(0.32, 0.48), (0.68, 0.48)], label="Read (/RD) & Write (/WR) Strobes", color=BUS_CTRL, lw=2.0, label_pos=(0.50, 0.505))

    # /RESET through 74LS04 Inverter
    draw_bus(ax, [(0.32, 0.40), (0.40, 0.40)], label="", color=BUS_AUDIO, lw=1.8)
    draw_bus(ax, [(0.60, 0.40), (0.68, 0.40)], label="RESET (Pin 35)", color=BUS_AUDIO, lw=1.8, label_pos=(0.64, 0.42))

    # A12-A15, /MREQ to Decoder -> /CS2
    draw_bus(ax, [(0.32, 0.22), (0.38, 0.22)], label="A12-A15, /MREQ", color=BUS_ADDR, lw=1.8, label_pos=(0.35, 0.24))
    draw_bus(ax, [(0.62, 0.22), (0.68, 0.22)], label="/CS2 (Pin 6)", color=BUS_CS, lw=2.2, label_pos=(0.65, 0.24))

    # Truth Table at the bottom
    tt_box = FancyBboxPatch((0.06, 0.02), 0.88, 0.08,
                            boxstyle=BoxStyle("Round", pad=0.0, rounding_size=0.01),
                            facecolor="#0f172a", edgecolor="#334155", linewidth=1.0)
    ax.add_patch(tt_box)
    ax.text(0.50, 0.07, "8255 PPI REGISTER ADDRESS TRUTH TABLE", color=COLOR_PPI, fontsize=9, fontweight="bold", ha="center", va="center")
    ax.text(0.50, 0.04, "0x2000 = Port A (Keypad Columns)  •  0x2001 = Port B (7-Segment a-g, dp)  •  0x2002 = Port C (Digits, Rows, Audio)  •  0x2003 = Control Register",
            color=TEXT_WHITE, fontsize=8, ha="center", va="center")

    plt.tight_layout()
    plt.savefig(f"{OUTPUT_DIR}/abc80_cpu_8255.png", dpi=300, facecolor=BG_COLOR, edgecolor="none")
    plt.savefig(f"{OUTPUT_DIR}/abc80_cpu_8255.svg", facecolor=BG_COLOR, edgecolor="none")
    plt.close()
    print("Rendered CPU-8255 diagram (PNG & SVG).")


def render_8255_peripherals_diagram():
    """Generates the 8255 PPI to Front Panel & Peripherals Routing Diagram."""
    fig, ax = plt.subplots(figsize=(16, 10), dpi=300)
    fig.patch.set_facecolor(BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")

    ax.text(0.5, 0.965, "ABC-80 FRONT PANEL & PERIPHERAL ROUTING SCHEMATIC",
            color="#ffffff", fontsize=15, fontweight="bold", ha="center", va="center")
    ax.text(0.5, 0.94, "Intel 8255 PPI Port Assignments, Transistor Drivers, Multiplexed LED & Matrix Keypad",
            color=TEXT_MUTED, fontsize=9.5, ha="center", va="center")

    # 8255 Box (Left)
    draw_rounded_box(ax, 0.04, 0.12, 0.25, 0.78,
                     title="Intel 8255 PPI (IC6)", subtitle="Front-Panel Controller",
                     lines=[
                         ("Port B (Output):", "7-Segment Data Bus"),
                         ("PB0 (Pin 18)", "Segment a"),
                         ("PB1 (Pin 19)", "Segment b"),
                         ("PB2 (Pin 20)", "Segment c"),
                         ("PB3 (Pin 21)", "Segment d"),
                         ("PB4 (Pin 22)", "Segment e"),
                         ("PB5 (Pin 23)", "Segment f"),
                         ("PB6 (Pin 24)", "Segment g"),
                         ("PB7 (Pin 25)", "Decimal Point (dp)"),
                         ("-----------------", "-----------------"),
                         ("Port C (Output):", "Digit & Row Strobes"),
                         ("PC0–PC3 (Pins 14–17)", "Digits 1–4 & Key Rows 0–3"),
                         ("PC4–PC5 (Pins 13, 12)", "Digits 5–6 Cathodes"),
                         ("PC6 (Pin 11)", "Tone / Cassette Out"),
                         ("PC7 (Pin 10)", "Cassette Tape In"),
                         ("-----------------", "-----------------"),
                         ("Port A (Input):", "Keypad Column Sense"),
                         ("PA0–PA3 (Pins 4, 3, 2, 1)", "Columns 0–3 (4.7kΩ PU)")
                     ],
                     title_color=COLOR_PPI, body_fontsize=7.8)

    # Middle Intermediary Blocks
    # 8x Resistors
    draw_rounded_box(ax, 0.35, 0.72, 0.18, 0.18,
                     title="8x 220Ω Resistors", subtitle="Current Limiting Array",
                     lines=[
                         ("Inputs:", "PB0 – PB7 (From 8255)"),
                         ("Outputs:", "Segments a, b, c, d, e, f, g, dp"),
                         ("Current:", "~15 mA per active segment")
                     ],
                     title_color=COLOR_MISC, body_fontsize=8)

    # 14x PNP Drivers
    draw_rounded_box(ax, 0.35, 0.48, 0.18, 0.20,
                     title="14x PNP Drivers", subtitle="CS9012 / 2SA1015 Transistors",
                     lines=[
                         ("Digit Drivers:", "6x PNP Transistors"),
                         ("Cathode Strobes:", "Active-Low (PC0–PC5)"),
                         ("Keypad Isolation:", "Prevents ghosting"),
                         ("Drive Current:", ">150 mA peak per digit")
                     ],
                     title_color=COLOR_MISC, body_fontsize=8)

    # 4x Pull-ups
    draw_rounded_box(ax, 0.35, 0.28, 0.18, 0.16,
                     title="4x 4.7kΩ Pull-Ups", subtitle="Keypad Sense Array",
                     lines=[
                         ("VCC Rail:", "+5V Pull-up"),
                         ("Sensed by:", "Port A (PA0–PA3)"),
                         ("Key Pressed:", "Pulls PA pin Low (0)")
                     ],
                     title_color=COLOR_MISC, body_fontsize=8)

    # Audio Conditioning
    draw_rounded_box(ax, 0.35, 0.08, 0.18, 0.16,
                     title="Audio Circuitry", subtitle="Driver & Comparator",
                     lines=[
                         ("Output Driver:", "CS9013 NPN Transistor"),
                         ("Audio Input:", "LM324 / LM311 Comparator"),
                         ("Tape Filter:", "Bandpass + Schmitt Trigger")
                     ],
                     title_color=COLOR_MISC, body_fontsize=8)

    # Right Column Targets
    # 6-Digit Display
    draw_rounded_box(ax, 0.61, 0.65, 0.35, 0.25,
                     title="6-Digit Common-Cathode 7-Segment LED", subtitle="Front-Panel Numerical Display",
                     lines=[
                         ("Address Display (4 Digits):", "Digit 1 (D4), Digit 2 (D3), Digit 3 (D2), Digit 4 (D1)"),
                         ("Data Display (2 Digits):", "Digit 5 (D2), Digit 6 (D1)"),
                         ("Common Anodes:", "Tied across all 6 digits (Segments a-g, dp)"),
                         ("Common Cathodes:", "Individually strobed by PC0–PC5 PNP Drivers"),
                         ("Display Update:", "Interrupt-driven / software multiplexed in Monitor")
                     ],
                     title_color=COLOR_DISPLAY, body_fontsize=8)

    # 24-Key Matrix
    draw_rounded_box(ax, 0.61, 0.32, 0.35, 0.28,
                     title="24-Key Switch Matrix", subtitle="Hexadecimal Data & Control Keypad",
                     lines=[
                         ("Row 0 (PC0):", "[ 0 ]   [ 1 ]   [ 2 ]   [ 3 ]"),
                         ("Row 1 (PC1):", "[ 4 ]   [ 5 ]   [ 6 ]   [ 7 ]"),
                         ("Row 2 (PC2):", "[ 8 ]   [ 9 ]   [ A ]   [ B ]"),
                         ("Row 3 (PC3):", "[ C ]   [ D ]   [ E ]   [ F ]"),
                         ("Func 1 (PC0):", "[ ADRS ]   [ DATA ]   [  +  ]   [  -  ]"),
                         ("Func 2 (PC1):", "[  GO  ]   [ STEP ]   [ EXEC ]   [ RST ]"),
                         ("Columns:", "PA0 (Col 0), PA1 (Col 1), PA2 (Col 2), PA3 (Col 3)")
                     ],
                     title_color=COLOR_KEYPAD, body_fontsize=8)

    # Audio Jacks & Buzzer
    draw_rounded_box(ax, 0.61, 0.08, 0.35, 0.18,
                     title="Audio Jacks & Onboard Buzzer", subtitle="Cassette Storage & Acoustic Feedback",
                     lines=[
                         ("Piezo Speaker:", "Onboard tone generator for beep / alarms"),
                         ("MIC Jack (Audio Out):", "Tape recording audio output to cassette recorder"),
                         ("EAR Jack (Audio In):", "Tape playback audio input from cassette player"),
                         ("Protocol:", "FSK (1.0 kHz = '0', 2.0 kHz = '1')")
                     ],
                     title_color=COLOR_AUDIO, body_fontsize=8)

    # Bus Routing Arrows
    # PB0-PB7 -> Resistors -> Segments
    draw_bus(ax, [(0.29, 0.81), (0.35, 0.81)], label="PB0–PB7", color=COLOR_DISPLAY, lw=2.5, label_pos=(0.32, 0.825))
    draw_bus(ax, [(0.53, 0.81), (0.61, 0.81)], label="Segments a–g, dp", color=COLOR_DISPLAY, lw=2.5, label_pos=(0.57, 0.825))

    # PC0-PC5 -> PNP Drivers -> Cathodes
    draw_bus(ax, [(0.29, 0.58), (0.35, 0.58)], label="PC0–PC5", color=BUS_ADDR, lw=2.2, label_pos=(0.32, 0.595))
    draw_bus(ax, [(0.53, 0.60), (0.57, 0.60), (0.57, 0.70), (0.61, 0.70)], label="Digit Cathodes (1–6)", color=BUS_ADDR, lw=2.2, label_pos=(0.57, 0.65))

    # PC0-PC3 -> Keypad Rows
    draw_bus(ax, [(0.53, 0.52), (0.61, 0.52)], label="Row Strobes", color=COLOR_KEYPAD, lw=2.2, label_pos=(0.57, 0.535))

    # Keypad Cols -> Pull-ups -> PA0-PA3
    draw_bus(ax, [(0.61, 0.38), (0.53, 0.38)], label="Col Lines", color=COLOR_KEYPAD, lw=2.0, label_pos=(0.57, 0.395))
    draw_bus(ax, [(0.35, 0.36), (0.29, 0.36)], label="PA0–PA3", color=COLOR_KEYPAD, lw=2.2, label_pos=(0.32, 0.375))

    # PC6/PC7 -> Audio Circuitry -> Jacks
    draw_bus(ax, [(0.29, 0.16), (0.35, 0.16)], label="PC6 / PC7", color=COLOR_AUDIO, lw=2.0, label_pos=(0.32, 0.175))
    draw_bus(ax, [(0.53, 0.16), (0.61, 0.16)], label="Audio In/Out", color=COLOR_AUDIO, lw=2.0, label_pos=(0.57, 0.175))

    plt.tight_layout()
    plt.savefig(f"{OUTPUT_DIR}/abc80_8255_peripherals.png", dpi=300, facecolor=BG_COLOR, edgecolor="none")
    plt.savefig(f"{OUTPUT_DIR}/abc80_8255_peripherals.svg", facecolor=BG_COLOR, edgecolor="none")
    plt.close()
    print("Rendered 8255 peripherals diagram (PNG & SVG).")

if __name__ == "__main__":
    render_topology_diagram()
    render_cpu_8255_diagram()
    render_8255_peripherals_diagram()
    print("All professional diagrams rendered successfully!")
