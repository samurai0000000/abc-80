#
# test_ext_io.py
#
# End-to-end Playwright browser tests for the Floating External I/O Daughterboard
# Window (#ext-io-window), header EXT I/O selector (#ext-io-select), show/hide
# toggle (#btn-toggle-ext-io), strict mutual exclusivity across all 6 circuits
# (Exercises 24-29), hybrid tape 0024-0029 auto-attach, and live telemetry.
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for

ALL_CIRCUITS = [
    ("traffic_light", 24),
    ("led_chaser", 25),
    ("oscilloscope", 26),
    ("school_bell", 27),
    ("appliance_control", 28),
    ("robotic_arm", 29),
]


def test_ext_io_header_selector_and_mutual_exclusivity(panel):
    panel.open()
    page = panel.page

    # Initial power-on state: no circuit attached, window hidden, toggle button disabled
    t0 = panel.wait_telemetry(lambda t: "extCircuit" in t, what="initial extCircuit telemetry")
    assert t0["extCircuit"]["id"] == "none"
    assert t0["extCircuit"]["attached"] is False
    assert page.locator("#ext-io-select").input_value() == "none"
    assert page.locator("#btn-toggle-ext-io").is_disabled()
    assert page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")

    # Cycle through all 6 circuits via #ext-io-select and verify strict mutual exclusivity
    for cid, ex_num in ALL_CIRCUITS:
        page.select_option("#ext-io-select", cid)
        t = panel.wait_telemetry(
            lambda t, expected=cid: t.get("extCircuit", {}).get("id") == expected
            and t.get("extCircuit", {}).get("attached") is True,
            what=f"server attach {cid}",
        )
        assert t["extCircuit"]["exercise"] == ex_num
        assert not page.locator("#btn-toggle-ext-io").is_disabled()
        assert page.locator("#btn-toggle-ext-io").evaluate(
            "el => el.classList.contains('header-btn-active')"
        )
        assert not page.locator("#ext-io-window").evaluate(
            "el => el.classList.contains('hidden')"
        )

        # Verify only the selected stage is active (display: flex) and the other 5 are hidden
        for other_id, _ in ALL_CIRCUITS:
            stage = page.locator(f"#ext-stage-{other_id}")
            is_active = stage.evaluate("el => el.classList.contains('active')")
            disp = stage.evaluate("el => getComputedStyle(el).display")
            if other_id == cid:
                assert is_active is True, f"expected #ext-stage-{cid} to have .active"
                assert disp == "flex", f"expected #ext-stage-{cid} display:flex, got {disp}"
            else:
                assert is_active is False, f"expected #ext-stage-{other_id} not to have .active"
                assert disp == "none", f"expected #ext-stage-{other_id} display:none, got {disp}"

    # Selecting "none" detaches the circuit and hides the window
    page.select_option("#ext-io-select", "none")
    t_none = panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "none"
        and t.get("extCircuit", {}).get("attached") is False,
        what="detach via select none",
    )
    assert t_none["extCircuit"]["attached"] is False
    assert page.locator("#btn-toggle-ext-io").is_disabled()
    assert page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")


def test_ext_io_show_hide_toggle_vs_detach_and_dragging(panel):
    panel.open()
    page = panel.page

    page.select_option("#ext-io-select", "traffic_light")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "traffic_light",
        what="attach traffic_light",
    )
    assert page.locator("#ext-io-window").is_visible()

    # Drag the window via #ext-io-drag-handle and verify coordinates update
    box_before = page.locator("#ext-io-window").bounding_box()
    handle_box = page.locator("#ext-io-drag-handle").bounding_box()
    cx = handle_box["x"] + handle_box["width"] / 2
    cy = handle_box["y"] + handle_box["height"] / 2
    page.mouse.move(cx, cy)
    page.mouse.down()
    page.mouse.move(cx + 90, cy + 55)
    page.mouse.up()
    box_after = page.locator("#ext-io-window").bounding_box()
    assert abs(box_after["x"] - (box_before["x"] + 90)) < 15
    assert abs(box_after["y"] - (box_before["y"] + 55)) < 15

    # Clicking [X] (#btn-ext-io-close) hides the window WITHOUT detaching the circuit
    page.click("#btn-ext-io-close")
    assert page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")
    assert not page.locator("#btn-toggle-ext-io").is_disabled()
    assert not page.locator("#btn-toggle-ext-io").evaluate(
        "el => el.classList.contains('header-btn-active')"
    )
    t_hidden = panel.telemetry()
    assert t_hidden["extCircuit"]["id"] == "traffic_light"
    assert t_hidden["extCircuit"]["attached"] is True
    assert page.locator("#ext-io-select").input_value() == "traffic_light"

    # Clicking header #btn-toggle-ext-io re-opens the window while keeping it attached
    page.click("#btn-toggle-ext-io")
    assert not page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")
    assert page.locator("#btn-toggle-ext-io").evaluate(
        "el => el.classList.contains('header-btn-active')"
    )

    # Clicking #btn-ext-io-detach in the apron detaches the circuit completely
    page.click("#btn-ext-io-detach")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "none"
        and t.get("extCircuit", {}).get("attached") is False,
        what="detach via apron button",
    )
    assert page.locator("#ext-io-select").input_value() == "none"
    assert page.locator("#btn-toggle-ext-io").is_disabled()
    assert page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")


