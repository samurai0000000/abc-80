#
# test_qb_host.py
#
# End-to-end Playwright verification of the 1993 Boot ROM (vintage/rom.abc) and
# PC/AT QuickBASIC 4.0 Color VGA CRT Monitor interface (#qb-crt-window).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def qb_state(panel):
    return panel.page.evaluate("window.__abc80 && window.__abc80.qbState")


def wait_qb_state(panel, predicate, timeout=15.0, what="qb_state"):
    def check():
        st = qb_state(panel)
        if st and predicate(st):
            return st
        return None
    return wait_for(check, timeout=timeout, what=what)


def test_qb_crt_opens_in_standby_on_1993_rom_and_connects_with_16_color_vga_palette(panel):
    panel.open(rom="1993")
    panel.wait_telemetry(lambda t: t["rom"] == "1993", timeout=20, what="1993 ROM boot")

    # Selecting 1993 ROM opens #qb-crt-window in standby without stealing PCB keyboard focus
    st = wait_qb_state(
        panel,
        lambda s: s["visible"] is True and len(s["rows"]) == 25,
        what="CRT window visible with 25 rows in standby",
    )
    assert st["connected"] is False
    assert st["focused"] is False
    assert not panel.lit(), "unconnected 1993 ROM remains dark at 0B04H poll"

    # Click Connect (F1) on the lower monitor apron
    panel.page.click("#btn-qb-connect")
    st = wait_qb_state(
        panel,
        lambda s: s["connected"] is True and s["mode"] == "neutral",
        what="InitialABC handshake complete and connected",
    )
    assert st["focused"] is True

    # Verify authentic SCREEN 12 16-color VGA palette spans are rendered inside #qb-crt-screen
    screen_text = panel.page.locator("#qb-crt-screen").inner_text()
    assert "Address" in screen_text
    assert "Byte" in screen_text
    assert "1 0 0 0" in screen_text
    assert "Charles Chiou" in screen_text

    # Verify VGA color classes: 13 (Light Magenta banner), 14 (Yellow center row), 15 (Bright White), 12 (Light Red footer)
    assert panel.page.locator("#qb-crt-screen .vga-fg-13").count() > 0
    assert panel.page.locator("#qb-crt-screen .vga-fg-14").count() > 0
    assert panel.page.locator("#qb-crt-screen .vga-fg-15").count() > 0
    assert panel.page.locator("#qb-crt-screen .vga-fg-12").count() > 0
    assert "LINK: 8255 ACTIVE" in panel.page.locator("#qb-link-badge").inner_text()


