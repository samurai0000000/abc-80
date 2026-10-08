#
# test_rom_switch.py
#
# Choosing a ROM: the book monitor boots into the keypad monitor; the author's 1993 ROM boots into
# its PC-link server with a dark display and a dead keypad (plan scenarios A11, A12).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def test_a11_a12_the_1993_rom_boots_dark_and_deaf_and_switching_back_restores_the_monitor(panel):
    panel.open()
    panel.wait_telemetry(lambda t: t["rom"] == "monitor" and any(m != 0 for m in t["displayMasks"]), what="monitor display")

    panel.page.select_option("#rom-select", "1993")
    panel.wait_telemetry(lambda t: t["rom"] == "1993", timeout=20, what="the board switched to the 1993 ROM")
    wait_for(lambda: not panel.lit(), what="a dark display at the link poll")
    panel.type_keys("ADRS", "1")
    panel.page.wait_for_timeout(800)
    assert not panel.lit(), "keys do nothing at the link poll"

    panel.page.select_option("#rom-select", "monitor")
    panel.wait_telemetry(lambda t: t["rom"] == "monitor" and any(m != 0 for m in t["displayMasks"]),
                         timeout=20, what="the monitor display again")
    panel.type_keys("ADRS", "4", "2")
    panel.wait_digits(lambda d: "42" in d, what="the keypad works again")


def test_the_rom_choice_survives_a_reload(panel):
    panel.open(rom="1993")
    panel.wait_telemetry(lambda t: t["rom"] == "1993", timeout=20, what="1993 ROM")
    panel.page.reload(wait_until="domcontentloaded")
    panel.wait_telemetry(lambda t: t["rom"] == "1993", timeout=20, what="the same ROM after the reload")
    assert panel.page.evaluate("document.getElementById('rom-select').value") == "1993"