def test_ext_io_tape_select_does_not_auto_attach(panel):
    panel.open()
    page = panel.page

    # Open the Cassette Tape tab in the inspector
    page.click('.tab-btn[data-tab="tab-tape"]')

    # Selecting tapes 0001 and 0024..0029 must NEVER auto-attach an external circuit or pop open #ext-io-window
    for fid in ["0001", "0024", "0025", "0026", "0027", "0028", "0029"]:
        page.fill("#tape-file-id-input", fid)
        page.click("#btn-tape-select")
        panel.wait_telemetry(
            lambda t, expected=fid: t.get("tape", {}).get("fileId") == expected,
            what=f"tape {fid} selected without auto-attach",
        )
        assert panel.telemetry()["extCircuit"]["id"] == "none"
        assert panel.telemetry()["extCircuit"]["attached"] is False
        assert page.locator("#ext-io-select").input_value() == "none"
        assert page.locator("#btn-toggle-ext-io").is_disabled()
        assert page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")

    # External circuit attaches and opens #ext-io-window only when the user explicitly chooses it in #ext-io-select
    page.select_option("#ext-io-select", "traffic_light")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "traffic_light"
        and t.get("extCircuit", {}).get("attached") is True,
        what="manual attach traffic_light via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "traffic_light"
    assert not page.locator("#ext-io-window").evaluate("el => el.classList.contains('hidden')")
    assert page.locator("#ext-stage-traffic_light").evaluate("el => el.classList.contains('active')")


def test_ext_io_live_telemetry_and_clock_presets(panel):
    panel.open()
    page = panel.page

    # 1. Attach School Bell (Ex.27) and test BCD clock preset button (#btn-bell-preset-0800)
    page.select_option("#ext-io-select", "school_bell")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "school_bell",
        what="attach school_bell",
    )
    page.click("#btn-bell-preset-0800")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("bcdHour") == 0x07
        and t.get("extCircuit", {}).get("bcdMinute") == 0x59
        and t.get("extCircuit", {}).get("bcdSecond") == 0x58,
        what="set_clock 07:59:58 via preset button",
    )
    wait_for(
        lambda: page.locator("#bell-bcd-time").inner_text() == "07:59:58",
        what="bell BCD time readout 07:59:58",
    )

    # 2. Verify live UI rendering for custom/synthetic telemetry frames across all 6 stages
    # Traffic Light (Ex.24): Road A Green (0x04) + Road B Red (0x08) = 0x0C
    page.evaluate(
        """() => {
            window.__abc80.applyTelemetry({
                ppi: { pa: 0, pb: 0x0C, pc: 0, ctrl: 0x80 },
                extCircuit: {
                    id: 'traffic_light',
                    attached: true,
                    exercise: 24,
                    clockTStates: 123456,
                    portB: 0x0C,
                    roadA: { red: false, yellow: false, green: true, duty: [0, 0, 1] },
                    roadB: { red: true, yellow: false, green: false, duty: [1, 0, 0] },
                    transitions: 4
                }
            });
        }"""
    )
    assert page.locator("#tl-led-a-green").evaluate("el => el.classList.contains('active')")
    assert page.locator("#tl-led-b-red").evaluate("el => el.classList.contains('active')")
    assert page.locator("#tl-portb").inner_text() == "0CH"
    assert page.locator("#tl-phase-summary").inner_text() == "A:GREEN / B:RED"
    assert page.locator("#tl-transitions").inner_text() == "4"

    # LED Chaser (Ex.25): rawPortB = 0xFE -> activeMask = 0x01 (LED0 lit)
    page.evaluate(
        """() => {
            window.__abc80.applyTelemetry({
                ppi: { pa: 0, pb: 0xFE, pc: 0, ctrl: 0x80 },
                extCircuit: {
                    id: 'led_chaser',
                    attached: true,
                    exercise: 25,
                    clockTStates: 234567,
                    rawPortB: 0xFE,
                    activeMask: 0x01,
                    activeLed: 0,
                    leds: [true, false, false, false, false, false, false, false],
                    duty: [1, 0, 0, 0, 0, 0, 0, 0],
                    steps: 9
                }
            });
        }"""
    )
    assert page.locator("#chaser-led-0").evaluate("el => el.classList.contains('active')")
    assert not page.locator("#chaser-led-7").evaluate("el => el.classList.contains('active')")
    assert page.locator("#chaser-raw-pb").inner_text() == "FEH"
    assert page.locator("#chaser-active-mask").inner_text() == "01H"
    assert page.locator("#chaser-active-led").inner_text() == "LED0"

    # Appliance Control (Ex.28): relayMask = 0x25 (bits 0, 2, 5 energized)
    page.evaluate(
        """() => {
            window.__abc80.applyTelemetry({
                ppi: { pa: 0, pb: 0, pc: 0, ctrl: 0x80, auxPa: 0, auxPb: 0x25, auxPc: 0, auxCtrl: 0x80 },
                extCircuit: {
                    id: 'appliance_control',
                    attached: true,
                    exercise: 28,
                    clockTStates: 345678,
                    auxControl: 0x80,
                    relayMask: 0x25,
                    relays: [true, false, true, false, false, true, false, false],
                    duty: [1, 0, 1, 0, 0, 1, 0, 0],
                    updates: 3,
                    bcdHour: 0x19,
                    bcdMinute: 0x00,
                    bcdSecond: 0x00
                }
            });
        }"""
    )
    assert page.locator("#appliance-relay-0").evaluate("el => el.classList.contains('energized')")
    assert page.locator("#appliance-relay-2").evaluate("el => el.classList.contains('energized')")
    assert page.locator("#appliance-relay-5").evaluate("el => el.classList.contains('energized')")
    assert not page.locator("#appliance-relay-1").evaluate("el => el.classList.contains('energized')")
    assert page.locator("#appliance-relay-mask").inner_text() == "25H"
    assert page.locator("#appliance-bcd-time").inner_text() == "19:00:00"

    # Robotic Arm (Ex.29): 3 motors + electromagnet energized
    page.evaluate(
        """() => {
            window.__abc80.applyTelemetry({
                ppi: { pa: 0, pb: 0, pc: 0, ctrl: 0x80, expPa: 0x06, expPb: 0x0C, expPc: 0x83, expCtrl: 0x80 },
                extCircuit: {
                    id: 'robotic_arm',
                    attached: true,
                    exercise: 29,
                    clockTStates: 456789,
                    expControl: 0x80,
                    portA: 0x06,
                    portB: 0x0C,
                    portC: 0x83,
                    electromagnet: true,
                    motors: [
                        { phase: 1, coils: 0x06, steps: 5, totalSteps: 5 },
                        { phase: 2, coils: 0x0C, steps: -3, totalSteps: 3 },
                        { phase: 0, coils: 0x03, steps: 2, totalSteps: 2 }
                    ]
                }
            });
        }"""
    )
    assert page.locator("#arm-motor-0-steps").inner_text() == "+5"
    assert page.locator("#arm-motor-1-steps").inner_text() == "-3"
    assert page.locator("#arm-magnet-badge").inner_text() == "ON (ENERGIZED)"