def test_qb_keyboard_focus_isolation_and_address_data_entry_and_ins_del(panel):
    panel.open(rom="1993")
    panel.page.click("#btn-qb-connect")
    wait_qb_state(panel, lambda s: s["connected"] is True, what="connected")

    # Focus is on VGA CRT: press Home -> 1, 2, 0, 0 -> End -> A, 5 -> PageDown -> 3, C
    page = panel.page
    page.keyboard.press("Home")
    wait_qb_state(panel, lambda s: s["mode"] == "address", what="address input mode")
    # Verify Light Blue (COLOR 9) mode cursor blocks on row 8
    assert page.locator("#qb-crt-screen .vga-fg-9").count() > 0

    for key in ["1", "2", "0", "0"]:
        page.keyboard.press(key)
    wait_qb_state(panel, lambda s: s["currentAddress"] == 0x1200, what="address 0x1200")

    page.keyboard.press("End")
    wait_qb_state(panel, lambda s: s["mode"] == "data", what="data input mode")

    for key in ["A", "5"]:
        page.keyboard.press(key)
    wait_qb_state(
        panel,
        lambda s: "1 2 0 0" in s["rows"][12] and "A5" in s["rows"][12],
        what="wrote A5 at 0x1200",
    )

    page.keyboard.press("PageDown")
    wait_qb_state(panel, lambda s: s["currentAddress"] == 0x1201, what="stepped to 0x1201")
    for key in ["3", "C"]:
        page.keyboard.press(key)
    wait_qb_state(
        panel,
        lambda s: "1 2 0 1" in s["rows"][12] and "3C" in s["rows"][12],
        what="wrote 3C at 0x1201",
    )

    # Step back to 0x1200 using PageUp and verify INS / DEL via uploaded 0x1700/0x1740 Z80 helpers
    page.keyboard.press("PageUp")
    wait_qb_state(panel, lambda s: s["currentAddress"] == 0x1200, what="back at 0x1200")

    page.keyboard.press("Insert")
    wait_qb_state(
        panel,
        lambda s: "1 2 0 0" in s["rows"][12] and "00" in s["rows"][12] and "A5" in s["rows"][13],
        what="INS shifted 0x1200..0x16FF and wrote 00 at 0x1200",
    )

    page.keyboard.press("Delete")
    wait_qb_state(
        panel,
        lambda s: "1 2 0 0" in s["rows"][12] and "A5" in s["rows"][12] and "3C" in s["rows"][13],
        what="DEL restored A5 at 0x1200 and 3C at 0x1201",
    )

    # Clicking #board-container transfers keyboard focus back to ABC-80 PCB
    page.click("#board-container")
    wait_qb_state(panel, lambda s: s["focused"] is False, what="PCB keyboard focus restored")
    assert "ABC-80 PCB" in page.locator("#qb-focus-badge").inner_text()


def test_qb_save_and_load_abc_file_and_jump_to_009b_monitor(panel):
    panel.open(rom="1993")
    panel.page.click("#btn-qb-connect")
    wait_qb_state(panel, lambda s: s["connected"] is True, what="connected")

    page = panel.page
    # Write 3E at 0x1000 and 42 at 0x1001
    page.keyboard.press("End")
    wait_qb_state(panel, lambda s: s["mode"] == "data", what="data mode")
    for k in ["3", "E"]:
        page.keyboard.press(k)
    wait_qb_state(panel, lambda s: "3E" in s["rows"][12], what="wrote 3E at 0x1000")

    page.keyboard.press("PageDown")
    wait_qb_state(panel, lambda s: s["currentAddress"] == 0x1001, what="at 0x1001")
    for k in ["4", "2"]:
        page.keyboard.press(k)
    wait_qb_state(panel, lambda s: "42" in s["rows"][12], what="wrote 42 at 0x1001")

    # Reset currentAddress to 0x1000 and click Save on the monitor apron
    page.keyboard.press("Home")
    for k in ["1", "0", "0", "0"]:
        page.keyboard.press(k)
    wait_qb_state(panel, lambda s: s["currentAddress"] == 0x1000, what="at 0x1000")

    page.fill("#qb-filename-input", "WEBTEST.ABC")
    page.click("#btn-qb-save")
    wait_qb_state(
        panel,
        lambda s: "WEBTEST.ABC" in s.get("programs", []),
        what="WEBTEST.ABC saved in library",
    )

    # Overwrite 0x1000 with 00, then Load WEBTEST.ABC back
    page.click("#qb-crt-screen")
    page.keyboard.press("End")
    for k in ["0", "0"]:
        page.keyboard.press(k)
    wait_qb_state(panel, lambda s: "00" in s["rows"][12], what="zeroed 0x1000")

    page.click("#btn-qb-load")
    wait_qb_state(
        panel,
        lambda s: "Loaded" in s.get("statusMessage", "") and "3E" in s["rows"][12],
        what="WEBTEST.ABC loaded back into RAM",
    )

    # Click 009BH Monitor button to exit 0x0B00 PC-link and enter the interactive keypad monitor
    page.click("#btn-qb-mon-009b")
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="009BH monitor scanning display")
    panel.type_keys("ADRS", "1", "2", "3", "4")
    panel.wait_digits(lambda d: d.startswith("1234"), what="ABC-80 keypad works in 009BH monitor")
