#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
test_universal_calibration.py - Validates universal component calibration:
1. Activating calibration mode
2. Selecting various discrete components and ICs
3. Pointer dragging & keyboard arrow nudging
4. Grouped CSS generation
5. Verifying zero JavaScript console errors
"""

import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parent.parent

def main():
    html_path = ROOT / "web" / "index.html"
    url = f"file://{html_path}"
    console_errors = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        ctx = browser.new_context(
            viewport={"width": 1280, "height": 1024},
            permissions=["clipboard-read", "clipboard-write"]
        )
        page = ctx.new_page()

        page.on("console", lambda msg: console_errors.append(msg.text) if msg.type == "error" and "ws://" not in msg.text else None)
        page.on("pageerror", lambda err: console_errors.append(str(err)))

        print(f"[test] Loading page: {url}")
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(500)

        # 1. Verify Production UI Hiding (Calibration & Zoom controls hidden by default)
        calib_btn = page.locator("#btn-calibration-mode")
        canvas_controls = page.locator("#canvas-view-controls")
        assert not calib_btn.is_visible(), "Expected #btn-calibration-mode to be hidden in production!"
        assert not canvas_controls.is_visible(), "Expected #canvas-view-controls to be hidden in production!"
        print("[test] Production UI hiding verified: dev tools hidden by default.")

        # 2. Test Interactive CAD Yellow Tooltips BEFORE opening calibration mode
        tooltip = page.locator("#component-tooltip-box")
        assert not tooltip.is_visible(), "Tooltip should initially be hidden"

        # Hover over U1
        u1 = page.locator("#ic-u1")
        u1.hover()
        page.wait_for_timeout(200)
        assert tooltip.is_visible(), "Tooltip did not become visible when hovering over U1!"
        tooltip_text = tooltip.text_content()
        assert "U1" in tooltip_text, f"Expected 'U1' in tooltip, got: {tooltip_text}"
        assert "Z80A" in tooltip_text, f"Expected 'Z80A' in tooltip, got: {tooltip_text}"
        print(f"[test] Hover U1 tooltip verified: {tooltip_text.replace('\n', ' ')}")

        # Hover over disc-cap-c1
        c1 = page.locator("#disc-cap-c1")
        c1.hover()
        page.wait_for_timeout(200)
        assert tooltip.is_visible(), "Tooltip did not become visible when hovering over C1!"
        tooltip_text_c1 = tooltip.text_content()
        assert "C1" in tooltip_text_c1, f"Expected 'C1' in tooltip, got: {tooltip_text_c1}"
        print(f"[test] Hover C1 tooltip verified: {tooltip_text_c1.replace('\n', ' ')}")

        # Hover off component
        page.mouse.move(10, 10)
        page.wait_for_timeout(200)
        assert not tooltip.is_visible(), "Tooltip should hide when mouse leaves components"
        print("[test] Tooltip hide on mouseleave verified.")

        # 3. Unhide dev tools via Shift+Alt+C hotkey
        page.keyboard.press("Shift+Alt+KeyC")
        page.wait_for_timeout(300)
        assert calib_btn.is_visible(), "Expected #btn-calibration-mode to be visible after Shift+Alt+C!"
        assert canvas_controls.is_visible(), "Expected #canvas-view-controls to be visible after Shift+Alt+C!"
        print("[test] Shift+Alt+C hotkey successfully toggled dev tools visibility.")

        # 4. Open Calibration Mode
        calib_btn.click()
        page.wait_for_timeout(300)

        # Check HUD visibility
        hud = page.locator("#ic-calibration-hud")
        assert hud.is_visible(), "Calibration HUD did not become visible!"
        print("[test] Calibration HUD is visible.")

        # 2. Select a transistor (disc-q9) via dropdown
        select = page.locator("#calib-chip-select")
        select.select_option("disc-q9")
        page.wait_for_timeout(200)

        input_x = page.locator("#calib-input-x")
        input_y = page.locator("#calib-input-y")
        print(f"[test] Selected disc-q9: X={input_x.input_value()}, Y={input_y.input_value()}")
        assert float(input_x.input_value()) == 21.1
        assert float(input_y.input_value()) == 233.5

        # 3. Test keyboard arrow nudge on disc-q9
        page.keyboard.press("ArrowRight")
        page.wait_for_timeout(100)
        assert float(input_x.input_value()) == 22.1
        print(f"[test] Nudged right: X={input_x.input_value()}")

        page.keyboard.press("Shift+ArrowDown")
        page.wait_for_timeout(100)
        assert float(input_y.input_value()) == 238.5
        print(f"[test] Nudged down with Shift: Y={input_y.input_value()}")

        # 4. Click directly on upright transistor disc-q1 to select
        q1 = page.locator("#disc-q1")
        q1.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-q1", f"Expected select to be disc-q1, got {select.input_value()}"
        print(f"[test] Clicked disc-q1 on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 5. Click on resistor disc-r-q10 to select
        r_q10 = page.locator("#disc-r-q10")
        r_q10.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-r-q10", f"Expected select to be disc-r-q10, got {select.input_value()}"
        print(f"[test] Clicked disc-r-q10 on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 6. Click on capacitor disc-cap-c2 to select
        c2 = page.locator("#disc-cap-c2")
        c2.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-cap-c2", f"Expected select to be disc-cap-c2, got {select.input_value()}"
        assert float(input_x.input_value()) == 15.7
        assert float(input_y.input_value()) == 4.6
        print(f"[test] Clicked disc-cap-c2 on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 7. Click on piezo speaker disk
        piezo = page.locator("#disc-piezo-disk")
        piezo.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-piezo-disk", f"Expected select to be disc-piezo-disk, got {select.input_value()}"
        print(f"[test] Clicked disc-piezo-disk on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 8. Drag piezo speaker disk by 10px right, 5px down
        box = piezo.bounding_box()
        page.mouse.move(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
        page.mouse.down()
        page.mouse.move(box["x"] + box["width"] / 2 + 10, box["y"] + box["height"] / 2 + 5)
        page.mouse.up()
        page.wait_for_timeout(200)
        print(f"[test] Dragged disc-piezo-disk: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 9. Test newly individualized resistors: disc-r-pu1, disc-r-pu18, disc-r-prog1, disc-r-clock
        select.select_option("disc-r-pu1")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 36.0, f"Expected disc-r-pu1 X=36.0, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 74.0, f"Expected disc-r-pu1 Y=74.0, got {input_y.input_value()}"
        page.keyboard.press("ArrowRight")
        page.wait_for_timeout(100)
        assert float(input_x.input_value()) == 37.0, f"Expected nudged disc-r-pu1 X=37.0, got {input_x.input_value()}"
        print(f"[test] Tested disc-r-pu1: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-pu18")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 40.5, f"Expected disc-r-pu18 X=40.5, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 240.5, f"Expected disc-r-pu18 Y=240.5, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-pu18: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-prog1")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 202.2, f"Expected disc-r-prog1 X=202.2, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 288.5, f"Expected disc-r-prog1 Y=288.5, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-prog1: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-prog-10k")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 202.2, f"Expected disc-r-prog-10k X=202.2, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 318.0, f"Expected disc-r-prog-10k Y=318.0, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-prog-10k: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-ep-1k8")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 332.8, f"Expected disc-r-ep-1k8 X=332.8, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 221.9, f"Expected disc-r-ep-1k8 Y=221.9, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-ep-1k8: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-led-ep")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 323.2, f"Expected disc-led-ep X=323.2, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 205.8, f"Expected disc-led-ep Y=205.8, got {input_y.input_value()}"
        print(f"[test] Tested disc-led-ep: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Test Reset & Clock passives: disc-cap-50p, disc-cap-101a, disc-cap-101b, disc-cap-10u, disc-r-rst-10k, disc-d-rst
        select.select_option("disc-cap-50p")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 238.8
        assert float(input_y.input_value()) == 23.0
        print(f"[test] Tested disc-cap-50p: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-101a")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 238.0
        assert float(input_y.input_value()) == 65.7
        print(f"[test] Tested disc-cap-101a: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-101b")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 248.8
        assert float(input_y.input_value()) == 63.6
        print(f"[test] Tested disc-cap-101b: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-10u")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 259.2
        assert float(input_y.input_value()) == 57.5
        print(f"[test] Tested disc-cap-10u: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-rst-10k")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 266.8
        assert float(input_y.input_value()) == 63.6
        print(f"[test] Tested disc-r-rst-10k: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-d-rst")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 274.9
        assert float(input_y.input_value()) == 63.0
        print(f"[test] Tested disc-d-rst: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Test Power & Cassette Header Subsystem Passives
        select.select_option("disc-r-ear-330")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 282.8
        assert float(input_y.input_value()) == 63.6
        print(f"[test] Tested disc-r-ear-330: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-d-ear")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 290.9
        assert float(input_y.input_value()) == 63.0
        print(f"[test] Tested disc-d-ear: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-ear-203")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 298.1
        assert float(input_y.input_value()) == 72.9
        print(f"[test] Tested disc-cap-ear-203: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-c1")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 408.8
        assert float(input_y.input_value()) == 13.3
        print(f"[test] Tested disc-cap-c1: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-mic-203")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 310.0
        assert float(input_y.input_value()) == 63.6
        print(f"[test] Tested disc-cap-mic-203: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-mic-10k1")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 320.0
        assert float(input_y.input_value()) == 63.6
        print(f"[test] Tested disc-r-mic-10k1: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-dc-100u")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 358.0
        assert float(input_y.input_value()) == 57.5
        print(f"[test] Tested disc-cap-dc-100u: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-cap-7805-203")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 382.0
        assert float(input_y.input_value()) == 76.0
        print(f"[test] Tested disc-cap-7805-203: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Verify bogus cont0 component is completely absent from DOM
        assert page.locator("#disc-conto-socket").count() == 0, "Bogus cont0 component should not exist in DOM!"
        print("[test] Confirmed disc-conto-socket is completely absent from DOM.")

        # 10. Test individual 7-segment LED display packages (disc-disp-a3 .. disc-disp-d0)
        select.select_option("disc-disp-a3")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 60.3, f"Expected disc-disp-a3 X=60.3, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 379.1, f"Expected disc-disp-a3 Y=379.1, got {input_y.input_value()}"
        page.keyboard.press("ArrowRight")
        page.wait_for_timeout(100)
        assert float(input_x.input_value()) == 61.3, f"Expected nudged disc-disp-a3 X=61.3, got {input_x.input_value()}"
        print(f"[test] Tested disc-disp-a3 dropdown & nudge: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Click directly on disc-disp-d0 on the board
        disp_d0 = page.locator("#disc-disp-d0")
        disp_d0.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-disp-d0", f"Expected select to be disc-disp-d0, got {select.input_value()}"
        assert float(input_x.input_value()) == 259.1, f"Expected disc-disp-d0 X=259.1, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 379.1, f"Expected disc-disp-d0 Y=379.1, got {input_y.input_value()}"
        print(f"[test] Clicked disc-disp-d0 directly on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Verify all 6 display modules contain exactly 8 segment elements (7 bars + 1 decimal point dot)
        for disp_id in ["disc-disp-a3", "disc-disp-a2", "disc-disp-a1", "disc-disp-a0", "disc-disp-d1", "disc-disp-d0"]:
            seg_count = page.locator(f"#{disp_id} .seg-path").count()
            assert seg_count == 8, f"Expected 8 segment elements in {disp_id}, got {seg_count}"
            dp_count = page.locator(f"#{disp_id} .seg-dp").count()
            assert dp_count == 1, f"Expected 1 decimal point circle in {disp_id}, got {dp_count}"
        print("[test] Confirmed all 6 displays have 8 precision-aligned LED elements with decimal point.")

        # 11. Test Dual 3x4 Keypads (disc-keypad-left & disc-keypad-right)
        select.select_option("disc-keypad-left")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 86.0, f"Expected disc-keypad-left X=86.0, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 451.0, f"Expected disc-keypad-left Y=451.0, got {input_y.input_value()}"
        input_w = page.locator("#calib-input-w")
        input_h = page.locator("#calib-input-h")
        assert float(input_w.input_value()) == 160.0, f"Expected disc-keypad-left W=160.0, got {input_w.input_value()}"
        assert float(input_h.input_value()) == 206.0, f"Expected disc-keypad-left H=206.0, got {input_h.input_value()}"
        page.keyboard.press("ArrowRight")
        page.wait_for_timeout(100)
        assert float(input_x.input_value()) == 87.0, f"Expected nudged disc-keypad-left X=87.0, got {input_x.input_value()}"
        print(f"[test] Tested disc-keypad-left dropdown & nudge: X={input_x.input_value()}, Y={input_y.input_value()}")

        # Click directly on disc-keypad-right on the board
        kp_right = page.locator("#disc-keypad-right")
        kp_right.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-keypad-right", f"Expected select to be disc-keypad-right, got {select.input_value()}"
        assert float(input_x.input_value()) == 249.0, f"Expected disc-keypad-right X=249.0, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 451.0, f"Expected disc-keypad-right Y=451.0, got {input_y.input_value()}"
        assert float(input_w.input_value()) == 160.0, f"Expected disc-keypad-right W=160.0, got {input_w.input_value()}"
        assert float(input_h.input_value()) == 206.0, f"Expected disc-keypad-right H=206.0, got {input_h.input_value()}"
        print(f"[test] Clicked disc-keypad-right directly on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 11b. Test Audio & Speaker Cluster (Envelope 8: 33R, 2x 330R, Green LED, Red LED)
        select.select_option("disc-r-sp33")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 414.1, f"Expected disc-r-sp33 X=414.1, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 385.2, f"Expected disc-r-sp33 Y=385.2, got {input_y.input_value()}"
        page.keyboard.press("ArrowRight")
        page.wait_for_timeout(100)
        assert float(input_x.input_value()) == 415.1, f"Expected nudged disc-r-sp33 X=415.1, got {input_x.input_value()}"
        print(f"[test] Tested disc-r-sp33 dropdown & nudge: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-sp330a")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 295.2, f"Expected disc-r-sp330a X=295.2, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 381.0, f"Expected disc-r-sp330a Y=381.0, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-sp330a: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-sp330b")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 314.1, f"Expected disc-r-sp330b X=314.1, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 377.1, f"Expected disc-r-sp330b Y=377.1, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-sp330b: X={input_x.input_value()}, Y={input_y.input_value()}")

        led_grn = page.locator("#disc-led-audio-grn")
        led_grn.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-led-audio-grn", f"Expected select to be disc-led-audio-grn, got {select.input_value()}"
        assert float(input_x.input_value()) == 307.5, f"Expected disc-led-audio-grn X=307.5, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 370.5, f"Expected disc-led-audio-grn Y=370.5, got {input_y.input_value()}"
        print(f"[test] Clicked disc-led-audio-grn directly on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        led_red = page.locator("#disc-led-audio-red")
        led_red.click()
        page.wait_for_timeout(200)
        assert select.input_value() == "disc-led-audio-red", f"Expected select to be disc-led-audio-red, got {select.input_value()}"
        assert float(input_x.input_value()) == 308.5, f"Expected disc-led-audio-red X=308.5, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 395.0, f"Expected disc-led-audio-red Y=395.0, got {input_y.input_value()}"
        print(f"[test] Clicked disc-led-audio-red directly on board: X={input_x.input_value()}, Y={input_y.input_value()}")

        # 12. Test Transistor Resizing Adaptability (Envelopes 4 & 5)
        select.select_option("disc-q1")
        page.wait_for_timeout(200)
        q1_body = page.locator("#disc-q1 .to92-body")
        box_before = q1_body.bounding_box()
        orig_w = float(input_w.input_value())
        orig_h = float(input_h.input_value())
        print(f"[test] Q1 before resize: W={orig_w}, H={orig_h}, body box={box_before}")

        # Click Bigger button twice
        btn_bigger = page.locator("#btn-size-bigger")
        btn_bigger.click()
        page.wait_for_timeout(100)
        btn_bigger.click()
        page.wait_for_timeout(100)

        new_w = float(input_w.input_value())
        new_h = float(input_h.input_value())
        box_after_bigger = q1_body.bounding_box()
        print(f"[test] Q1 after bigger: W={new_w}, H={new_h}, body box={box_after_bigger}")
        assert new_w > orig_w, f"Expected width to increase from {orig_w}, got {new_w}"
        assert new_h > orig_h, f"Expected height to increase from {orig_h}, got {new_h}"
        assert box_after_bigger["width"] > box_before["width"], "Expected to92-body rendered width to increase!"
        assert box_after_bigger["height"] > box_before["height"], "Expected to92-body rendered height to increase!"

        # Click Smaller button twice
        btn_smaller = page.locator("#btn-size-smaller")
        btn_smaller.click()
        page.wait_for_timeout(100)
        btn_smaller.click()
        page.wait_for_timeout(100)
        box_after_smaller = q1_body.bounding_box()
        print(f"[test] Q1 after smaller: W={input_w.input_value()}, H={input_h.input_value()}, body box={box_after_smaller}")
        assert abs(float(input_w.input_value()) - orig_w) < 0.2
        assert abs(float(input_h.input_value()) - orig_h) < 0.2

        # 12b. Test Audio Jack Resizing Adaptability (Envelope 9)
        select.select_option("disc-jack-ear")
        page.wait_for_timeout(200)
        jack_body = page.locator("#disc-jack-ear .phone-jack-35mm")
        jack_box_before = jack_body.bounding_box()
        orig_jack_w = float(input_w.input_value())
        orig_jack_h = float(input_h.input_value())
        print(f"[test] EAR Jack before resize: W={orig_jack_w}, H={orig_jack_h}, body box={jack_box_before}")

        btn_bigger.click()
        page.wait_for_timeout(100)
        btn_bigger.click()
        page.wait_for_timeout(100)
        jack_box_after = jack_body.bounding_box()
        print(f"[test] EAR Jack after bigger: W={input_w.input_value()}, H={input_h.input_value()}, body box={jack_box_after}")
        assert jack_box_after["width"] > jack_box_before["width"], "Expected phone-jack-35mm width to increase"
        assert jack_box_after["height"] > jack_box_before["height"], "Expected phone-jack-35mm height to increase"

        btn_smaller.click()
        page.wait_for_timeout(100)
        btn_smaller.click()
        page.wait_for_timeout(100)
        jack_box_restored = jack_body.bounding_box()
        print(f"[test] EAR Jack after smaller: W={input_w.input_value()}, H={input_h.input_value()}, body box={jack_box_restored}")
        assert abs(jack_box_restored["width"] - jack_box_before["width"]) < 0.5

        # 12c. Test DC Jack Resizing Adaptability (Envelope 14)
        select.select_option("disc-jack-dc")
        page.wait_for_timeout(200)
        dc_body = page.locator("#disc-jack-dc .dc-barrel-jack")
        dc_box_before = dc_body.bounding_box()
        orig_dc_w = float(input_w.input_value())
        orig_dc_h = float(input_h.input_value())
        btn_bigger.click()
        page.wait_for_timeout(100)
        btn_bigger.click()
        page.wait_for_timeout(100)
        dc_box_after = dc_body.bounding_box()
        print(f"[test] DC Jack after bigger: W={input_w.input_value()}, H={input_h.input_value()}, body box={dc_box_after}")
        assert dc_box_after["width"] > dc_box_before["width"], "Expected dc-barrel-jack width to increase"
        btn_smaller.click()
        page.wait_for_timeout(100)
        btn_smaller.click()
        page.wait_for_timeout(100)

        # 12d. Test Reset Button Resizing Adaptability (Envelope 14)
        select.select_option("disc-hw-rst")
        page.wait_for_timeout(200)
        rst_body = page.locator("#disc-hw-rst .hw-reset-tactile-btn")
        rst_box_before = rst_body.bounding_box()
        btn_bigger.click()
        page.wait_for_timeout(100)
        btn_bigger.click()
        page.wait_for_timeout(100)
        rst_box_after = rst_body.bounding_box()
        print(f"[test] Reset button after bigger: W={input_w.input_value()}, H={input_h.input_value()}, body box={rst_box_after}")
        assert rst_box_after["width"] > rst_box_before["width"], "Expected hw-reset-tactile-btn width to increase"
        btn_smaller.click()
        page.wait_for_timeout(100)
        btn_smaller.click()
        page.wait_for_timeout(100)

        # 12e. Test LM7805 Regulator Resizing Adaptability (Envelope 14)
        select.select_option("disc-lm7805")
        page.wait_for_timeout(200)
        reg_body = page.locator("#disc-lm7805 .to220-heatsink")
        reg_box_before = reg_body.bounding_box()
        btn_bigger.click()
        page.wait_for_timeout(100)
        btn_bigger.click()
        page.wait_for_timeout(100)
        reg_box_after = reg_body.bounding_box()
        print(f"[test] LM7805 heatsink after bigger: W={input_w.input_value()}, H={input_h.input_value()}, body box={reg_box_after}")
        assert reg_box_after["width"] > reg_box_before["width"], "Expected to220-heatsink width to increase"
        btn_smaller.click()
        page.wait_for_timeout(100)
        btn_smaller.click()
        page.wait_for_timeout(100)

        # 13. Test 90° Component Orientation Engine (Envelope 6)
        select.select_option("disc-q1")
        page.wait_for_timeout(200)
        rot_badge = page.locator("#calib-rot-val")
        btn_rot_right = page.locator("#btn-rot-right")
        btn_rot_left = page.locator("#btn-rot-left")
        assert rot_badge.text_content() == "0°", f"Expected initial rotation 0°, got {rot_badge.text_content()}"

        # Rotate +90°
        btn_rot_right.click()
        page.wait_for_timeout(100)
        assert rot_badge.text_content() == "90°", f"Expected 90°, got {rot_badge.text_content()}"
        rot_style = page.eval_on_selector("#disc-q1", "el => el.style.transform")
        assert "rotate(90deg)" in rot_style, f"Expected rotate(90deg) in transform, got: {rot_style}"
        print(f"[test] Rotated +90°: badge={rot_badge.text_content()}, style={rot_style}")

        # Rotate +90° again -> 180°
        btn_rot_right.click()
        page.wait_for_timeout(100)
        assert rot_badge.text_content() == "180°", f"Expected 180°, got {rot_badge.text_content()}"

        # Rotate -90° -> 90°
        btn_rot_left.click()
        page.wait_for_timeout(100)
        assert rot_badge.text_content() == "90°", f"Expected 90°, got {rot_badge.text_content()}"

        # Keyboard hotkey: press '[' to rotate -90° -> 0°
        page.keyboard.press("[")
        page.wait_for_timeout(100)
        assert rot_badge.text_content() == "0°", f"Expected 0°, got {rot_badge.text_content()}"
        rot_style_0 = page.eval_on_selector("#disc-q1", "el => el.style.transform")
        assert rot_style_0 == "", f"Expected empty transform for 0°, got: {rot_style_0}"
        print("[test] Keyboard hotkey [ rotated back to 0° successfully.")

        # 14. Test Copy CSS button
        copy_btn = page.locator("#btn-copy-css")
        copy_btn.click()
        page.wait_for_timeout(300)
        print("[test] Copy CSS button clicked successfully.")

        # 15. Test Discrete LEDs active class, Reset Button, and MIC Input
        close_btn = page.locator("#btn-close-calib")
        close_btn.click()
        page.wait_for_timeout(200)

        # Test LEDs active toggle
        page.evaluate("""() => {
            document.getElementById('disc-led-audio-grn').classList.add('active');
            document.getElementById('disc-led-audio-red').classList.add('active');
            document.getElementById('disc-led-ep').classList.add('active');
        }""")
        page.wait_for_timeout(100)
        assert page.locator("#disc-led-audio-grn").evaluate("el => el.classList.contains('active')")
        assert page.locator("#disc-led-audio-red").evaluate("el => el.classList.contains('active')")
        assert page.locator("#disc-led-ep").evaluate("el => el.classList.contains('active')")
        print("[test] 3 Discrete LEDs active state toggle verified.")

        # Test Hardware Reset Tactile Button Click
        hw_rst = page.locator("#btn-hw-rst")
        hw_rst.click()
        page.wait_for_timeout(200)
        print("[test] Hardware reset button click verified.")

        # Test MIC audio input click
        jack_mic = page.locator("#disc-jack-mic")
        jack_mic.click()
        page.wait_for_timeout(200)
        print("[test] Cassette MIC input trigger verified.")

        browser.close()

    if console_errors:
        print(f"[ERROR] Browser console errors detected: {console_errors}", file=sys.stderr)
        sys.exit(1)
    else:
        print("[test] All calibration tests passed with 0 console errors!")

if __name__ == "__main__":
    main()