def test_exercise24_traffic_light_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..101EH that configures Primary 8255 Mode 0 (90H -> 83H)
    # and cycles Port B (81H) through the 4 Exercise 24 traffic light phases: 0CH, 0AH, 21H, 11H.
    program = [
        0x3E, 0x90,             # 1000: LD A, 90H
        0xD3, 0x83,             # 1002: OUT (83H), A
        0x21, 0x1B, 0x10,       # 1004: LD HL, 101BH
        0x0E, 0x04,             # 1007: LD C, 04H
        0x7E,                   # 1009: LD A, (HL)
        0xD3, 0x81,             # 100A: OUT (81H), A
        0x23,                   # 100C: INC HL
        0x16, 0x90,             # 100D: LD D, 90H (~0.27s deterministic hold per phase)
        0x06, 0x00,             # 100F: LD B, 00H (256 iterations per outer D tick)
        0x10, 0xFE,             # 1011: DJNZ 1011H
        0x15,                   # 1013: DEC D
        0x20, 0xF9,             # 1014: JR NZ, 100FH
        0x0D,                   # 1016: DEC C
        0x20, 0xF0,             # 1017: JR NZ, 1009H
        0x18, 0xE9,             # 1019: JR 1004H
        0x0C, 0x0A, 0x21, 0x11  # 101B: Phase table: A-Grn/B-Red, A-Yel/B-Red, A-Red/B-Grn, A-Red/B-Yel
    ]
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=20)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=20)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the traffic light circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach traffic_light manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "traffic_light")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "traffic_light"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 24,
        what="attach traffic_light via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "traffic_light"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-traffic_light")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s FR-4 PCB components, CD4050 buffer IC, 330-ohm resistors, PB0..PB5 silkscreen, and 6 LEDs
    stage_text = stage.inner_text()
    assert "CD4050" in stage_text
    assert "HEX BUFFER" in stage_text
    assert stage.locator(".ext-axial-res").count() == 6
    for bit in range(6):
        assert f"330Ω (PB{bit})" in stage_text
    assert "ROAD A (N–S)" in stage_text
    assert "ROAD B (E–W)" in stage_text

    all_leds = [
        "#tl-led-a-red",
        "#tl-led-a-yellow",
        "#tl-led-a-green",
        "#tl-led-b-red",
        "#tl-led-b-yellow",
        "#tl-led-b-green",
    ]
    for led_sel in all_leds:
        assert page.locator(led_sel).is_visible(), f"expected {led_sel} to be visible"

    # Start Z80 execution at 1000H and verify live DOM updates across 0x0C, 0x0A, 0x21, and 0x11
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    def dom_matches_phase(expected_portb, expected_phase, lit_selectors):
        return page.evaluate(
            """([expPb, expPhase, allSels, litSels]) => {
                const pbEl = document.getElementById('tl-portb');
                const phEl = document.getElementById('tl-phase-summary');
                if (!pbEl || !phEl) return false;
                if (pbEl.textContent !== expPb) return false;
                if (phEl.textContent !== expPhase) return false;
                const litSet = new Set(litSels);
                for (const sel of allSels) {
                    const el = document.querySelector(sel);
                    if (!el) return false;
                    const isLit = el.classList.contains('lit');
                    if (litSet.has(sel) && !isLit) return false;
                    if (!litSet.has(sel) && isLit) return false;
                }
                return true;
            }""",
            [expected_portb, expected_phase, all_leds, list(lit_selectors)],
        )

    wait_for(
        lambda: dom_matches_phase("0CH", "A:GREEN / B:RED", {"#tl-led-a-green", "#tl-led-b-red"}),
        timeout=10.0,
        poll=0.02,
        what="Phase 0CH (Road A Green, Road B Red) in live DOM",
    )
    wait_for(
        lambda: dom_matches_phase("0AH", "A:YELLOW / B:RED", {"#tl-led-a-yellow", "#tl-led-b-red"}),
        timeout=10.0,
        poll=0.02,
        what="Phase 0AH (Road A Yellow, Road B Red) in live DOM",
    )
    wait_for(
        lambda: dom_matches_phase("21H", "A:RED / B:GREEN", {"#tl-led-a-red", "#tl-led-b-green"}),
        timeout=10.0,
        poll=0.02,
        what="Phase 21H (Road A Red, Road B Green) in live DOM",
    )
    wait_for(
        lambda: dom_matches_phase("11H", "A:RED / B:YELLOW", {"#tl-led-a-red", "#tl-led-b-yellow"}),
        timeout=10.0,
        poll=0.02,
        what="Phase 11H (Road A Red, Road B Yellow) in live DOM",
    )

    wait_for(
        lambda: int(page.locator("#tl-transitions").inner_text()) >= 4,
        timeout=5.0,
        what="#tl-transitions >= 4 in live DOM",
    )


