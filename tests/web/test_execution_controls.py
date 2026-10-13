#
# test_execution_controls.py
#
# Automated Playwright tests for CPU execution controls (Run, Pause, Step),
# status badge, keyboard shortcuts, and sticky bounded disassembly window.
#
# Copyright (C) 2026, Charles Chiou
#

import pytest
from conftest import wait_for


def test_initial_execution_control_state_and_elements(panel):
    panel.open()
    page = panel.page

    # Verify buttons and badge exist
    assert page.locator("#btn-cpu-run").count() == 1
    assert page.locator("#btn-cpu-pause").count() == 1
    assert page.locator("#btn-cpu-step").count() == 1
    assert page.locator("#cpu-status-badge").count() == 1

    # Verify SVG icons
    assert page.locator("#btn-cpu-run svg").count() == 1
    assert page.locator("#btn-cpu-pause svg").count() == 1
    assert page.locator("#btn-cpu-step svg").count() == 1

    # Initially the emulator is running
    assert page.locator("#btn-cpu-run").is_disabled()
    assert page.locator("#btn-cpu-pause").is_enabled()
    assert page.locator("#btn-cpu-step").is_disabled()
    assert page.inner_text("#cpu-status-badge").strip() == "RUNNING"
    assert "running" in page.locator("#cpu-status-badge").get_attribute("class")


def test_pause_and_resume_controls(panel):
    panel.open()
    page = panel.page

    # Click Pause
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="telemetry paused")

    # In paused state
    wait_for(lambda: page.locator("#btn-cpu-run").is_enabled(), what="run button enabled")
    assert page.locator("#btn-cpu-pause").is_disabled()
    assert page.locator("#btn-cpu-step").is_enabled()
    assert page.inner_text("#cpu-status-badge").strip() == "PAUSED"
    assert "paused" in page.locator("#cpu-status-badge").get_attribute("class")

    # Disassembly should be populated and not dimmed
    wait_for(lambda: page.locator("#disasm-container .disasm-line.current").count() == 1, what="disasm active line")
    assert "dimmed" not in page.locator("#disasm-container").get_attribute("class")

    # Click Run
    page.click("#btn-cpu-run")
    panel.wait_telemetry(lambda t: t.get("paused") is False, what="telemetry running")

    # Back in running state
    wait_for(lambda: page.locator("#btn-cpu-run").is_disabled(), what="run button disabled")
    assert page.locator("#btn-cpu-pause").is_enabled()
    assert page.locator("#btn-cpu-step").is_disabled()
    assert page.inner_text("#cpu-status-badge").strip() == "RUNNING"
    assert "running" in page.locator("#cpu-status-badge").get_attribute("class")
    assert "dimmed" in page.locator("#disasm-container").get_attribute("class")


def test_step_execution_controls(panel):
    panel.open()
    page = panel.page

    # Pause CPU
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="telemetry paused")
    wait_for(lambda: page.locator("#disasm-container .disasm-line.current").count() == 1, what="disasm active line")

    t1 = panel.telemetry()
    cycles1 = t1["cycles"]
    pc1 = t1["registers"]["pc"]

    # Click Step button
    page.click("#btn-cpu-step")
    panel.wait_telemetry(lambda t: t.get("paused") is True and t["cycles"] > cycles1,
                         timeout=10, what="cycles advance on step")

    # Still paused
    assert page.locator("#btn-cpu-step").is_enabled()
    assert page.inner_text("#cpu-status-badge").strip() == "PAUSED"
    t_step1 = panel.telemetry()
    assert t_step1["registers"]["pc"] != pc1 or t_step1["cycles"] > cycles1
    wait_for(lambda: page.locator("#disasm-container .disasm-line.current").count() == 1, what="disasm active line")
    current_addr = int(page.locator("#disasm-container .disasm-line.current").get_attribute("data-addr"))
    assert current_addr == t_step1["registers"]["pc"]

    # Redundant disasm-step button must not exist
    assert page.locator("#btn-disasm-step").count() == 0

    # Step again via main step button
    t2 = panel.telemetry()
    cycles2 = t2["cycles"]
    pc2 = t2["registers"]["pc"]
    page.click("#btn-cpu-step")
    panel.wait_telemetry(lambda t: t.get("paused") is True and t["cycles"] > cycles2,
                         timeout=10, what="cycles advance on step")
    t_step2 = panel.telemetry()
    assert t_step2["registers"]["pc"] != pc2 or t_step2["cycles"] > cycles2
    wait_for(lambda: int(page.locator("#disasm-container .disasm-line.current").get_attribute("data-addr")) == t_step2["registers"]["pc"],
             what="disasm line current matches PC")


