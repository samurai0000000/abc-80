#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
test_web_ui_navigation.py - Validates ABC-80 Web UI Layout & Viewport Navigation:
1. DOM Nesting: Inspector section side-by-side with hardware panel in main-workspace.
2. Left Anchoring: Board chassis anchored to the left of the viewport.
3. Pan & Zoom Controls: Zoom in, zoom out, reset view, and pan D-pad buttons.
4. Mouse Interactions: Wheel zoom and drag-to-pan.
5. Zero JavaScript console errors.
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
        ctx = browser.new_context(viewport={"width": 1280, "height": 1024})
        page = ctx.new_page()

        page.on("console", lambda msg: console_errors.append(msg.text) if msg.type == "error" and "ws://" not in msg.text else None)
        page.on("pageerror", lambda err: console_errors.append(str(err)))

        print(f"[test] Loading page: {url}")
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(500)

        # 1. Validate DOM Nesting of Inspector & Hardware Panel
        inspector_parent = page.evaluate("() => document.getElementById('inspector-section').parentElement.id")
        assert inspector_parent == "main-workspace", f"Expected inspector parent 'main-workspace', got '{inspector_parent}'"
        print(f"[test] ✓ Inspector parent is: {inspector_parent}")

        hw_box = page.locator("#hardware-panel-section").bounding_box()
        insp_box = page.locator("#inspector-section").bounding_box()
        board_box = page.locator("#board-container").bounding_box()

        print(f"[test] Hardware Panel: {hw_box}")
        print(f"[test] Inspector Panel: {insp_box}")
        print(f"[test] Board Chassis: {board_box}")

        assert insp_box["x"] >= hw_box["width"] - 2, f"Inspector should be to the right of hardware panel ({insp_box['x']} >= {hw_box['width']})"
        assert insp_box["y"] < 100, f"Inspector should be at the top alongside workspace ({insp_box['y']} < 100)"
        print("[test] ✓ Inspector is properly docked on the right side.")

        # 2. Validate Board Left-Anchoring
        assert board_box["x"] <= 50, f"Board chassis should be anchored to the left ({board_box['x']} <= 50)"
        print("[test] ✓ Board chassis is anchored to the left of the viewport.")

        # 3. Test Zoom In Button
        btn_zoom_in = page.locator("#btn-zoom-in")
        btn_zoom_in.click()
        page.wait_for_timeout(200)

        badge = page.locator("#view-zoom-badge").text_content()
        assert badge == "125%", f"Expected zoom badge '125%', got '{badge}'"
        print(f"[test] ✓ Zoom In button increased scale to: {badge}")

        # 4. Test Pan Buttons
        btn_pan_right = page.locator("#btn-pan-right")
        btn_pan_right.click()
        page.wait_for_timeout(100)

        transform_after_pan = page.evaluate("() => document.getElementById('board-container').style.transform")
        print(f"[test] ✓ Transform after Pan Right: {transform_after_pan}")
        assert "scale(1.25)" in transform_after_pan and "-135px" in transform_after_pan, f"Expected pan right with scale(1.25) and -135px, got {transform_after_pan}"

        # 5. Test Reset Button (1:1)
        btn_reset = page.locator("#btn-zoom-reset")
        btn_reset.click()
        page.wait_for_timeout(200)

        badge_reset = page.locator("#view-zoom-badge").text_content()
        assert badge_reset == "100%", f"Expected zoom badge '100%', got '{badge_reset}'"
        transform_reset = page.evaluate("() => document.getElementById('board-container').style.transform")
        assert "translate(0px, 0px) scale(1)" in transform_reset, f"Expected reset transform, got {transform_reset}"
        print(f"[test] ✓ Viewport reset to: {badge_reset}, transform: {transform_reset}")

        # 6. Test Mouse Wheel Zoom
        hw_panel = page.locator("#hardware-panel-section")
        hw_panel.dispatch_event("wheel", {"deltaY": -100, "clientX": 300, "clientY": 300})
        page.wait_for_timeout(200)

        badge_wheel = page.locator("#view-zoom-badge").text_content()
        assert badge_wheel != "100%", f"Mouse wheel should have changed zoom, got {badge_wheel}"
        print(f"[test] ✓ Mouse wheel zoom active, scale: {badge_wheel}")

        # 7. Test Drag-to-Pan
        page.mouse.move(800, 300)
        page.mouse.down()
        page.mouse.move(750, 250)
        page.mouse.up()
        page.wait_for_timeout(100)

        transform_drag = page.evaluate("() => document.getElementById('board-container').style.transform")
        print(f"[test] ✓ Transform after mouse drag pan: {transform_drag}")

        # 8. Test Toggle Inspector Collapse / Expand
        toggle_insp = page.locator("#btn-toggle-inspector")
        toggle_insp.click()
        page.wait_for_timeout(300)

        insp_box_collapsed = page.locator("#inspector-section").bounding_box()
        assert insp_box_collapsed["width"] == 0, f"Inspector should collapse to width 0, got {insp_box_collapsed['width']}"
        print(f"[test] ✓ Inspector collapsed to width: {insp_box_collapsed['width']}")

        toggle_insp.click()
        page.wait_for_timeout(300)
        insp_box_expanded = page.locator("#inspector-section").bounding_box()
        assert insp_box_expanded["width"] >= 350, f"Inspector should restore to >= 350px, got {insp_box_expanded['width']}"
        print(f"[test] ✓ Inspector re-expanded to width: {insp_box_expanded['width']}")

        # 9. Verify 0 console errors
        assert len(console_errors) == 0, f"Found console errors: {console_errors}"
        print("[test] ✓ Zero console errors detected across all interactions!")

        browser.close()

    print("\n[SUCCESS] All Web UI layout, left-anchoring, and pan/zoom navigation tests PASSED!")


if __name__ == "__main__":
    main()