def test_exercise25_led_chaser_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..101EH that configures Primary 8255 Mode 0 (90H -> 83H)
    # and cycles Port B (81H) through active-low Exercise 25 chaser patterns: FEH, FDH, BFH, 7FH.
    program = [
        0x3E, 0x90,             # 1000: LD A, 90H
        0xD3, 0x83,             # 1002: OUT (83H), A
        0x21, 0x1B, 0x10,       # 1004: LD HL, 101BH
        0x0E, 0x04,             # 1007: LD C, 04H
        0x7E,                   # 1009: LD A, (HL)
        0xD3, 0x81,             # 100A: OUT (81H), A
        0x23,                   # 100C: INC HL
        0x16, 0x90,             # 100D: LD D, 90H (~0.27s deterministic hold per chaser step)
        0x06, 0x00,             # 100F: LD B, 00H (256 iterations per outer D tick)
        0x10, 0xFE,             # 1011: DJNZ 1011H
        0x15,                   # 1013: DEC D
        0x20, 0xF9,             # 1014: JR NZ, 100FH
        0x0D,                   # 1016: DEC C
        0x20, 0xF0,             # 1017: JR NZ, 1009H
        0x18, 0xE9,             # 1019: JR 1004H
        0xFE, 0xFD, 0xBF, 0x7F  # 101B: Active-low chaser values for LED0, LED1, LED6, LED7
    ]
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=20)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=20)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the LED chaser circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach led_chaser manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "led_chaser")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "led_chaser"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 25,
        what="attach led_chaser via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "led_chaser"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-led_chaser")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s FR-4 PCB components, CD4009/SN7404 inverter IC, 8 axial resistors R1..R8, copper traces, and 8 LEDs
    stage_text = stage.inner_text()
    assert "CD4009" in stage_text
    assert "SN7404" in stage_text
    assert "HEX INVERTER BUFFER" in stage_text
    assert "COPPER BUS" in stage_text
    assert stage.locator(".ext-axial-res").count() == 8
    for r_idx in range(1, 9):
        assert f"R{r_idx} 330Ω" in stage_text

    all_leds = [f"#chaser-led-{i}" for i in range(8)]
    for led_sel in all_leds:
        assert page.locator(led_sel).is_visible(), f"expected {led_sel} to be visible"

    # Start Z80 execution at 1000H and verify live DOM updates across 0xFE, 0xFD, 0xBF, and 0x7F
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    def dom_matches_chaser(expected_raw_pb, expected_mask, lit_index):
        return page.evaluate(
            """([expRaw, expMask, litIdx]) => {
                const rawEl = document.getElementById('chaser-raw-pb');
                const maskEl = document.getElementById('chaser-active-mask');
                const actEl = document.getElementById('chaser-active-led');
                if (!rawEl || !maskEl || !actEl) return false;
                if (rawEl.textContent !== expRaw) return false;
                if (maskEl.textContent !== expMask) return false;
                if (actEl.textContent !== 'LED' + litIdx) return false;
                for (let i = 0; i < 8; ++i) {
                    const el = document.getElementById('chaser-led-' + i);
                    if (!el) return false;
                    const isLit = el.classList.contains('lit');
                    if (i === litIdx && !isLit) return false;
                    if (i !== litIdx && isLit) return false;
                }
                return true;
            }""",
            [expected_raw_pb, expected_mask, lit_index],
        )

    wait_for(
        lambda: dom_matches_chaser("FEH", "01H", 0),
        timeout=10.0,
        poll=0.02,
        what="Chaser FEH (~PB=01H, #chaser-led-0 .lit) in live DOM",
    )
    wait_for(
        lambda: dom_matches_chaser("FDH", "02H", 1),
        timeout=10.0,
        poll=0.02,
        what="Chaser FDH (~PB=02H, #chaser-led-1 .lit) in live DOM",
    )
    wait_for(
        lambda: dom_matches_chaser("BFH", "40H", 6),
        timeout=10.0,
        poll=0.02,
        what="Chaser BFH (~PB=40H, #chaser-led-6 .lit) in live DOM",
    )
    wait_for(
        lambda: dom_matches_chaser("7FH", "80H", 7),
        timeout=10.0,
        poll=0.02,
        what="Chaser 7FH (~PB=80H, #chaser-led-7 .lit) in live DOM",
    )

    wait_for(
        lambda: int(page.locator("#chaser-steps").inner_text()) >= 4,
        timeout=5.0,
        what="#chaser-steps >= 4 in live DOM",
    )


