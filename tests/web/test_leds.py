#
# test_leds.py
#
# The three LEDs: green speaker LED (brightness follows the speaker line), red HALT, red EP
# (plan scenarios A13, A14).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def frame(leds, **extra):
    t = {"type": "telemetry", "seq": 1, "rom": "monitor", "cycles": 1, "displayMasks": [0] * 6,
         "displayDigits": [" "] * 6, "leds": leds, "speakerLevel": False, "speakerEdges": [],
         "speakerOverflow": False, "registers": {"pc": 0, "sp": 0, "af": 0, "bc": 0, "de": 0, "hl": 0}}
    t.update(extra)
    return t


def feed(page, telemetry):
    page.evaluate("t => window.__abc80.applyTelemetry(t)", telemetry)


def test_led_elements_follow_the_telemetry(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    feed(page, frame({"ep": True, "halt": False, "speaker": 0.0}))
    assert page.evaluate("document.getElementById('disc-led-ep').classList.contains('active')")
    assert not page.evaluate("document.getElementById('disc-led-halt').classList.contains('active')")
    feed(page, frame({"ep": False, "halt": True, "speaker": 0.0}))
    assert not page.evaluate("document.getElementById('disc-led-ep').classList.contains('active')")
    assert page.evaluate("document.getElementById('disc-led-halt').classList.contains('active')")


def test_a14_green_led_brightness_maps_from_the_speaker_fraction(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    for level, expected_opacity in ((0.0, 0.65), (0.5, 0.825), (1.0, 1.0)):
        feed(page, frame({"ep": False, "halt": False, "speaker": level}))
        assert page.evaluate("document.getElementById('disc-led-audio-grn').dataset.level") == "%.3f" % level
        # The LED fades over 0.1 s (CSS transition): wait for the settled value.
        wait_for(lambda: abs(float(page.evaluate(
            "getComputedStyle(document.querySelector('#disc-led-audio-grn .led-green')).opacity")) - expected_opacity) < 0.01,
            timeout=3, what="LED opacity %.3f for level %.1f" % (expected_opacity, level))


def test_a14_green_led_is_dark_at_idle_and_lights_during_a_keyclick(panel):
    panel.open()
    page = panel.page
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="start display")
    wait_for(lambda: page.evaluate("document.getElementById('disc-led-audio-grn').dataset.level") == "0.000",
             what="the speaker LED dark at idle")
    page.evaluate("""() => { window.__maxLed = 0;
        window.__ledTimer = setInterval(() => { window.__maxLed = Math.max(window.__maxLed,
            parseFloat(document.getElementById('disc-led-audio-grn').dataset.level || '0')); }, 5); }""")
    panel.click_key("ADRS")  # the monitor answers every key with a click on the speaker
    wait_for(lambda: page.evaluate("window.__maxLed") > 0.05, timeout=5, what="the LED lighting during the click")
    page.evaluate("clearInterval(window.__ledTimer)")
    wait_for(lambda: page.evaluate("document.getElementById('disc-led-audio-grn').dataset.level") == "0.000",
             what="the LED dark again after the click")


def test_a13_halt_led_lights_when_a_keyed_in_program_halts_and_reset_clears_it(panel):
    """Keys in 'ld a,55h / halt' at 2000h, runs it with GO, watches A and the HALT LED."""
    panel.open()
    panel.type_keys("ADRS", "2", "0", "0", "0", "DATA", "3", "E", "+", "5", "5", "+", "7", "6", "+")
    panel.type_keys("ADRS", "2", "0", "0", "0")
    panel.wait_digits(lambda d: d.startswith("2000"), what="start address 2000")
    panel.type_keys("GO EXEC")
    panel.wait_telemetry(lambda t: t["leds"]["halt"], timeout=10, what="the HALT LED")
    t = panel.telemetry()
    assert (t["registers"]["af"] >> 8) == 0x55, "the program ran: A holds 55h"
    assert panel.page.evaluate("document.getElementById('disc-led-halt').classList.contains('active')")
    panel.page.click("#btn-hw-rst", delay=50)
    panel.wait_telemetry(lambda t: not t["leds"]["halt"], what="the HALT LED cleared by reset")
