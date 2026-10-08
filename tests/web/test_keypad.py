#
# test_keypad.py
#
# The 24-key keypad and reset through the page: real clicks and key presses go over the
# WebSocket to a real board running a real ROM (plan scenarios A2, A3, A5, A9).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def test_a2_address_entry_by_clicking_keys(panel):
    panel.open()
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="start display")
    panel.type_keys("ADRS", "1", "2", "3", "4")
    panel.wait_digits(lambda d: d.startswith("1234"), what="address 1234 on the display")


def test_a3_data_entry_and_stepping(panel):
    panel.open()
    panel.type_keys("ADRS", "2", "5", "0", "0")
    panel.wait_digits(lambda d: d.startswith("2500"), what="address 2500")
    panel.type_keys("DATA", "C", "3")
    panel.wait_digits(lambda d: d.endswith("C3"), what="data C3")  # 'C' has the same glyph in both ROMs
    panel.type_keys("+")
    panel.wait_digits(lambda d: d.startswith("2501"), what="address stepped to 2501")
    panel.type_keys("-")
    panel.wait_digits(lambda d: d.startswith("2500"), what="address stepped back to 2500")
    panel.wait_digits(lambda d: d.endswith("C3"), what="the byte stored at 2500 reads back as C3")


def test_a5_fast_clicks_lose_no_keys(panel):
    panel.open()
    panel.type_keys("ADRS", delay=20)
    # Ten digits clicked with a 20 ms hold and no pause between them: the queue spaces them for the ROM.
    panel.type_keys("1", "2", "3", "4", "5", "6", "7", "8", "9", "0", delay=20)
    panel.wait_digits(lambda d: d.startswith("7890"), timeout=20, what="the last four of ten fast digits")


def test_keyboard_shortcuts(panel):
    panel.open()
    panel.page.keyboard.press("m")
    for ch in "1234":
        panel.page.keyboard.press(ch)
    panel.wait_digits(lambda d: d.startswith("1234"), what="address typed on the keyboard")
    panel.page.keyboard.press("n")
    panel.page.keyboard.press("9")
    panel.page.keyboard.press("0")
    panel.wait_digits(lambda d: d.endswith("90"), what="data 90 typed on the keyboard")
    panel.page.keyboard.press("+")
    panel.wait_digits(lambda d: d.startswith("1235"), what="'+' steps the address")
    panel.page.keyboard.press("-")
    panel.wait_digits(lambda d: d.startswith("1234"), what="'-' steps back")


def test_a9_reset_button_restarts_the_rom(panel):
    panel.open()
    panel.type_keys("ADRS", "1", "2", "3", "4")
    panel.wait_digits(lambda d: d.startswith("1234"), what="address typed")
    panel.page.click("#btn-hw-rst", delay=50)
    wait_for(lambda: not panel.lit(), what="the display going dark while the reset chime plays")
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), timeout=20, what="display back after the chime")
    assert not panel.digits().startswith("1234")


def test_a9_escape_and_the_keypad_rst_key_also_reset_and_are_not_matrix_keys(panel):
    panel.open()
    for press in (lambda: panel.page.keyboard.press("Escape"),
                  lambda: panel.page.click('button.key-rst', delay=50)):
        panel.type_keys("ADRS", "7")
        panel.wait_digits(lambda d: "7" in d, what="a digit typed")
        press()
        wait_for(lambda: not panel.lit(), what="dark after reset")
        panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), timeout=20, what="display back after reset")
        assert "7" not in panel.digits()


def test_keypad_keys_show_no_hover_text_but_other_components_still_do(panel):
    panel.open()
    page = panel.page
    box = "document.getElementById('component-tooltip-box')"
    for key in ("1", "ADRS", "GO EXEC"):
        page.hover('button[data-key="%s"]' % key)
        page.wait_for_timeout(300)
        assert page.evaluate("getComputedStyle(%s).display" % box) == "none", key
        assert page.get_attribute('button[data-key="%s"]' % key, "title") is None, key
        assert page.get_attribute('button[data-key="%s"]' % key, "aria-label"), key  # still has an accessible name
    # The bezel around the keys and the gaps between them: no custom box, no native title either.
    for module in ("#disc-keypad-left", "#disc-keypad-right"):
        page.hover(module, position={"x": 3, "y": 3})
        page.wait_for_timeout(300)
        assert page.evaluate("getComputedStyle(%s).display" % box) == "none", module
        assert page.get_attribute(module, "title") is None, module
    page.hover("#disc-led-ep")
    wait_for(lambda: page.evaluate("getComputedStyle(%s).display" % box) == "block", timeout=3,
             what="the tooltip on a non-keypad component")
    page.hover('button[data-key="5"]')
    wait_for(lambda: page.evaluate("getComputedStyle(%s).display" % box) == "none", timeout=3,
             what="the tooltip hiding when the mouse moves onto a key")


def test_hex_a_and_f_show_the_right_glyphs_on_the_book_monitor(panel):
    """Regression: rom.asm once had wrong font bytes, so hex A showed as 6 and F as a wrong glyph."""
    panel.open()
    panel.wait_digits(lambda d: d == "AbC-80", what="the start screen reading AbC-80")
    panel.type_keys("ADRS", "A", "B", "C", "D")
    panel.wait_digits(lambda d: d.startswith("AbCd"), what="address ABCD")
    panel.type_keys("DATA", "E", "F")
    panel.wait_digits(lambda d: d.endswith("EF"), what="data EF")
    panel.type_keys("ADRS", "F", "A", "F", "A")
    panel.wait_digits(lambda d: d.startswith("FAFA"), what="address FAFA")


def test_no_hover_box_anywhere_over_the_keypad_area(panel):
    """Sweeps the mouse over a grid covering both keypad modules, bezels and the gaps between keys."""
    panel.open()
    page = panel.page
    box = "document.getElementById('component-tooltip-box')"
    rects = page.evaluate("""['disc-keypad-left', 'disc-keypad-right'].map(id => {
        const r = document.getElementById(id).getBoundingClientRect();
        return {x: r.left, y: r.top, w: r.width, h: r.height};})""")
    shown = []
    for r in rects:
        for i in range(0, 12):
            for j in range(0, 10):
                x = r["x"] + 1 + (r["w"] - 2) * i / 11
                y = r["y"] + 1 + (r["h"] - 2) * j / 9
                page.mouse.move(x, y)
                if page.evaluate("getComputedStyle(%s).display" % box) != "none":
                    shown.append((round(x), round(y)))
    page.wait_for_timeout(300)
    assert page.evaluate("getComputedStyle(%s).display" % box) == "none"
    assert shown == [], "the hover box appeared over the keypad at %r" % shown[:5]