def test_exercise26_oscilloscope_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..101DH that configures Primary 8255 Mode 0 (90H -> 83H)
    # and cycles Port B (81H) through Exercise 26 X-Y coordinates: 92H (X=2,Y=9), 82H (X=2,Y=8), 6DH (X=13,Y=6).
    program = [
        0x3E, 0x90,             # 1000: LD A, 90H
        0xD3, 0x83,             # 1002: OUT (83H), A
        0x21, 0x1B, 0x10,       # 1004: LD HL, 101BH
        0x0E, 0x03,             # 1007: LD C, 03H
        0x7E,                   # 1009: LD A, (HL)
        0xD3, 0x81,             # 100A: OUT (81H), A
        0x23,                   # 100C: INC HL
        0x16, 0x90,             # 100D: LD D, 90H (~0.27s deterministic hold per coordinate)
        0x06, 0x00,             # 100F: LD B, 00H (256 iterations per outer D tick)
        0x10, 0xFE,             # 1011: DJNZ 1011H
        0x15,                   # 1013: DEC D
        0x20, 0xF9,             # 1014: JR NZ, 100FH
        0x0D,                   # 1016: DEC C
        0x20, 0xF0,             # 1017: JR NZ, 1009H
        0x18, 0xE9,             # 1019: JR 1004H
        0x92, 0x82, 0x6D        # 101B: Packed (Y,X) bytes: 92H (X=2,Y=9), 82H (X=2,Y=8), 6DH (X=13,Y=6)
    ]
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=20)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=20)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the oscilloscope circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach oscilloscope manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "oscilloscope")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "oscilloscope"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 26,
        what="attach oscilloscope via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "oscilloscope"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-oscilloscope")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s FR-4 R-2R Ladder DAC PCB components and P31 CRT canvas
    stage_text = stage.inner_text()
    assert "CD4050" in stage_text
    assert "CMOS HEX BUFFER" in stage_text
    assert "X-AXIS R-2R LADDER (PB0..PB3)" in stage_text
    assert "Y-AXIS R-2R LADDER (PB4..PB7)" in stage_text
    assert "10kΩ/20kΩ" in stage_text
    assert stage.locator(".ext-axial-res").count() == 2
    assert "BNC CH-X" in stage_text
    assert "BNC CH-Y" in stage_text
    assert "P31 PHOSPHOR X-Y CRT" in stage_text
    assert page.locator("#ext-scope-canvas").is_visible()

    # Start Z80 execution at 1000H and verify live DOM updates across 0x92, 0x82, and 0x6D
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    def dom_matches_scope(expected_raw_pb, expected_x, expected_y):
        return page.evaluate(
            """([expRaw, expX, expY]) => {
                const rawEl = document.getElementById('scope-raw-pb');
                const bxEl = document.getElementById('scope-beam-x');
                const byEl = document.getElementById('scope-beam-y');
                if (!rawEl || !bxEl || !byEl) return false;
                return rawEl.textContent === expRaw &&
                       bxEl.textContent === expX &&
                       byEl.textContent === expY;
            }""",
            [expected_raw_pb, str(expected_x), str(expected_y)],
        )

    wait_for(
        lambda: dom_matches_scope("92H", 2, 9),
        timeout=10.0,
        poll=0.02,
        what="Oscilloscope 92H (X=2, Y=9) in live DOM",
    )
    wait_for(
        lambda: dom_matches_scope("82H", 2, 8),
        timeout=10.0,
        poll=0.02,
        what="Oscilloscope 82H (X=2, Y=8) in live DOM",
    )
    wait_for(
        lambda: dom_matches_scope("6DH", 13, 6),
        timeout=10.0,
        poll=0.02,
        what="Oscilloscope 6DH (X=13, Y=6) in live DOM",
    )

    wait_for(
        lambda: int(page.locator("#scope-samples").inner_text()) >= 3,
        timeout=5.0,
        what="#scope-samples >= 3 in live DOM",
    )

    # Verify non-zero green phosphor pixel rendering on #ext-scope-canvas at plotted coordinate (X=2, Y=9)
    has_phosphor_glow = page.evaluate(
        """() => {
            const canvas = document.getElementById('ext-scope-canvas');
            if (!canvas) return false;
            const ctx = canvas.getContext('2d');
            if (!ctx) return false;
            const pad = 14;
            const plotW = canvas.width - pad * 2;
            const plotH = canvas.height - pad * 2;
            const px = Math.round(pad + (2 / 15) * plotW);
            const py = Math.round((canvas.height - pad) - (9 / 15) * plotH);
            const data = ctx.getImageData(px - 2, py - 2, 5, 5).data;
            for (let i = 0; i < data.length; i += 4) {
                const r = data[i];
                const g = data[i + 1];
                if (g > 80 && g > r) {
                    return true;
                }
            }
            return false;
        }"""
    )
    assert has_phosphor_glow is True, "expected non-zero P31 green phosphor glow on #ext-scope-canvas"


