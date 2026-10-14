#
# test_ai_hand.py
#
# Tests for AI Hand cursor animation, human operator keypad lockout,
# and Cancel AI Actions button override.
#
# Copyright (C) 2026, Charles Chiou
#

import time
from conftest import Panel, wait_for


def test_ai_hand_cursor_dom_and_initial_state(panel):
    panel.open()
    cursor = panel.page.locator("#ai-hand-cursor")
    assert cursor.count() == 1, "Expected #ai-hand-cursor element in DOM"
    assert "ai-hand-hidden" in (cursor.get_attribute("class") or "")

    cancel_btn = panel.page.locator("#cancel-ai-btn")
    assert cancel_btn.count() == 1, "Expected #cancel-ai-btn in DOM"
    assert cancel_btn.is_disabled(), "Cancel AI button should initially be disabled"


def test_ai_punch_event_animates_hand_and_locks_operator(panel):
    panel.open()
    assert not panel.page.evaluate("window.__abc80.isAiLocked")

    # Simulate aiPunch down on ADRS key with remaining 2 keys
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "ADRS",
            down: true,
            remaining: 2
        });
    """)

    wait_for(lambda: panel.page.evaluate("window.__abc80.isAiLocked"), timeout=3, what="operator lockout active")
    assert panel.page.evaluate("window.__abc80.isAiHandVisible"), "AI Hand should be visible"
    cancel_btn = panel.page.locator("#cancel-ai-btn")
    assert not cancel_btn.is_disabled(), "Cancel AI button should be enabled during punch"

    btn = panel.page.locator('button.key-btn[data-key="ADRS"]')
    assert "ai-punched" in (btn.get_attribute("class") or "")

    # Simulate key release
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "ADRS",
            down: false,
            remaining: 2
        });
    """)
    assert "ai-punched" not in (btn.get_attribute("class") or "")
    assert panel.page.evaluate("window.__abc80.isAiLocked"), "Still locked while remaining > 0"

    # Simulate final key sequence complete (remaining 0, down false)
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "1",
            down: false,
            remaining: 0
        });
    """)

    wait_for(lambda: not panel.page.evaluate("window.__abc80.isAiLocked"), timeout=3, what="operator lockout removed")
    assert cancel_btn.is_disabled(), "Cancel AI button disabled after sequence completes"


def test_cancel_ai_actions_button_clears_lockout(panel):
    panel.open()

    # Trigger active punching state
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "GO",
            down: true,
            remaining: 5
        });
    """)
    assert panel.page.evaluate("window.__abc80.isAiLocked")
    cancel_btn = panel.page.locator("#cancel-ai-btn")
    assert not cancel_btn.is_disabled()

    # Click Cancel AI Actions button
    cancel_btn.click()

    wait_for(lambda: not panel.page.evaluate("window.__abc80.isAiLocked"), timeout=3, what="operator lockout cleared by cancel")
    assert cancel_btn.is_disabled()
    assert not panel.page.evaluate("window.__abc80.isAiHandVisible")


def test_ai_punch_rst_button_and_full_operator_lockout(panel):
    panel.open()

    # Trigger active punching state on RST tactile button
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "RST",
            down: true,
            remaining: 1
        });
    """)

    assert panel.page.evaluate("window.__abc80.isAiLocked")
    rst_btn = panel.page.locator("#btn-hw-rst")
    assert "ai-punched" in (rst_btn.get_attribute("class") or "")
    assert rst_btn.is_disabled()
    assert panel.page.locator("#btn-cpu-run").is_disabled()
    assert panel.page.locator("#btn-cpu-pause").is_disabled()
    assert panel.page.locator("#btn-cpu-step").is_disabled()
    assert panel.page.locator("#rom-select").is_disabled()

    # Physical keyboard F6 (pause) must be ignored while locked
    panel.page.keyboard.press("F6")
    time.sleep(0.1)
    assert not panel.page.evaluate("window.__abc80.isPaused")

    # Release RST and finish sequence
    panel.page.evaluate("""
        window.__abc80.handleAiPunch({
            type: "aiPunch",
            key: "RST",
            down: false,
            remaining: 0
        });
    """)
    wait_for(lambda: not panel.page.evaluate("window.__abc80.isAiLocked"), timeout=3, what="operator lockout cleared after RST")
    assert "ai-punched" not in (rst_btn.get_attribute("class") or "")
    assert not rst_btn.is_disabled()

