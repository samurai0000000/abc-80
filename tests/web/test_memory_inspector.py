#
# test_memory_inspector.py
#
# Automated Playwright tests for stacked Memory Map windows:
# - General Memory Hex Viewer (offset jumping, quick chips, pagination)
# - Stack Memory Window (SP tracking, TOS indicator, auto-reanchoring, depth)
#
# Copyright (C) 2026, Charles Chiou
#

import pytest
from conftest import wait_for


def switch_to_memory_tab(page):
    page.click("button[data-tab='tab-memory']")
    wait_for(lambda: page.locator("#tab-memory").is_visible(), what="tab-memory active")


def test_memory_viewer_elements_and_quick_chips(panel):
    panel.open()
    page = panel.page

    # Switch to Memory Map tab
    switch_to_memory_tab(page)

    # Verify elements exist
    assert page.locator("#mem-offset-input").count() == 1
    assert page.locator("#btn-mem-jump").count() == 1
    assert page.locator("#btn-mem-prev").count() == 1
    assert page.locator("#btn-mem-next").count() == 1
    assert page.locator("#hex-editor-table").count() == 1

    assert page.locator("#stack-sp-val").count() == 1
    assert page.locator("#stack-depth-val").count() == 1
    assert page.locator("#stack-anchor-input").count() == 1
    assert page.locator("#btn-stack-anchor").count() == 1
    assert page.locator("#stack-editor-table").count() == 1

    # Verify quick chip labels (including RAM 2000H label)
    chip_texts = [page.locator(".mem-jump-bar .chip-btn").nth(i).inner_text().strip()
                  for i in range(page.locator(".mem-jump-bar .chip-btn").count())]
    assert "ROM 0000H" in chip_texts
    assert "RAM 1000H" in chip_texts
    assert "RAM 2000H" in chip_texts

    # Wait for initial memory fetch (0x0000)
    wait_for(lambda: page.locator("#hex-editor-table .hex-row").count() > 0, what="hex rows loaded")
    first_addr = page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip()
    assert first_addr == "0000:"

    # Click RAM 1000H chip
    page.click("button.chip-btn[data-addr='0x1000']")
    wait_for(lambda: page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "1000:",
             what="RAM 1000H rows")
    assert page.locator("#mem-offset-input").input_value() == "1000"

    # Click RAM 2000H chip
    page.click("button.chip-btn[data-addr='0x2000']")
    wait_for(lambda: page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "2000:",
             what="RAM 2000H rows")
    assert page.locator("#mem-offset-input").input_value() == "2000"


def test_memory_viewer_offset_input_and_jump(panel):
    panel.open()
    page = panel.page
    switch_to_memory_tab(page)

    # Enter custom offset and jump
    page.fill("#mem-offset-input", "1500")
    page.click("#btn-mem-jump")
    wait_for(lambda: page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "1500:",
             what="custom offset 1500H rows")

    # Next page (+128 bytes -> 0x1580)
    page.click("#btn-mem-next")
    wait_for(lambda: page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "1580:",
             what="next page 1580H rows")

    # Prev page (-128 bytes -> 0x1500)
    page.click("#btn-mem-prev")
    wait_for(lambda: page.locator("#hex-editor-table .hex-row .hex-addr").first.inner_text().strip() == "1500:",
             what="prev page 1500H rows")


def test_stack_window_sp_tracking_and_tos(panel):
    panel.open()
    page = panel.page

    # Pause CPU on CPU tab to get paused telemetry with stackBytes
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry with stackBytes")
    switch_to_memory_tab(page)

    # Verify stack rows rendered
    wait_for(lambda: page.locator("#stack-editor-table .stack-row").count() > 0, what="stack rows")
    top_row = page.locator("#stack-editor-table .stack-row").first
    assert "tos" in top_row.get_attribute("class")
    assert "▶ TOS" in top_row.locator(".stack-marker").inner_text()
    assert "(SP+00)" in top_row.locator(".stack-rel").inner_text()

    # Verify SP display matches telemetry SP
    t = panel.telemetry()
    sp_hex = "0x%04X" % t["registers"]["sp"]
    assert page.inner_text("#stack-sp-val").strip() == sp_hex


def test_stack_anchor_and_depth_controls(panel):
    panel.open()
    page = panel.page

    # Pause CPU
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry")
    switch_to_memory_tab(page)

    t = panel.telemetry()
    sp = t["registers"]["sp"]

    # Set anchor higher by 4 bytes (2 words depth)
    new_anchor = sp + 4
    page.fill("#stack-anchor-input", "%04X" % new_anchor)
    page.click("#btn-stack-anchor")

    # Verify depth recalculated to 2 words
    wait_for(lambda: page.inner_text("#stack-depth-val").strip() == "2", what="depth = 2 words")

    # Click anchor with current SP sets depth to 0 words
    page.fill("#stack-anchor-input", "%04X" % sp)
    page.click("#btn-stack-anchor")
    wait_for(lambda: page.inner_text("#stack-depth-val").strip() == "0", what="depth = 0 words")