def test_exercise27_school_bell_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..100FH that configures Primary 8255 Mode 0 (90H -> 83H),
    # pulls Port C bit 6 low (3FH -> 82H) to energize the school bell relay, and toggles Port C bit 7
    # (3FH <-> BFH) to drive speaker chime edges while keeping PC6=0.
    program = [
        0x3E, 0x90,  # 1000: LD A, 90H
        0xD3, 0x83,  # 1002: OUT (83H), A
        0x3E, 0x3F,  # 1004: LD A, 3FH (PC6=0 bell relay energized, PC7=0)
        0xD3, 0x82,  # 1006: OUT (82H), A
        0xEE, 0x80,  # 1008: XOR 80H   (toggle PC7 speaker between 3FH and BFH with PC6=0)
        0x06, 0x40,  # 100A: LD B, 40H
        0x10, 0xFE,  # 100C: DJNZ 100CH
        0x18, 0xF6,  # 100E: JR 1006H
    ]
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=20)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=20)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the school bell circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach school_bell manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "school_bell")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "school_bell"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 27,
        what="attach school_bell via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "school_bell"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-school_bell")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s relay-driver daughterboard PCB components
    stage_text = stage.inner_text()
    assert "10kΩ" in stage_text
    assert "SN7404" in stage_text
    assert "2SC1815" in stage_text
    assert "1N4001" in stage_text
    assert page.locator("#bell-relay").is_visible()
    assert page.locator("#bell-dome").is_visible()

    # Click #btn-bell-preset-0800 and #btn-bell-preset-0810 to verify #bell-bcd-time updates
    page.click("#btn-bell-preset-0800")
    wait_for(
        lambda: page.locator("#bell-bcd-time").inner_text() == "07:59:58",
        timeout=5.0,
        what="#bell-bcd-time updated to 07:59:58",
    )
    page.click("#btn-bell-preset-0810")
    wait_for(
        lambda: page.locator("#bell-bcd-time").inner_text() == "08:09:58",
        timeout=5.0,
        what="#bell-bcd-time updated to 08:09:58",
    )

    # Start Z80 execution at 1000H and verify #bell-relay.energized, #bell-dome.ringing, #bell-ring-count, and #bell-speaker-edges
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    wait_for(
        lambda: page.locator("#bell-relay").evaluate("el => el.classList.contains('energized')")
        and page.locator("#bell-dome").evaluate("el => el.classList.contains('ringing')")
        and int(page.locator("#bell-ring-count").inner_text()) >= 1
        and int(page.locator("#bell-speaker-edges").inner_text()) > 0,
        timeout=10.0,
        poll=0.02,
        what="#bell-relay.energized, #bell-dome.ringing, #bell-ring-count >= 1, and #bell-speaker-edges > 0 in live DOM",
    )