def test_keyboard_shortcuts(panel):
    panel.open()
    page = panel.page

    # Initially running, press F6 to pause
    page.keyboard.press("F6")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="paused via F6")

    # Press Space or F7 to step
    t1 = panel.telemetry()
    cycles1 = t1["cycles"]
    page.keyboard.press("Space")
    panel.wait_telemetry(lambda t: t["cycles"] > cycles1, what="stepped via Space")

    t2 = panel.telemetry()
    cycles2 = t2["cycles"]
    page.keyboard.press("F7")
    panel.wait_telemetry(lambda t: t["cycles"] > cycles2, what="stepped via F7")

    # Press F5 to run
    page.keyboard.press("F5")
    panel.wait_telemetry(lambda t: t.get("paused") is False, what="running via F5")


def test_sticky_bounded_disassembly_window(panel):
    panel.open()
    page = panel.page

    # Pause CPU
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="paused")
    wait_for(lambda: page.locator("#disasm-container .disasm-line.current").count() == 1, what="disasm active line")

    # Read min and max address in the current window
    disasm_state = page.evaluate("window.__abc80.disasmState")
    min_addr = disasm_state["minAddr"]
    max_addr = disasm_state["maxAddr"]
    assert min_addr is not None and max_addr is not None
    assert max_addr >= min_addr

    # Step instruction: if next PC is still within [minAddr .. maxAddr] on a row,
    # the window boundaries must not change (sticky property)
    t = panel.telemetry()
    pc = t["registers"]["pc"]
    lines = disasm_state["lines"]
    current_index = next((i for i, l in enumerate(lines) if l["addr"] == pc), None)
    assert current_index is not None, "PC must be on a decoded row"

    # Step one instruction
    page.click("#btn-cpu-step")
    panel.wait_telemetry(lambda t_new: t_new["cycles"] > t["cycles"], what="stepped")

    new_disasm_state = page.evaluate("window.__abc80.disasmState")
    new_t = panel.telemetry()
    new_pc = new_t["registers"]["pc"]

    # Unconditional assertion: new PC must be within decoded rows and window must not relocate
    assert any(l["addr"] == new_pc for l in lines), f"New PC 0x{new_pc:04X} must be within decoded window rows"
    assert new_disasm_state["minAddr"] == min_addr
    assert new_disasm_state["maxAddr"] == max_addr
    # Current highlight shifted to new PC
    assert page.locator("#disasm-container .disasm-line.current").count() == 1
    current_el_addr = page.locator("#disasm-container .disasm-line.current").get_attribute("data-addr")
    assert int(current_el_addr) == new_pc

    # Test off-row relocation: PC is within [minAddr .. maxAddr] but on no row boundary
    # Take an address halfway into a multi-byte instruction in the active ROM window
    multi_byte_line = next((l for l in new_disasm_state["lines"] if len(l["bytes"]) > 1), None)
    assert multi_byte_line is not None, "multi-byte instruction expected in monitor ROM disassembly window"
    off_row_pc = multi_byte_line["addr"] + 1
    page.evaluate("""(addr) => window.__abc80.applyTelemetry({
        type: 'telemetry', seq: 999, rom: 'monitor', paused: true, cycles: 99999,
        registers: {pc: addr, sp: 0x17FE, af: 0, bc: 0, de: 0, hl: 0, ix: 0, iy: 0},
        displayDigits: [' ',' ',' ',' ',' ',' '], displayMasks: [0,0,0,0,0,0],
        leds: {ep: false, halt: false, speaker: 0}, speakerLevel: false,
        speakerEdges: [], speakerOverflow: false})""", off_row_pc)
    wait_for(lambda: page.evaluate("window.__abc80.disasmState.minAddr") == off_row_pc,
             what="window relocated to off_row_pc")

    # Test out-of-range relocation: simulate PC jump outside window via telemetry
    out_of_bounds_pc = 0x1500
    page.evaluate("""(addr) => window.__abc80.applyTelemetry({
        type: 'telemetry', seq: 1000, rom: 'monitor', paused: true, cycles: 100000,
        registers: {pc: addr, sp: 0x17FE, af: 0, bc: 0, de: 0, hl: 0, ix: 0, iy: 0},
        displayDigits: [' ',' ',' ',' ',' ',' '], displayMasks: [0,0,0,0,0,0],
        leds: {ep: false, halt: false, speaker: 0}, speakerLevel: false,
        speakerEdges: [], speakerOverflow: false})""", out_of_bounds_pc)
    wait_for(lambda: page.evaluate("window.__abc80.disasmState.minAddr") == out_of_bounds_pc,
             what="window relocated to out_of_bounds_pc")