def test_stack_auto_rebase_guarded_when_running(panel):
    panel.open()
    page = panel.page

    # Pause CPU and open Memory Map tab
    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry")
    switch_to_memory_tab(page)

    t = panel.telemetry()
    initial_sp = t["registers"]["sp"]

    # Ensure anchor is at initial SP
    page.fill("#stack-anchor-input", "%04X" % initial_sp)
    page.click("#btn-stack-anchor")
    assert page.evaluate("window.__abc80.stackState.anchorSp") == initial_sp

    # 1. SP changes while running (isPaused = false) must NOT re-anchor
    page.evaluate("(sp) => window.__abc80.updateStackView(sp, [0x00, 0x00], sp, false)", initial_sp - 80)
    assert page.evaluate("window.__abc80.stackState.anchorSp") == initial_sp, "Must not re-anchor when running"

    # 2. SP changes by >64 bytes while paused (isPaused = true) MUST auto-rebase
    # Delta from lastSp (initial_sp - 80) to (initial_sp - 180) is 100 > 64
    page.evaluate("(sp) => window.__abc80.updateStackView(sp, [0x00, 0x00], sp, true)", initial_sp - 180)
    assert page.evaluate("window.__abc80.stackState.anchorSp") == initial_sp - 180, "Must auto-rebase on large delta when paused"

    # 3. SP moves above anchor while paused MUST auto-rebase
    page.evaluate("(sp) => window.__abc80.updateStackView(sp, [0x00, 0x00], sp, true)", initial_sp + 16)
    assert page.evaluate("window.__abc80.stackState.anchorSp") == initial_sp + 16, "Must auto-rebase when SP moves above anchor"


def test_stack_depth_and_tos_rows_on_stack_growth(panel):
    panel.open()
    page = panel.page

    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry")
    switch_to_memory_tab(page)

    t = panel.telemetry()
    anchor = t["registers"]["sp"]
    page.fill("#stack-anchor-input", "%04X" % anchor)
    page.click("#btn-stack-anchor")

    # Simulate 1 word pushed (SP drops by 2 bytes: anchor -> anchor - 2)
    sp1 = anchor - 2
    page.evaluate("([sp, b]) => window.__abc80.updateStackView(sp, b, sp, true)",
                  [sp1, [0x34, 0x12, 0x78, 0x56]])
    wait_for(lambda: page.inner_text("#stack-depth-val").strip() == "1", what="stack depth = 1 word")

    rows = page.locator("#stack-editor-table .stack-row")
    assert rows.count() >= 2
    row0 = rows.nth(0)
    assert "▶ TOS" in row0.locator(".stack-marker").inner_text()
    assert ("%04X:" % sp1) in row0.locator(".stack-addr").inner_text()
    assert "(SP+00)" in row0.locator(".stack-rel").inner_text()

    row1 = rows.nth(1)
    assert ("%04X:" % anchor) in row1.locator(".stack-addr").inner_text()
    assert "(SP+02)" in row1.locator(".stack-rel").inner_text()

    # Simulate 3 words pushed (SP = anchor - 6)
    sp3 = anchor - 6
    bytes_arr = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x78, 0x56]
    page.evaluate("([sp, b]) => window.__abc80.updateStackView(sp, b, sp, true)", [sp3, bytes_arr])
    wait_for(lambda: page.inner_text("#stack-depth-val").strip() == "3", what="stack depth = 3 words")
    assert "▶ TOS" in rows.nth(0).locator(".stack-marker").inner_text()
    assert "(SP+00)" in rows.nth(0).locator(".stack-rel").inner_text()
    assert "(SP+02)" in rows.nth(1).locator(".stack-rel").inner_text()
    assert "(SP+04)" in rows.nth(2).locator(".stack-rel").inner_text()
    assert "(SP+06)" in rows.nth(3).locator(".stack-rel").inner_text()


def test_tab_memory_zero_scrollbar_and_compact_stack_header(panel):
    """Enforces Directives 11 & 12: tab-memory has no outer scrollbar, stack header is compact single-line."""
    panel.open()
    page = panel.page

    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry")
    switch_to_memory_tab(page)

    # 1. Verify tab-memory does not overflow vertically
    scroll_h, client_h = page.evaluate(
        "() => [document.getElementById('tab-memory').scrollHeight, document.getElementById('tab-memory').clientHeight]")
    assert scroll_h <= client_h + 2, f"tab-memory has vertical scrollbar: scrollHeight={scroll_h}, clientHeight={client_h}"

    # 2. Verify stack-editor-table does not overflow vertically
    s_scroll, s_client = page.evaluate(
        "() => [document.getElementById('stack-editor-table').scrollHeight, document.getElementById('stack-editor-table').clientHeight]")
    assert s_scroll <= s_client + 2, f"stack-editor-table has vertical scrollbar: scrollHeight={s_scroll}, clientHeight={s_client}"

    # 3. Verify STACK MEMORY (SP) title and Depth badge do not wrap into multi-line blocks
    title_box = page.locator(".stack-box .box-title").bounding_box()
    badge_box = page.locator("#stack-depth-badge").bounding_box()
    assert title_box is not None and title_box["height"] < 25, f"Title wrapped unexpectedly: height={title_box['height']}"
    assert badge_box is not None and badge_box["height"] < 25, f"Badge wrapped unexpectedly: height={badge_box['height']}"


def test_stack_rebase_on_large_jump_while_paused(panel):
    """Verifies that while paused, an SP jump > 64 bytes triggers automatic re-anchoring."""
    panel.open()
    page = panel.page

    page.click("#btn-cpu-pause")
    panel.wait_telemetry(lambda t: t.get("paused") is True and len(t.get("stackBytes", [])) > 0,
                         what="paused telemetry")
    switch_to_memory_tab(page)

    init_anchor = int(page.input_value("#stack-anchor-input"), 16)

    # Simulate a large SP jump: anchor - 100 bytes (> 64)
    new_sp = (init_anchor - 100) & 0xFFFF
    page.evaluate("([sp, b]) => window.__abc80.updateStackView(sp, b, sp, true)",
                  [new_sp, [0x00, 0x00, 0x00, 0x00]])

    # Anchor must rebase to new_sp, resetting depth to 0
    wait_for(lambda: int(page.input_value("#stack-anchor-input"), 16) == new_sp,
             what="anchor rebased on > 64-byte jump while paused")
    assert page.inner_text("#stack-depth-val").strip() == "0"