def test_exercise28_appliance_control_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..101CH that configures Auxiliary 8255 Mode 0 (80H -> 43H)
    # and cycles Auxiliary Port C (42H) through Exercise 28 scheduled relay masks: E1H and C4H.
    program = [
        0x3E, 0x80,             # 1000: LD A, 80H
        0xD3, 0x43,             # 1002: OUT (43H), A
        0x21, 0x1B, 0x10,       # 1004: LD HL, 101BH
        0x0E, 0x02,             # 1007: LD C, 02H
        0x7E,                   # 1009: LD A, (HL)
        0xD3, 0x42,             # 100A: OUT (42H), A
        0x23,                   # 100C: INC HL
        0x16, 0x90,             # 100D: LD D, 90H (~0.27s deterministic hold per relay mask)
        0x06, 0x00,             # 100F: LD B, 00H (256 iterations per outer D tick)
        0x10, 0xFE,             # 1011: DJNZ 1011H
        0x15,                   # 1013: DEC D
        0x20, 0xF9,             # 1014: JR NZ, 100FH
        0x0D,                   # 1016: DEC C
        0x20, 0xF0,             # 1017: JR NZ, 1009H
        0x18, 0xE9,             # 1019: JR 1004H
        0xE1, 0xC4,             # 101B: Relay schedule masks: E1H (06:00) and C4H (08:00)
    ]
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=20)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=20)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the appliance control circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach appliance_control manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "appliance_control")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "appliance_control"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 28,
        what="attach appliance_control via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "appliance_control"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-appliance_control")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s Aux 8255 relay-control daughterboard PCB components
    stage_text = stage.inner_text()
    assert "NEC D8255AC-2" in stage_text
    assert "74LS139 PIN 11" in stage_text
    assert "ULN2803A" in stage_text
    assert stage.locator(".appliance-card").count() == 8
    for bit in range(8):
        card = stage.locator(f'.appliance-card[data-bit="{bit}"]')
        assert card.is_visible()
        assert f"RY{bit + 1}" in card.inner_text()

    # Click #btn-app-preset-0600 and #btn-app-preset-1900 to verify #appliance-bcd-time updates
    page.click("#btn-app-preset-0600")
    wait_for(
        lambda: page.locator("#appliance-bcd-time").inner_text() == "05:59:58",
        timeout=5.0,
        what="#appliance-bcd-time updated to 05:59:58",
    )
    page.click("#btn-app-preset-1900")
    wait_for(
        lambda: page.locator("#appliance-bcd-time").inner_text() == "18:59:58",
        timeout=5.0,
        what="#appliance-bcd-time updated to 18:59:58",
    )

    # Start Z80 execution at 1000H and verify live DOM updates for E1H and C4H on Port 42H
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    def dom_matches_appliances(expected_mask_hex, active_bits):
        return page.evaluate(
            """([expMask, actBits]) => {
                const maskEl = document.getElementById('appliance-relay-mask');
                if (!maskEl || maskEl.textContent !== expMask) return false;
                const actSet = new Set(actBits);
                for (let b = 0; b < 8; ++b) {
                    const card = document.querySelector('.appliance-card[data-bit="' + b + '"]');
                    if (!card) return false;
                    const isAct = card.classList.contains('active') && card.classList.contains('energized');
                    if (actSet.has(b) && !isAct) return false;
                    if (!actSet.has(b) && isAct) return false;
                }
                return true;
            }""",
            [expected_mask_hex, list(active_bits)],
        )

    wait_for(
        lambda: dom_matches_appliances("E1H", {0, 5, 6, 7}),
        timeout=10.0,
        poll=0.02,
        what="Appliance mask E1H (bits 0, 5, 6, 7 active) in live DOM",
    )
    wait_for(
        lambda: dom_matches_appliances("C4H", {2, 6, 7}),
        timeout=10.0,
        poll=0.02,
        what="Appliance mask C4H (bits 2, 6, 7 active) in live DOM",
    )
    wait_for(
        lambda: int(page.locator("#appliance-updates").inner_text()) >= 2,
        timeout=5.0,
        what="#appliance-updates >= 2 in live DOM",
    )


