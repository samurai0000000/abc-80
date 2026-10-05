#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
ABC-80 Hardware Schematic & Topology Diagram Generator
Generates publication-quality, high-resolution PNG & SVG diagrams for ABC-80 documentation.
"""

from pathlib import Path
import os
import subprocess

OUTPUT_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "doc/assets")
os.makedirs(OUTPUT_DIR, exist_ok=True)

def generate_topology_dot():
    dot_content = """
digraph ABC80_Topology {
    graph [
        rankdir=TB,
        nodesep=0.4,
        ranksep=0.6,
        bgcolor="#0f172a",
        fontname="DejaVu Sans, Arial, Helvetica",
        fontcolor="#f8fafc",
        pad="0.3",
        dpi=200
    ];
    
    node [
        shape=none,
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=10
    ];
    
    edge [
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=9,
        color="#94a3b8",
        fontcolor="#cbd5e1",
        penwidth=1.8
    ];

    // CPU & Core
    subgraph cluster_cpu {
        label=< <font color="#38bdf8" point-size="12"><b>CPU &amp; System Control Circuitry</b></font> >;
        style="filled,rounded";
        fillcolor="#1e293b";
        color="#475569";
        penwidth=1.5;

        cpu [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="6" bgcolor="#0369a1" color="#38bdf8">
                <tr><td bgcolor="#0284c7" align="center"><font color="#ffffff" point-size="12"><b>Zilog Z80A CPU (IC1)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    &bull; 2.5 MHz Clock (Pin 6)<br/>
                    &bull; 16-bit Address Bus (A0&ndash;A15)<br/>
                    &bull; 8-bit Data Bus (D0&ndash;D7)<br/>
                    &bull; Control: /MREQ, /RD, /WR, /INT, /HALT
                </font></td></tr>
            </table>
        >];

        clk [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="4" bgcolor="#1e293b" color="#475569">
                <tr><td bgcolor="#334155" align="center"><font color="#f8fafc"><b>2.5 MHz Clock Oscillator</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">2.5 MHz Crystal + 74LS04 Inverter</font></td></tr>
            </table>
        >];

        step_rst [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="4" bgcolor="#1e293b" color="#475569">
                <tr><td bgcolor="#334155" align="center"><font color="#f8fafc"><b>Reset &amp; Single-Step</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">Power-On RC / [RST] Key<br/>74LS74 /M1 &rarr; /INT (RST 38H)</font></td></tr>
            </table>
        >];

        clk -> cpu [xlabel=" CLK ", color="#38bdf8"];
        step_rst -> cpu [xlabel=" /RESET & /INT ", color="#f43f5e"];
    }

    // Decoders
    subgraph cluster_decoders {
        label=< <font color="#38bdf8" point-size="12"><b>Address Decoding Subsystem</b></font> >;
        style="filled,rounded";
        fillcolor="#1e293b";
        color="#475569";
        penwidth=1.5;

        decoder [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="6" bgcolor="#0f766e" color="#2dd4bf">
                <tr><td bgcolor="#0d9488" align="center"><font color="#ffffff" point-size="11"><b>74LS138 / 74LS139 Decoders (IC4/5)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    &bull; Inputs: A12&ndash;A15, /MREQ<br/>
                    &bull; /CS0 (0x0000&ndash;0x0FFF): 2732 Monitor ROM<br/>
                    &bull; /CS1 (0x1000&ndash;0x17FF): 6116 User RAM<br/>
                    &bull; /CS2 (0x2000&ndash;0x2FFF): 8255 PPI I/O<br/>
                    &bull; /CS3 (0x3000&ndash;0x3FFF): 2732 Burner Socket<br/>
                    &bull; /CS4&ndash;/CS7: Expansion Bus
                </font></td></tr>
            </table>
        >];
    }

    // Bus Devices
    subgraph cluster_memory {
        label=< <font color="#38bdf8" point-size="12"><b>Memory &amp; Peripheral Bus Devices</b></font> >;
        style="filled,rounded";
        fillcolor="#1e293b";
        color="#475569";
        penwidth=1.5;

        rom [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#4338ca" color="#818cf8">
                <tr><td bgcolor="#4f46e5" align="center"><font color="#ffffff"><b>2732 EPROM (IC2)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    4 KB Monitor ROM<br/>
                    0x0000 &ndash; 0x0FFF<br/>
                    /CE: /CS0, /OE: /RD
                </font></td></tr>
            </table>
        >];

        ram [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#4338ca" color="#818cf8">
                <tr><td bgcolor="#4f46e5" align="center"><font color="#ffffff"><b>6116 SRAM (IC3)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    2 KB User / System RAM<br/>
                    0x1000 &ndash; 0x17FF<br/>
                    /CE: /CS1, /OE: /RD, /WE: /WR
                </font></td></tr>
            </table>
        >];

        ppi [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#b45309" color="#fbbf24">
                <tr><td bgcolor="#d97706" align="center"><font color="#ffffff"><b>Intel 8255 PPI (IC6)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    Memory-Mapped I/O (0x2000)<br/>
                    /CS: /CS2, A0&ndash;A1, /RD, /WR<br/>
                    PA: Keypad Col In (4.7k&Omega; PU)<br/>
                    PB: 7-Seg Segment Out<br/>
                    PC: Digit/Row Strobe + Audio
                </font></td></tr>
            </table>
        >];

        burner [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#334155" color="#64748b">
                <tr><td bgcolor="#475569" align="center"><font color="#f8fafc"><b>2732 Burner Socket (IC7)</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">
                    EPROM Programmer<br/>
                    0x3000 &ndash; 0x3FFF<br/>
                    +21V/+25V Vpp Circuit
                </font></td></tr>
            </table>
        >];
    }

    // Front-Panel & Peripherals
    subgraph cluster_peripherals {
        label=< <font color="#38bdf8" point-size="12"><b>Front-Panel &amp; Audio Interfaces</b></font> >;
        style="filled,rounded";
        fillcolor="#1e293b";
        color="#475569";
        penwidth=1.5;

        display [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#b91c1c" color="#f87171">
                <tr><td bgcolor="#dc2626" align="center"><font color="#ffffff"><b>6-Digit 7-Segment LED Display</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    [ ADDR: 4 Digits ] [ DATA: 2 Digits ]<br/>
                    Segments a&ndash;g, dp via 8&times; 220&Omega; Resistors<br/>
                    Common Cathodes via 6&times; CS9012 PNP Drivers
                </font></td></tr>
            </table>
        >];

        keypad [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#047857" color="#34d399">
                <tr><td bgcolor="#059669" align="center"><font color="#ffffff"><b>24-Key Switch Matrix</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    16 Hex Keys: [ 0 &ndash; F ]<br/>
                    8 Function Keys: ADRS, DATA, +, &minus;, GO, STEP, EXEC, RST<br/>
                    Rows: PC0&ndash;PC3 | Columns: PA0&ndash;PA3
                </font></td></tr>
            </table>
        >];

        audio [label=<
            <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#6d28d9" color="#c084fc">
                <tr><td bgcolor="#7c3aed" align="center"><font color="#ffffff"><b>Cassette &amp; Audio Subsystem</b></font></td></tr>
                <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">
                    PC6 Out &rarr; CS9013 NPN &rarr; Speaker &amp; MIC OUT<br/>
                    PC7 In &larr; LM324 Comparator &larr; EAR IN<br/>
                    FSK Protocol: 1 kHz (&apos;0&apos;) / 2 kHz (&apos;1&apos;)
                </font></td></tr>
            </table>
        >];
    }

    // Bus connections
    cpu -> decoder [xlabel=" A12-A15, /MREQ ", color="#38bdf8", penwidth=2.5];
    
    decoder -> rom [xlabel=" /CS0 ", color="#a855f7", penwidth=2.0];
    decoder -> ram [xlabel=" /CS1 ", color="#a855f7", penwidth=2.0];
    decoder -> ppi [xlabel=" /CS2 ", color="#a855f7", penwidth=2.0];
    decoder -> burner [xlabel=" /CS3 ", color="#a855f7", penwidth=2.0];

    cpu -> rom [xlabel=" Address & Data Buses, /RD ", color="#38bdf8"];
    cpu -> ram [xlabel=" Address & Data Buses, /RD, /WR ", color="#38bdf8"];
    cpu -> ppi [xlabel=" A0-A1, D0-D7, /RD, /WR ", color="#38bdf8", penwidth=2.5];
    cpu -> burner [xlabel=" Bus & Controls ", color="#64748b"];

    ppi -> display [xlabel=" PB0-PB7 (Segs), PC0-PC5 (Digits) ", color="#f59e0b", penwidth=2.5];
    ppi -> keypad [xlabel=" PC0-PC3 (Strobes), PA0-PA3 (Sense) ", color="#10b981", penwidth=2.5];
    ppi -> audio [xlabel=" PC6 (Out), PC7 (In) ", color="#c084fc", penwidth=2.0];
}
"""
    dot_path = "/tmp/abc80_topology.dot"
    with open(dot_path, "w") as f:
        f.write(dot_content)
    
    subprocess.run(["dot", "-Tpng", "-Gdpi=200", dot_path, "-o", f"{OUTPUT_DIR}/abc80_topology.png"], check=True)
    subprocess.run(["dot", "-Tsvg", dot_path, "-o", f"{OUTPUT_DIR}/abc80_topology.svg"], check=True)
    print("Generated topology diagrams.")

def generate_cpu_8255_dot():
    dot_content = """
