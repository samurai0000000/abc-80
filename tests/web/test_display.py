#
# test_display.py
#
# The six 7-segment digits: every segment of every digit maps to the right SVG element and
# colour, and the live display agrees with the telemetry (plan Section 2.3 item D).
#
# Copyright (C) 2026, Charles Chiou
#

import io

import pytest
from PIL import Image, ImageChops

from conftest import GOLDEN_DIR, wait_for

DIGIT_IDS = ["digit-a3", "digit-a2", "digit-a1", "digit-a0", "digit-d1", "digit-d0"]
SEGMENTS = ["a", "b", "c", "d", "e", "f", "g", "dp"]
LIT = "rgb(240, 80, 64)"    # #f05040
DARK = "rgb(74, 21, 16)"    # #4a1510


def apply_masks(page, masks):
    page.evaluate("""masks => window.__abc80.applyTelemetry({
        type: 'telemetry', seq: 1, rom: 'monitor', cycles: 1, displayMasks: masks,
        displayDigits: [' ', ' ', ' ', ' ', ' ', ' '], leds: {ep: false, halt: false, speaker: 0},
        speakerLevel: false, speakerEdges: [], speakerOverflow: false,
        registers: {pc: 0, sp: 0, af: 0, bc: 0, de: 0, hl: 0}})""", masks)


def active_segments(page, digit_id):
    return page.evaluate("""id => Array.from(document.querySelectorAll('#' + id + ' .seg-path.active'))
        .map(e => Array.from(e.classList).find(c => c.startsWith('seg-') && c !== 'seg-path'))""", digit_id)


def fill_of(page, digit_id, segment):
    return page.evaluate("""([id, seg]) => getComputedStyle(
        document.querySelector('#' + id + ' .seg-path.seg-' + seg)).fill""", [digit_id, segment])


def test_every_segment_of_every_digit_maps_to_its_own_element(panel):
    """Page renderer against the telemetry format: the 8-bit truth table for all six digits."""
    panel.open()
    page = panel.page
    # The server keeps sending live frames; freeze the renderer's input by making the live
    # connection quiet: close it so only the injected frames reach the page.
    page.evaluate("window.__abc80.disconnect && window.__abc80.disconnect()")
    for index, digit_id in enumerate(DIGIT_IDS):
        for bit, segment in enumerate(SEGMENTS):
            masks = [0] * 6
            masks[index] = 1 << bit
            apply_masks(page, masks)
            assert active_segments(page, digit_id) == ["seg-" + segment], (digit_id, segment)
            for other in DIGIT_IDS:
                if other != digit_id:
                    assert active_segments(page, other) == [], (digit_id, segment, other)
            wait_for(lambda: fill_of(page, digit_id, segment) == LIT, timeout=2, what="lit fill colour")
            other_segment = SEGMENTS[(bit + 1) % 8]
            assert fill_of(page, digit_id, other_segment) == DARK


def test_live_display_matches_the_telemetry_masks(panel):
    panel.open()
    t = panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="start display")
    for index, digit_id in enumerate(DIGIT_IDS):
        mask = t["displayMasks"][index]
        expected = sorted("seg-" + SEGMENTS[b] for b in range(8) if mask & (1 << b))
        assert sorted(active_segments(panel.page, digit_id)) == expected, digit_id


def test_golden_screenshot_of_the_digits(panel, request):
    panel.open()
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="start display")
    # Settle: the monitor keeps the same start display; take the shot once the telemetry is stable.
    first = panel.telemetry()["displayMasks"]
    wait_for(lambda: panel.telemetry()["displayMasks"] == first, what="stable display")
    clip = panel.page.evaluate("""() => {
        const boxes = ['digit-a3', 'digit-d0'].map(id => document.getElementById(id).getBoundingClientRect());
        return {x: boxes[0].left - 6, y: Math.min(boxes[0].top, boxes[1].top) - 6,
                width: boxes[1].right - boxes[0].left + 12, height: Math.max(boxes[0].height, boxes[1].height) + 12};
    }""")
    panel.page.wait_for_timeout(150)  # the segment fill transition is 40 ms
    shot = Image.open(io.BytesIO(panel.page.screenshot(clip=clip))).convert("RGB")
    golden = GOLDEN_DIR / "digits_monitor_start.png"
    if request.config.getoption("--update-goldens") or not golden.exists():
        GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
        shot.save(golden)
        pytest.skip("golden written: %s" % golden)
    ref = Image.open(golden).convert("RGB")
    assert ref.size == shot.size, "screenshot size changed: %s vs %s" % (shot.size, ref.size)
    diff = ImageChops.difference(ref, shot).convert("L")
    differing = sum(1 for v in diff.tobytes() if v > 24)
    assert differing / (ref.size[0] * ref.size[1]) <= 0.01, "more than 1%% of pixels differ (%d)" % differing