def test_exercise29_robotic_arm_end_to_end(panel):
    panel.open()
    page = panel.page

    # Key in a compact Z80 driver at 1000H..102FH that configures Expansion 8255 Mode 0 (80H -> 03H),
    # zeros Ports A/B/C (00H..02H), then executes the exact Exercise 29 trajectory:
    #   Motor 1 (Port A 00H bits 0..1): +10 forward then -14 backward -> steps = -4, totalSteps = 24
    #   Motor 2 (Port B 01H bits 0..1): +16 forward -> steps = +16, totalSteps = 16
    #   Motor 3 (Port B 01H bits 4..5): -8 backward then +11 forward -> steps = +3, totalSteps = 19
    #   Electromagnet (Port C 02H bit 0): ON (01H) during pick, then OFF (00H) at STOP (HALT)
    program = [
        0x3E, 0x80,             # 1000: LD A, 80H (Expansion 8255 Mode 0 all outputs)
        0xD3, 0x03,             # 1002: OUT (03H), A
        0xAF,                   # 1004: XOR A
        0xD3, 0x00,             # 1005: OUT (00H), A (Motor 1 phase 0)
        0xD3, 0x01,             # 1007: OUT (01H), A (Motors 2 & 3 phase 0)
        0xD3, 0x02,             # 1009: OUT (02H), A (Electromagnet OFF)
        0x3E, 0x01,             # 100B: LD A, 01H (Electromagnet ON)
        0xD3, 0x02,             # 100D: OUT (02H), A
        0x21, 0x1F, 0x10,       # 100F: LD HL, 101FH (table of [port, val] pairs)
        0x06, 0x3B,             # 1012: LD B, 59 (24 M1 + 16 M2 + 19 M3 = 59 step writes)
        0x4E,                   # 1014: LD C, (HL)
        0x23,                   # 1015: INC HL
        0x7E,                   # 1016: LD A, (HL)
        0x23,                   # 1017: INC HL
        0xED, 0x79,             # 1018: OUT (C), A
        0x10, 0xF8,             # 101A: DJNZ 1014H
        0xAF,                   # 101C: XOR A (Electromagnet OFF at end of trajectory)
        0xD3, 0x02,             # 101D: OUT (02H), A
        0x76,                   # 101F-1: HALT at 101FH -> table starts at 1020H
    ]
    # Patch LD HL, 1020H at offset 5..6 of program (1010H..1011H)
    program[16] = 0x20
    program[17] = 0x10

    # Build the 59 [port, val] step pairs matching Exercise 29's exact Gray-code transitions:
    fwd_gray = [0x01, 0x03, 0x02, 0x00]
    bak_gray = [0x02, 0x03, 0x01, 0x00]

    # 1. Motor 1 forward 10 steps on Port 00H (ends at phase 03H, idx 1)
    m1_seq = [fwd_gray[i % 4] for i in range(10)]
    # 2. Motor 1 backward 14 steps from phase 03H (03 -> 01 -> 00 -> 02 -> 03...)
    bak_from_03 = [0x01, 0x00, 0x02, 0x03]
    m1_seq += [bak_from_03[i % 4] for i in range(14)]
    for v in m1_seq:
        program += [0x00, v]

    # 3. Motor 2 forward 16 steps on Port 01H bits 0..1 (ends at phase 00H)
    for i in range(16):
        program += [0x01, fwd_gray[i % 4]]

    # 4. Motor 3 backward 8 steps on Port 01H bits 4..5 (00 -> 20 -> 30 -> 10 -> 00)
    for i in range(8):
        program += [0x01, (bak_gray[i % 4] << 4)]
    # 5. Motor 3 forward 11 steps on Port 01H bits 4..5 (00 -> 10 -> 30 -> 20 -> 00)
    for i in range(11):
        program += [0x01, (fwd_gray[i % 4] << 4)]

    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=15)
    panel.wait_digits(lambda d: d.startswith("1000"), what="address 1000")
    for idx, b in enumerate(program):
        hi, lo = f"{b:02X}"[0], f"{b:02X}"[1]
        panel.type_keys(hi, lo, "+", delay=15)
        next_addr = f"{0x1000 + idx + 1:04X}"
        panel.wait_digits(lambda d, na=next_addr: d.upper().startswith(na), what=f"address {next_addr}")

    # Re-arm execution start address 1000H before attaching the robotic arm circuit
    panel.type_keys("ADRS", "1", "0", "0", "0", delay=20)
    panel.wait_digits(lambda d: d.upper().startswith("1000"), what="re-armed address 1000")

    # Attach robotic_arm manually via #ext-io-select -> opens #ext-io-window
    page.select_option("#ext-io-select", "robotic_arm")
    panel.wait_telemetry(
        lambda t: t.get("extCircuit", {}).get("id") == "robotic_arm"
        and t.get("extCircuit", {}).get("attached") is True
        and t.get("extCircuit", {}).get("exercise") == 29,
        what="attach robotic_arm via #ext-io-select",
    )
    assert page.locator("#ext-io-select").input_value() == "robotic_arm"
    assert page.locator("#ext-io-window").is_visible()
    stage = page.locator("#ext-stage-robotic_arm")
    assert stage.is_visible()
    assert stage.evaluate("el => el.classList.contains('active')")

    # Verify exposed early-'90s Expansion 8255 stepper-driver daughterboard PCB components & SVG stage
    stage_text = stage.inner_text()
    assert "NEC D8255AC-2" in stage_text
    assert "74LS139 PIN 12" in stage_text
    assert "SN7404" in stage_text
    assert "2SD313" in stage_text
    for m_idx in range(3):
        card = page.locator(f"#arm-motor-{m_idx}")
        assert card.is_visible()
        assert card.locator(".arm-coil-led").count() == 4
    assert page.locator("#ext-arm-svg").is_visible()
    assert page.locator("#arm-magnet-badge").is_visible()

    # Start Z80 execution at 1000H and verify live DOM updates for Motors 1..3, coil LEDs, and SVG transforms
    page.click('button[data-key="GO EXEC"]', delay=30, force=True)

    wait_for(
        lambda: page.locator("#arm-motor-0-steps").inner_text() == "-4"
        and page.locator("#arm-motor-0-total").inner_text() == "24"
        and page.locator("#arm-motor-1-steps").inner_text() == "+16"
        and page.locator("#arm-motor-1-total").inner_text() == "16"
        and page.locator("#arm-motor-2-steps").inner_text() == "+3"
        and page.locator("#arm-motor-2-total").inner_text() == "19",
        timeout=10.0,
        poll=0.02,
        what="Motor 1 (-4, 24), Motor 2 (+16, 16), Motor 3 (+3, 19) in live DOM",
    )

    assert page.locator("#arm-magnet-badge").inner_text() == "OFF"
    # Motor 3 ends at phase 2 (PB4..5 = 20H -> phase 2 -> coils = 0x06 -> B and C active)
    assert page.locator("#arm-motor-2-phase").inner_text() == "2"
    assert page.locator('#arm-motor-2 .arm-coil-led[data-coil="1"]').evaluate(
        "el => el.classList.contains('active')"
    )
    assert page.locator('#arm-motor-2 .arm-coil-led[data-coil="2"]').evaluate(
        "el => el.classList.contains('active')"
    )
    # Verify kinematic SVG transforms on #arm-joint-shoulder and #arm-joint-elbow
    shoulder_tf = page.locator("#arm-joint-shoulder").get_attribute("transform")
    elbow_tf = page.locator("#arm-joint-elbow").get_attribute("transform")
    assert "rotate(45)" in shoulder_tf
    assert "rotate(15)" in elbow_tf