def test_disassembly_window_stays_put_while_running_and_relocates_on_pause(panel):
    panel.open()
    page = panel.page
    wait_for(lambda: page.locator("#disasm-container .disasm-line").count() > 0,
             what="initial disassembly loaded")
    initial_min = page.evaluate("window.__abc80.disasmState.minAddr")
    far_pc = 0x1500
    assert far_pc > page.evaluate("window.__abc80.disasmState.maxAddr")

    frame = """([addr, paused, seq]) => window.__abc80.applyTelemetry({
        type: 'telemetry', seq: seq, rom: 'monitor', paused: paused, cycles: 1000000 + seq,
        registers: {pc: addr, sp: 0x17FE, af: 0, bc: 0, de: 0, hl: 0, ix: 0, iy: 0},
        displayDigits: [' ',' ',' ',' ',' ',' '], displayMasks: [0,0,0,0,0,0],
        leds: {ep: false, halt: false, speaker: 0}, speakerLevel: false,
        speakerEdges: [], speakerOverflow: false})"""

    # Running: PC far outside the window must not move it.
    page.evaluate(frame, [far_pc, False, 5000])
    page.wait_for_timeout(500)
    assert page.evaluate("window.__abc80.disasmState.minAddr") == initial_min

    # Paused: the sticky rule now relocates to the PC.
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="paused")
    page.evaluate(frame, [far_pc, True, 5001])
    wait_for(lambda: page.evaluate("window.__abc80.disasmState.minAddr") == far_pc,
             what="window relocated to far_pc after pause")
    assert page.locator("#disasm-container .disasm-line").count() > 0



def test_no_continuous_polling_while_running(panel):
    """Transport-level check: wrap WebSocket.prototype.send before any page script runs, so the
    app's internal sendHostCommand calls are observed. After the cold-start window has loaded,
    a running emulator must send zero readMem / readDisasm frames."""
    page = panel.page
    page.add_init_script("""
        window.__wsSent = [];
        const origSend = WebSocket.prototype.send;
        WebSocket.prototype.send = function (data) {
            try { window.__wsSent.push(JSON.parse(data).type); } catch (e) { window.__wsSent.push('?'); }
            return origSend.apply(this, arguments);
        };
    """)
    panel.open()
    panel.wait_telemetry(lambda t: t.get("paused") is False, what="running")
    wait_for(lambda: page.locator("#disasm-container .disasm-line").count() > 0,
             what="cold-start disassembly loaded")
    page.evaluate("window.__wsSent.length = 0")

    page.wait_for_timeout(1000)
    assert panel.telemetry().get("paused") is False
    polled = [t for t in page.evaluate("window.__wsSent") if t in ("readMem", "readDisasm")]
    assert polled == [], f"Expected 0 memory/disasm queries while running, got {polled}"

    # Positive control: the hook really sees app traffic.
    page.click("#btn-cpu-pause")
    wait_for(lambda: "pause" in page.evaluate("window.__wsSent"), what="hook observed pause command")


def test_disassembly_container_and_tab_cpu_have_no_vertical_scrollbar(panel):
    """Enforces Directive 10: tab-cpu and disasm-container must not produce vertical scrollbars."""
    panel.open()
    page = panel.page
    wait_for(lambda: page.locator("#disasm-container .disasm-line").count() > 0,
             what="disassembly lines loaded")

    # Verify tab-cpu does not overflow vertically
    scroll_h, client_h = page.evaluate(
        "() => [document.getElementById('tab-cpu').scrollHeight, document.getElementById('tab-cpu').clientHeight]")
    assert scroll_h <= client_h + 2, f"tab-cpu has vertical scrollbar: scrollHeight={scroll_h}, clientHeight={client_h}"

    # Verify disasm-container does not overflow vertically
    d_scroll_h, d_client_h = page.evaluate(
        "() => [document.getElementById('disasm-container').scrollHeight, document.getElementById('disasm-container').clientHeight]")
    assert d_scroll_h <= d_client_h + 2, f"disasm-container has vertical scrollbar: scrollHeight={d_scroll_h}, clientHeight={d_client_h}"


def test_guide_modal_documents_execution_shortcuts(panel):
    panel.open()
    text = panel.page.evaluate("document.getElementById('exec-shortcuts-table').textContent")
    for key in ("F5", "F6", "F7", "Space", "Escape"):
        assert key in text, f"{key} missing from guide modal execution shortcuts"
