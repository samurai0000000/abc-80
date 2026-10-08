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


def test_panels_without_server_data_do_not_show_sample_data(panel):
    panel.open()
    page = panel.page
    assert "not provided" in page.inner_text("#disasm-container")
    assert "LD" not in page.inner_text("#disasm-container")
    assert "not provided" in page.inner_text("#hex-editor-table")
    assert page.inner_text("#reg-ix") == "----"
    assert page.inner_text("#reg-iy") == "----"
