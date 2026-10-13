#
# test_inspector.py
#
# The inspector panel shows the board's real registers and flags, and says so when it has nothing
# to show (it does not display sample data).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def hex4(v):
    return "0x%04X" % v


def test_registers_and_flags_match_the_telemetry_frame(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")  # freeze on one frame so the numbers cannot move under the check
    t = page.evaluate("window.__abc80.lastTelemetry")
    page.evaluate("t => window.__abc80.applyTelemetry(t)", t)
    regs = t["registers"]
    for name in ("pc", "sp", "af", "bc", "de", "hl"):
        assert page.inner_text("#reg-" + name) == hex4(regs[name]), name
    flags = {"flag-s": 0x80, "flag-z": 0x40, "flag-h": 0x10, "flag-pv": 0x04, "flag-n": 0x02, "flag-c": 0x01}
    for element, mask in flags.items():
        lit = page.evaluate("id => document.getElementById(id).classList.contains('active')", element)
        assert lit == bool(regs["af"] & mask), element


def test_disassembly_panel_shows_live_disassembly_when_paused(panel):
    panel.open()
    page = panel.page
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True, what="CPU paused")
    wait_for(lambda: page.locator("#disasm-container .disasm-line.current").count() == 1, what="disassembly active line")
    assert page.locator("#disasm-container .disasm-line").count() > 0
    assert "▶" in page.inner_text("#disasm-container .disasm-line.current")
    assert page.inner_text("#cpu-status-badge") == "PAUSED"


def test_cold_load_live_disassembly_and_memory_while_running(panel):
    panel.open()
    page = panel.page

    # Verify emulator is running on cold load
    assert page.inner_text("#cpu-status-badge").strip() == "RUNNING"

    # Disassembly stream must decode instructions immediately on cold load (dimmed)
    wait_for(lambda: page.locator("#disasm-container .disasm-line").count() > 0,
             what="cold load disassembly decoded")
    assert "dimmed" in page.locator("#disasm-container").get_attribute("class")

    # Switch to Memory Map tab while running
    page.click("button[data-tab='tab-memory']")
    wait_for(lambda: page.locator("#tab-memory").is_visible(), what="tab-memory visible")

    # Memory hex viewer must be populated
    wait_for(lambda: page.locator("#hex-editor-table .hex-row").count() > 0,
             what="cold load memory hex rows loaded")
    assert page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "0000:"

    # Stack memory window must be populated with TOS while running
    wait_for(lambda: page.locator("#stack-editor-table .stack-row").count() > 0,
             what="cold load stack rows loaded")
    top_stack_row = page.locator("#stack-editor-table .stack-row").first
    assert "tos" in top_stack_row.get_attribute("class")
    assert "▶ TOS" in top_stack_row.locator(".stack-marker").inner_text()
    assert "dimmed" in page.locator("#stack-editor-table").get_attribute("class")


