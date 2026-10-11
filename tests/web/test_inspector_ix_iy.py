#
# test_inspector_ix_iy.py
#
# IX and IY come from the telemetry frame like the other registers, and read "----" until the first
# frame that carries them arrives (the panel never shows sample data).
#
# Copyright (C) 2026, Charles Chiou
#


def frame(registers):
    return {"type": "telemetry", "seq": 1, "rom": "monitor", "cycles": 1, "displayMasks": [0] * 6,
            "displayDigits": [" "] * 6, "leds": {"ep": False, "halt": False, "speaker": 0.0},
            "speakerLevel": False, "speakerEdges": [], "speakerOverflow": False, "registers": registers}


def test_ix_and_iy_show_the_telemetry_values(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    regs = {"pc": 0, "sp": 0, "af": 0, "bc": 0, "de": 0, "hl": 0, "ix": 0x2468, "iy": 0x1357}
    page.evaluate("t => window.__abc80.applyTelemetry(t)", frame(regs))
    assert page.inner_text("#reg-ix") == "0x2468"
    assert page.inner_text("#reg-iy") == "0x1357"


def test_ix_and_iy_keep_their_last_value_when_a_frame_carries_none(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    regs = {"pc": 0, "sp": 0, "af": 0, "bc": 0, "de": 0, "hl": 0, "ix": 0x2468, "iy": 0x1357}
    page.evaluate("t => window.__abc80.applyTelemetry(t)", frame(regs))
    older = {"pc": 1, "sp": 2, "af": 3, "bc": 4, "de": 5, "hl": 6}
    page.evaluate("t => window.__abc80.applyTelemetry(t)", frame(older))
    assert page.inner_text("#reg-ix") == "0x2468"
    assert page.inner_text("#reg-iy") == "0x1357"


def test_ix_and_iy_are_dashes_before_any_telemetry_arrives(panel):
    page = panel.page
    page.add_init_script("window.WebSocket = class { constructor() {} send() {} close() {} addEventListener() {} };")
    page.goto(panel.server.url, wait_until="domcontentloaded")
    page.wait_for_selector("#reg-ix")
    assert page.inner_text("#reg-ix") == "----"
    assert page.inner_text("#reg-iy") == "----"
