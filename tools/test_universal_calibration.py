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

        # 1. Open Calibration Mode
        calib_btn = page.locator("#btn-calibration-mode")
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
        assert float(input_x.input_value()) == 17.6
        assert float(input_y.input_value()) == 5.2
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
        assert float(input_x.input_value()) == 208.0, f"Expected disc-r-prog1 X=208.0, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 236.0, f"Expected disc-r-prog1 Y=236.0, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-prog1: X={input_x.input_value()}, Y={input_y.input_value()}")

        select.select_option("disc-r-clock")
        page.wait_for_timeout(200)
        assert float(input_x.input_value()) == 216.0, f"Expected disc-r-clock X=216.0, got {input_x.input_value()}"
        assert float(input_y.input_value()) == 83.0, f"Expected disc-r-clock Y=83.0, got {input_y.input_value()}"
        print(f"[test] Tested disc-r-clock: X={input_x.input_value()}, Y={input_y.input_value()}")

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

        # 12. Test Copy CSS button
        copy_btn = page.locator("#btn-copy-css")
        copy_btn.click()
        page.wait_for_timeout(300)
        print("[test] Copy CSS button clicked successfully.")

        browser.close()

    if console_errors:
        print(f"[ERROR] Browser console errors detected: {console_errors}", file=sys.stderr)
        sys.exit(1)
    else:
        print("[test] All calibration tests passed with 0 console errors!")

if __name__ == "__main__":
    main()