digraph ABC80_CPU_8255 {
    graph [
        rankdir=LR,
        nodesep=0.5,
        ranksep=0.9,
        bgcolor="#0f172a",
        fontname="DejaVu Sans, Arial, Helvetica",
        fontcolor="#f8fafc",
        pad="0.3",
        dpi=200
    ];
    
    node [
        shape=none,
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=10
    ];
    
    edge [
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=9,
        color="#94a3b8",
        fontcolor="#cbd5e1",
        penwidth=1.8
    ];

    cpu [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#0369a1" color="#38bdf8">
            <tr><td bgcolor="#0284c7" align="center" colspan="2"><font color="#ffffff" point-size="12"><b>Z80A CPU (IC1)</b></font></td></tr>
            <tr><td port="d0" bgcolor="#0f172a"><font color="#e2e8f0">Pin 14</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D0</b> (Data 0)</font></td></tr>
            <tr><td port="d1" bgcolor="#0f172a"><font color="#e2e8f0">Pin 15</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D1</b> (Data 1)</font></td></tr>
            <tr><td port="d2" bgcolor="#0f172a"><font color="#e2e8f0">Pin 12</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D2</b> (Data 2)</font></td></tr>
            <tr><td port="d3" bgcolor="#0f172a"><font color="#e2e8f0">Pin 8</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D3</b> (Data 3)</font></td></tr>
            <tr><td port="d4" bgcolor="#0f172a"><font color="#e2e8f0">Pin 7</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D4</b> (Data 4)</font></td></tr>
            <tr><td port="d5" bgcolor="#0f172a"><font color="#e2e8f0">Pin 9</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D5</b> (Data 5)</font></td></tr>
            <tr><td port="d6" bgcolor="#0f172a"><font color="#e2e8f0">Pin 10</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D6</b> (Data 6)</font></td></tr>
            <tr><td port="d7" bgcolor="#0f172a"><font color="#e2e8f0">Pin 13</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>D7</b> (Data 7)</font></td></tr>
            <tr><td port="a0" bgcolor="#0f172a"><font color="#e2e8f0">Pin 30</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>A0</b> (Addr 0)</font></td></tr>
            <tr><td port="a1" bgcolor="#0f172a"><font color="#e2e8f0">Pin 31</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>A1</b> (Addr 1)</font></td></tr>
            <tr><td port="rd" bgcolor="#0f172a"><font color="#e2e8f0">Pin 21</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>/RD</b> (Read)</font></td></tr>
            <tr><td port="wr" bgcolor="#0f172a"><font color="#e2e8f0">Pin 22</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>/WR</b> (Write)</font></td></tr>
            <tr><td port="rst" bgcolor="#0f172a"><font color="#e2e8f0">Pin 26</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>/RESET</b></font></td></tr>
            <tr><td port="dec" bgcolor="#0f172a"><font color="#e2e8f0">Pins 2&ndash;5, 19</font></td><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>A12&ndash;A15, /MREQ</b></font></td></tr>
        </table>
    >];

    decoder [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="6" bgcolor="#0f766e" color="#2dd4bf">
            <tr><td bgcolor="#0d9488" align="center"><font color="#ffffff"><b>74LS138 Decoder</b></font></td></tr>
            <tr><td port="in" bgcolor="#0f172a"><font color="#e2e8f0">Inputs: A12&ndash;A15, /MREQ</font></td></tr>
            <tr><td port="cs2" bgcolor="#134e4a"><font color="#5eead4"><b>/CS2 Output (0x2000)</b></font></td></tr>
        </table>
    >];

    inverter [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#334155" color="#64748b">
            <tr><td bgcolor="#475569" align="center"><font color="#f8fafc"><b>74LS04</b></font></td></tr>
            <tr><td port="inv" bgcolor="#0f172a"><font color="#cbd5e1">Active-Low &rarr; Active-High Inverter</font></td></tr>
        </table>
    >];

    ppi [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#b45309" color="#fbbf24">
            <tr><td bgcolor="#d97706" align="center" colspan="2"><font color="#ffffff" point-size="12"><b>Intel 8255 PPI (IC6)</b></font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D0</b> (Data 0)</font></td><td port="d0" bgcolor="#0f172a"><font color="#e2e8f0">Pin 34</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D1</b> (Data 1)</font></td><td port="d1" bgcolor="#0f172a"><font color="#e2e8f0">Pin 33</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D2</b> (Data 2)</font></td><td port="d2" bgcolor="#0f172a"><font color="#e2e8f0">Pin 32</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D3</b> (Data 3)</font></td><td port="d3" bgcolor="#0f172a"><font color="#e2e8f0">Pin 31</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D4</b> (Data 4)</font></td><td port="d4" bgcolor="#0f172a"><font color="#e2e8f0">Pin 30</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D5</b> (Data 5)</font></td><td port="d5" bgcolor="#0f172a"><font color="#e2e8f0">Pin 29</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D6</b> (Data 6)</font></td><td port="d6" bgcolor="#0f172a"><font color="#e2e8f0">Pin 28</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>D7</b> (Data 7)</font></td><td port="d7" bgcolor="#0f172a"><font color="#e2e8f0">Pin 27</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>A0</b> (Port Sel 0)</font></td><td port="a0" bgcolor="#0f172a"><font color="#e2e8f0">Pin 9</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>A1</b> (Port Sel 1)</font></td><td port="a1" bgcolor="#0f172a"><font color="#e2e8f0">Pin 8</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>/RD</b> (Read Strobe)</font></td><td port="rd" bgcolor="#0f172a"><font color="#e2e8f0">Pin 5</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>/WR</b> (Write Strobe)</font></td><td port="wr" bgcolor="#0f172a"><font color="#e2e8f0">Pin 36</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>RESET</b> (Active-H)</font></td><td port="rst" bgcolor="#0f172a"><font color="#e2e8f0">Pin 35</font></td></tr>
            <tr><td align="right" bgcolor="#0f172a"><font color="#e2e8f0"><b>/CS</b> (Chip Select)</font></td><td port="cs" bgcolor="#0f172a"><font color="#e2e8f0">Pin 6</font></td></tr>
        </table>
    >];

    cpu:d0 -> ppi:d0 [color="#38bdf8", penwidth=2.0];
    cpu:d1 -> ppi:d1 [color="#38bdf8", penwidth=2.0];
    cpu:d2 -> ppi:d2 [color="#38bdf8", penwidth=2.0];
    cpu:d3 -> ppi:d3 [color="#38bdf8", penwidth=2.0];
    cpu:d4 -> ppi:d4 [color="#38bdf8", penwidth=2.0];
    cpu:d5 -> ppi:d5 [color="#38bdf8", penwidth=2.0];
    cpu:d6 -> ppi:d6 [color="#38bdf8", penwidth=2.0];
    cpu:d7 -> ppi:d7 [color="#38bdf8", penwidth=2.0, xlabel=" 8-Bit Data Bus (D0-D7) "];

    cpu:a0 -> ppi:a0 [color="#a855f7", xlabel=" Port Select A0 "];
    cpu:a1 -> ppi:a1 [color="#a855f7", xlabel=" Port Select A1 "];

    cpu:rd -> ppi:rd [color="#10b981", xlabel=" Read Strobe (/RD) "];
    cpu:wr -> ppi:wr [color="#f59e0b", xlabel=" Write Strobe (/WR) "];

    cpu:rst -> inverter:inv [color="#f43f5e"];
    inverter:inv -> ppi:rst [color="#f43f5e", xlabel=" Active-High RESET "];

    cpu:dec -> decoder:in [color="#38bdf8"];
    decoder:cs2 -> ppi:cs [color="#e11d48", penwidth=2.5, xlabel=" 0x2000-0x2FFF Enable (/CS2) "];
}
"""
    dot_path = "/tmp/abc80_cpu_8255.dot"
    with open(dot_path, "w") as f:
        f.write(dot_content)
    
    subprocess.run(["dot", "-Tpng", "-Gdpi=200", dot_path, "-o", f"{OUTPUT_DIR}/abc80_cpu_8255.png"], check=True)
    subprocess.run(["dot", "-Tsvg", dot_path, "-o", f"{OUTPUT_DIR}/abc80_cpu_8255.svg"], check=True)
    print("Generated CPU-8255 diagrams.")

def generate_8255_peripherals_dot():
    dot_content = """
