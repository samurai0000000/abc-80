#
# test_cassette.py
#
# End-to-end Playwright verification of the vintage cassette tape deck GUI, spinning reels,
# 32-character annotation persistence, and authentic 8255 PA7/PC7 ROM audio tone playback/recording.
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def test_cassette_deck_ui_and_seeded_tapes(panel):
    panel.open()
    page = panel.page

    # Switch to the Cassette Tape tab
    page.click("button[data-tab='tab-tape']")
    wait_for(lambda: page.locator("#tab-tape").is_visible(), what="tab-tape visible")

    # Verify vintage cassette deck elements and enabled piano keys
    assert page.locator("#cassette-deck").is_visible()
    assert page.locator("#tape-reel-left").is_visible()
    assert page.locator("#tape-reel-right").is_visible()
    for btn_id in ("#btn-tape-play", "#btn-tape-rec", "#btn-tape-rew", "#btn-tape-stop"):
        assert not page.locator(btn_id).is_disabled(), btn_id

    # Verify seeded tapes (0001..0029 and 0100..0103) are populated in the library
    wait_for(lambda: page.locator("#tape-library-list .tape-lib-row").count() >= 4,
             what="seeded tapes in library list")
    assert page.input_value("#tape-file-id-input") == "0001"
    assert "Lab01: Data Transfer" in page.input_value("#tape-annotation-input")


def test_cassette_annotation_32_char_and_slot_selection(panel):
    panel.open()
    page = panel.page
    page.click("button[data-tab='tab-tape']")
    wait_for(lambda: page.locator("#tab-tape").is_visible(), what="tab-tape visible")

    # Select an on-demand [0-F]{4} slot 0042
    page.fill("#tape-file-id-input", "0042")
    page.click("#btn-tape-select")
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("fileId") == "0042",
                         what="telemetry tape.fileId == 0042")

    # Set a 32-char annotation and save
    label = "1234567890ABCDEF1234567890ABCDEF"
    page.fill("#tape-annotation-input", label)
    page.click("#btn-tape-save-label")

    panel.wait_telemetry(lambda t: t.get("tape", {}).get("annotation") == label,
                         what="telemetry tape.annotation updated")
    wait_for(lambda: "0042" in page.inner_text("#tape-library-list"),
             what="0042 in library list")


def test_cassette_reel_spin_and_authentic_from_tape_playback(panel):
    panel.open()
    page = panel.page
    page.click("button[data-tab='tab-tape']")
    wait_for(lambda: page.locator("#tab-tape").is_visible(), what="tab-tape visible")

    # Select seeded showcase tape 0102 (Dual-Tone Chime at 1000H..100EH: 21 FA 00 ...) and press PLAY
    page.fill("#tape-file-id-input", "0102")
    page.click("#btn-tape-select")
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("fileId") == "0102",
                         what="tape 0102 selected")

    page.click("#btn-tape-play")
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("state") == "playing",
                         what="tape deck playing")

    # Verify spinning reel CSS class is active on the cassette deck
    wait_for(lambda: "deck-playing" in (page.locator("#cassette-deck").get_attribute("class") or ""),
             what="deck-playing class on #cassette-deck")
    assert page.inner_text("#tape-status-badge").strip() == "PLAYING"

    # Key in FROM TAPE -> 0 -> 1 -> 0 -> 2 -> GO EXEC on the ABC-80 keypad
    panel.type_keys("FROM TAPE", "0", "1", "0", "2", "GO EXEC", delay=60)

    # Wait for the authentic audio playback to finish and the deck to auto-stop
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("state") == "stopped",
                         timeout=30.0, what="tape playback auto-stop after FROM TAPE 0102")

    # Verify the ABC-80 Monitor ROM decoded the audio tones into RAM 1000H (0x21)
    panel.type_keys("ADRS", "1", "0", "0", "0", "DATA", delay=60)
    panel.wait_digits(lambda d: d == "100021", timeout=10.0, what="100021 on 7-seg display")


def test_cassette_record_to_tape_and_restore_from_tape(panel):
    panel.open()
    page = panel.page
    page.click("button[data-tab='tab-tape']")
    wait_for(lambda: page.locator("#tab-tape").is_visible(), what="tab-tape visible")

    # Enter 2 bytes at 1050H..1051H: CA, FE
    panel.type_keys("ADRS", "1", "0", "5", "0", "DATA", "C", "A", "+", "F", "E", delay=60)
    panel.wait_digits(lambda d: d == "1051FE", timeout=10.0, what="1051FE on 7-seg display")

    # Arm cassette deck in RECORD mode with custom annotation
    page.fill("#tape-annotation-input", "Recorded CA FE Test")
    page.click("#btn-tape-rec")
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("state") == "recording",
                         what="tape deck recording")
    wait_for(lambda: "deck-recording" in (page.locator("#cassette-deck").get_attribute("class") or ""),
             what="deck-recording class on #cassette-deck")

    # Execute TO TAPE: File ID 0099, Start 1050H, End 1051H, GO EXEC
    panel.type_keys(
        "TO TAPE",
        "0", "0", "9", "9", "+",
        "1", "0", "5", "0", "+",
        "1", "0", "5", "1", "GO EXEC",
        delay=60
    )

    # Wait for ewt to finish and CassetteDeck to auto-stop and save 0099.tape
    panel.wait_telemetry(
        lambda t: t.get("tape", {}).get("state") == "stopped" and t.get("tape", {}).get("fileId") == "0099",
        timeout=30.0,
        what="tape recording auto-stop and fileId 0099 demodulated"
    )
    wait_for(lambda: "0099" in page.inner_text("#tape-library-list"),
             what="0099 in tape library list")

    # Overwrite 1050H..1051H with 00, 00
    panel.type_keys("ADRS", "1", "0", "5", "0", "DATA", "0", "0", "+", "0", "0", delay=60)
    panel.wait_digits(lambda d: d == "105100", timeout=10.0, what="105100 on 7-seg display")

    # Press PLAY on 0099.tape and execute FROM TAPE -> 0 -> 0 -> 9 -> 9 -> GO EXEC
    page.click("#btn-tape-play")
    panel.wait_telemetry(lambda t: t.get("tape", {}).get("state") == "playing",
                         what="tape deck playing 0099")
    panel.type_keys("FROM TAPE", "0", "0", "9", "9", "GO EXEC", delay=60)

    panel.wait_telemetry(lambda t: t.get("tape", {}).get("state") == "stopped",
                         timeout=30.0, what="tape playback auto-stop after FROM TAPE 0099")

    # Verify 1051H is restored to FE and 1050H is restored to CA
    panel.wait_digits(lambda d: d == "1051FE", timeout=10.0, what="1051FE restored after FROM TAPE")
    panel.type_keys("-", delay=60)
    panel.wait_digits(lambda d: d == "1050CA", timeout=10.0, what="1050CA restored after FROM TAPE")


