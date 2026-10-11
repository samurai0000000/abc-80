#
# test_led_colors.py
#
# The three discrete LEDs light as solid red or green lenses; the off state is unchanged (plan Envelope 2).
#
# Copyright (C) 2026, Charles Chiou
#

import re

from conftest import wait_for


def frame(leds):
    return {"type": "telemetry", "seq": 1, "rom": "monitor", "cycles": 1, "displayMasks": [0] * 6,
            "displayDigits": [" "] * 6, "leds": leds, "speakerLevel": False, "speakerEdges": [],
            "speakerOverflow": False,
            "registers": {"pc": 0, "sp": 0, "af": 0, "bc": 0, "de": 0, "hl": 0}}


def feed(page, leds):
    page.evaluate("t => window.__abc80.applyTelemetry(t)", frame(leds))


def style(page, selector, prop):
    return page.evaluate("([s, p]) => getComputedStyle(document.querySelector(s))[p]", [selector, prop])


def rgb(css):
    """'rgb(255, 26, 26)' or 'color(srgb 1 0.1 0.1)' as a tuple of 0-255 integers."""
    nums = [float(x) for x in re.findall(r"[0-9.]+", css)]
    if css.startswith("color("):
        nums = [n * 255 for n in nums[:3]]
    return tuple(int(round(n)) for n in nums[:3])


def wait_style(page, selector, prop, expected, what):
    wait_for(lambda: rgb(style(page, selector, prop)) == expected, timeout=3, what=what)


def test_ep_and_halt_leds_are_a_solid_red_lens_when_on(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    feed(page, {"ep": True, "halt": True, "speaker": 0.0})
    for led in ("#disc-led-ep .led-red", "#disc-led-halt .led-red"):
        wait_style(page, led, "backgroundColor", (255, 26, 26), led + " solid red")
        assert style(page, led, "backgroundImage") == "none", led
    feed(page, {"ep": False, "halt": False, "speaker": 0.0})
    for led in ("#disc-led-ep .led-red", "#disc-led-halt .led-red"):
        wait_for(lambda: rgb(style(page, led, "backgroundColor")) != (255, 26, 26), timeout=3, what=led + " off again")
        assert style(page, led, "backgroundImage") != "none", led


def test_speaker_led_is_a_solid_green_lens_at_full_level_and_dark_at_zero(panel):
    panel.open()
    page = panel.page
    page.evaluate("window.__abc80.disconnect()")
    led = "#disc-led-audio-grn .led-green"
    feed(page, {"ep": False, "halt": False, "speaker": 1.0})
    wait_style(page, led, "backgroundColor", (0, 224, 56), "full-level solid green")
    feed(page, {"ep": False, "halt": False, "speaker": 0.0})
    wait_style(page, led, "backgroundColor", (13, 46, 20), "zero-level dark green")