digraph ABC80_8255_Peripherals {
    graph [
        rankdir=LR,
        nodesep=0.5,
        ranksep=0.8,
        bgcolor="#0f172a",
        fontname="DejaVu Sans, Arial, Helvetica",
        fontcolor="#f8fafc",
        pad="0.3",
        dpi=200
    ];
    
    node [
        shape=none,
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=10
    ];
    
    edge [
        fontname="DejaVu Sans, Arial, Helvetica",
        fontsize=9,
        color="#94a3b8",
        fontcolor="#cbd5e1",
        penwidth=1.8
    ];

    ppi [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="6" bgcolor="#b45309" color="#fbbf24">
            <tr><td bgcolor="#d97706" align="center" colspan="2"><font color="#ffffff" point-size="12"><b>Intel 8255 PPI (IC6)</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>Port B (PB0&ndash;PB7)</b><br/>Pins 18&ndash;25 (Output)</font></td><td port="pb" bgcolor="#1e293b"><font color="#fbbf24">7-Seg Segments</font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>Port C (PC0&ndash;PC5)</b><br/>Pins 14&ndash;17, 13, 12 (Output)</font></td><td port="pc_dig" bgcolor="#1e293b"><font color="#38bdf8">Digit Strobes</font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>Port C (PC0&ndash;PC3)</b><br/>Pins 14&ndash;17 (Output)</font></td><td port="pc_row" bgcolor="#1e293b"><font color="#34d399">Keypad Rows</font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>Port A (PA0&ndash;PA3)</b><br/>Pins 4, 3, 2, 1 (Input)</font></td><td port="pa_col" bgcolor="#1e293b"><font color="#34d399">Keypad Columns</font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0"><b>Port C (PC6&ndash;PC7)</b><br/>Pins 11, 10 (Audio I/O)</font></td><td port="pc_aud" bgcolor="#1e293b"><font color="#c084fc">Cassette &amp; Tone</font></td></tr>
        </table>
    >];

    resistors [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#334155" color="#64748b">
            <tr><td bgcolor="#475569" align="center"><font color="#f8fafc"><b>8&times; 220&Omega; Resistors</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">Current Limiting for Segments a&ndash;g, dp</font></td></tr>
        </table>
    >];

    display [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="6" bgcolor="#b91c1c" color="#f87171">
            <tr><td bgcolor="#dc2626" align="center" colspan="6"><font color="#ffffff" point-size="11"><b>6-Digit Common-Cathode 7-Segment LED</b></font></td></tr>
            <tr>
                <td bgcolor="#0f172a"><font color="#fca5a5"><b>Digit 1</b><br/>Addr D4</font></td>
                <td bgcolor="#0f172a"><font color="#fca5a5"><b>Digit 2</b><br/>Addr D3</font></td>
                <td bgcolor="#0f172a"><font color="#fca5a5"><b>Digit 3</b><br/>Addr D2</font></td>
                <td bgcolor="#0f172a"><font color="#fca5a5"><b>Digit 4</b><br/>Addr D1</font></td>
                <td bgcolor="#0f172a"><font color="#93c5fd"><b>Digit 5</b><br/>Data D2</font></td>
                <td bgcolor="#0f172a"><font color="#93c5fd"><b>Digit 6</b><br/>Data D1</font></td>
            </tr>
            <tr><td bgcolor="#1e293b" colspan="6" align="center"><font color="#cbd5e1">Shared Segment Anodes: a, b, c, d, e, f, g, dp</font></td></tr>
        </table>
    >];

    drivers [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#334155" color="#64748b">
            <tr><td bgcolor="#475569" align="center"><font color="#f8fafc"><b>14&times; PNP Drivers (CS9012)</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">Active-Low Common-Cathode Multiplexers</font></td></tr>
        </table>
    >];

    keypad [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#047857" color="#34d399">
            <tr><td bgcolor="#059669" align="center" colspan="4"><font color="#ffffff" point-size="11"><b>24-Key Switch Matrix</b></font></td></tr>
            <tr>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 0 ]</b> (Row 0)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 1 ]</b> (Row 0)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 2 ]</b> (Row 0)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 3 ]</b> (Row 0)</font></td>
            </tr>
            <tr>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 4 ]</b> (Row 1)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 5 ]</b> (Row 1)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 6 ]</b> (Row 1)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 7 ]</b> (Row 1)</font></td>
            </tr>
            <tr>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 8 ]</b> (Row 2)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ 9 ]</b> (Row 2)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ A ]</b> (Row 2)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ B ]</b> (Row 2)</font></td>
            </tr>
            <tr>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ C ]</b> (Row 3)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ D ]</b> (Row 3)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ E ]</b> (Row 3)</font></td>
                <td bgcolor="#0f172a"><font color="#a7f3d0"><b>[ F ]</b> (Row 3)</font></td>
            </tr>
            <tr>
                <td bgcolor="#1e293b"><font color="#fde047"><b>[ADRS]</b></font></td>
                <td bgcolor="#1e293b"><font color="#fde047"><b>[DATA]</b></font></td>
                <td bgcolor="#1e293b"><font color="#fde047"><b>[ + ]</b></font></td>
                <td bgcolor="#1e293b"><font color="#fde047"><b>[ &minus; ]</b></font></td>
            </tr>
            <tr>
                <td bgcolor="#1e293b"><font color="#fca5a5"><b>[ GO ]</b></font></td>
                <td bgcolor="#1e293b"><font color="#fca5a5"><b>[STEP]</b></font></td>
                <td bgcolor="#1e293b"><font color="#fca5a5"><b>[EXEC]</b></font></td>
                <td bgcolor="#1e293b"><font color="#f87171"><b>[RST]</b></font></td>
            </tr>
        </table>
    >];

    pullups [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#334155" color="#64748b">
            <tr><td bgcolor="#475569" align="center"><font color="#f8fafc"><b>4&times; 4.7k&Omega; Pull-Up Resistors</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#cbd5e1">Pulls PA0&ndash;PA3 to +5V (Active-Low Sense)</font></td></tr>
        </table>
    >];

    audio_out [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#6d28d9" color="#c084fc">
            <tr><td bgcolor="#7c3aed" align="center"><font color="#ffffff"><b>Tone Generator &amp; MIC Out</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">PC6 &rarr; CS9013 NPN &rarr; Buzzer &amp; MIC Jack</font></td></tr>
        </table>
    >];

    audio_in [label=<
        <table border="0" cellborder="1" cellspacing="0" cellpadding="5" bgcolor="#6d28d9" color="#c084fc">
            <tr><td bgcolor="#7c3aed" align="center"><font color="#ffffff"><b>Cassette EAR Input</b></font></td></tr>
            <tr><td align="left" bgcolor="#0f172a"><font color="#e2e8f0">EAR Jack &rarr; LM324 Comparator &rarr; PC7 In</font></td></tr>
        </table>
    >];

    // PB Routing
    ppi:pb -> resistors [color="#f59e0b", xlabel=" PB0-PB7 ", penwidth=2.5];
    resistors -> display [color="#f59e0b", xlabel=" Segments a-g, dp ", penwidth=2.5];

    // PC Digit Routing
    ppi:pc_dig -> drivers [color="#38bdf8", xlabel=" PC0-PC5 ", penwidth=2.5];
    drivers -> display [color="#38bdf8", xlabel=" Digit 1-6 Cathodes ", penwidth=2.5];

    // Keypad Routing
    ppi:pc_row -> keypad [color="#10b981", xlabel=" Row Strobes PC0-PC3 ", penwidth=2.5];
    keypad -> pullups [color="#10b981", xlabel=" Column Returns "];
    pullups -> ppi:pa_col [color="#10b981", xlabel=" PA0-PA3 ", penwidth=2.5];

    // Audio Routing
    ppi:pc_aud -> audio_out [color="#c084fc", xlabel=" PC6 Tone Output ", penwidth=2.0];
    audio_in -> ppi:pc_aud [color="#c084fc", xlabel=" PC7 Cassette In ", penwidth=2.0];
}
"""
    dot_path = "/tmp/abc80_8255_peripherals.dot"
    with open(dot_path, "w") as f:
        f.write(dot_content)
    
    subprocess.run(["dot", "-Tpng", "-Gdpi=200", dot_path, "-o", f"{OUTPUT_DIR}/abc80_8255_peripherals.png"], check=True)
    subprocess.run(["dot", "-Tsvg", dot_path, "-o", f"{OUTPUT_DIR}/abc80_8255_peripherals.svg"], check=True)
    print("Generated 8255 peripherals diagrams.")

if __name__ == "__main__":
    generate_topology_dot()
    generate_cpu_8255_dot()
    generate_8255_peripherals_dot()
    print("All diagrams successfully generated in docs/assets/!")
