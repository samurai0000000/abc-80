#
# test_manual_and_layout.py
#
# Qualification suite for ABC-80 front-panel toolbar decluttering, centered board layout,
# interactive 8-chapter User's Manual Book modal, all 33 sample tape program cards, and
# responsive mobile viewport layout.
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for


def test_header_decluttered_and_board_toolbar_centered(panel):
    panel.open()
    page = panel.page

    # Verify selectors are NOT inside #main-header
    assert page.locator("#main-header #instance-select").count() == 0
    assert page.locator("#main-header #rom-select").count() == 0
    assert page.locator("#main-header #ext-io-select").count() == 0
    assert page.locator("#main-header #btn-manual").count() == 0

    # Verify selectors and User's Manual button are inside #board-panel-toolbar
    assert page.locator("#hardware-panel-section #board-panel-toolbar").count() == 1
    assert page.locator("#board-panel-toolbar #instance-select").count() == 1
    assert page.locator("#board-panel-toolbar #rom-select").count() == 1
    assert page.locator("#board-panel-toolbar #ext-io-select").count() == 1
    assert page.locator("#board-panel-toolbar #btn-manual").count() == 1

    # Verify #btn-manual is positioned immediately to the right of .ext-io-picker
    ext_box = page.locator(".ext-io-picker").bounding_box()
    btn_box = page.locator("#btn-manual").bounding_box()
    assert ext_box is not None and btn_box is not None
    assert btn_box["x"] >= ext_box["x"] + ext_box["width"] - 2

    # Verify .chassis-outer-shell is horizontally centered inside #board-stage on 1600x1000
    stage_box = page.locator("#board-stage").bounding_box()
    chassis_box = page.locator(".chassis-outer-shell").bounding_box()
    assert stage_box is not None and chassis_box is not None

    left_offset = chassis_box["x"] - stage_box["x"]
    expected_left = (stage_box["width"] - chassis_box["width"]) / 2.0
    assert abs(expected_left - left_offset) <= 2.0, (
        f"Board is not centered: stage={stage_box}, chassis={chassis_box}, "
        f"expected_left={expected_left}, actual_left={left_offset}"
    )


def test_manual_book_chapters_and_escape_isolation(panel):
    panel.open()
    page = panel.page

    # Type address on the keypad first to set a known state
    panel.type_keys("ADRS", "1", "2", "3", "4")
    panel.wait_digits(lambda d: d.startswith("1234"), what="address 1234 on display")

    # Open User's Manual
    page.click("#btn-manual")
    wait_for(
        lambda: not page.locator("#manual-modal").evaluate("el => el.classList.contains('hidden')"),
        what="manual modal visible",
    )
    assert page.is_visible("#manual-modal")

    # Verify initial chapter indicator
    indicator = page.locator("#manual-ch-indicator")
    wait_for(lambda: "Chapter 1 of 8" in indicator.inner_text(), what="Chapter 1 indicator")
    assert page.is_visible("#manual-chapter-ch1")

    # Navigate forward through all 8 chapters using Next button
    for ch_idx in range(2, 9):
        page.click("#btn-manual-next-ch")
        wait_for(
            lambda idx=ch_idx: f"Chapter {idx} of 8" in indicator.inner_text(),
            what=f"Chapter {ch_idx} indicator via Next",
        )
        assert page.is_visible(f"#manual-chapter-ch{ch_idx}")

    # Next button should now be disabled on Chapter 8
    assert page.locator("#btn-manual-next-ch").is_disabled()

    # Navigate backward to Chapter 1 using Prev button
    for ch_idx in range(7, 0, -1):
        page.click("#btn-manual-prev-ch")
        wait_for(
            lambda idx=ch_idx: f"Chapter {idx} of 8" in indicator.inner_text(),
            what=f"Chapter {ch_idx} indicator via Prev",
        )
        assert page.is_visible(f"#manual-chapter-ch{ch_idx}")

    # Prev button should now be disabled on Chapter 1
    assert page.locator("#btn-manual-prev-ch").is_disabled()

    # Navigate directly using TOC buttons
    for ch_idx in range(1, 9):
        page.click(f'.book-toc-btn[data-chapter="ch{ch_idx}"]')
        wait_for(
            lambda idx=ch_idx: f"Chapter {idx} of 8" in indicator.inner_text(),
            what=f"Chapter {ch_idx} indicator via TOC button",
        )
        assert page.is_visible(f"#manual-chapter-ch{ch_idx}")

    # While #manual-modal is open, press Escape:
    # Must close modal AND NOT reset the ABC-80 CPU (display digits still start with 1234)
    page.keyboard.press("Escape")
    wait_for(
        lambda: page.locator("#manual-modal").evaluate("el => el.classList.contains('hidden')"),
        what="manual modal closed after Escape",
    )

    # CPU must not have reset:
    assert panel.digits().startswith("1234"), f"CPU was unexpectedly reset! Digits: {panel.digits()}"


def test_manual_tape_catalog_all_33_programs_search_and_load(panel):
    panel.open()
    page = panel.page

    # Open manual modal and switch to Chapter 5
    page.click("#btn-manual")
    wait_for(
        lambda: not page.locator("#manual-modal").evaluate("el => el.classList.contains('hidden')"),
        what="manual modal visible",
    )
    page.click('.book-toc-btn[data-chapter="ch5"]')
    wait_for(lambda: page.is_visible("#manual-chapter-ch5"), what="Chapter 5 active")

    # Verify all 33 tape program cards are rendered
    expected_ids = [f"{i:04d}" for i in range(1, 30)] + [f"{i:04d}" for i in range(100, 104)]
    wait_for(
        lambda: page.locator("#manual-tape-cards .book-tape-card").count() == 33,
        what="all 33 tape cards rendered",
    )
    rendered_ids = page.evaluate(
        "() => Array.from(document.querySelectorAll('#manual-tape-cards .book-tape-card')).map(c => c.getAttribute('data-tape-id'))"
    )
    assert rendered_ids == expected_ids, f"Rendered IDs mismatch: {rendered_ids}"

    # Verify each card contains 'What It Does', 'How It Works', 'How to Load & Run', and a load button
    invalid_cards = page.evaluate("""() => {
        const cards = Array.from(document.querySelectorAll('#manual-tape-cards .book-tape-card'));
        return cards.filter(c => {
            const text = c.textContent || "";
            const hasDoes = text.includes("What It Does");
            const hasWorks = text.includes("How It Works");
            const hasLoad = text.includes("How to Load");
            const hasBtn = c.querySelector('.book-load-tape-btn') !== null;
            return !(hasDoes && hasWorks && hasLoad && hasBtn);
        }).map(c => ({
            id: c.getAttribute('data-tape-id'),
            hasDoes: (c.textContent || "").includes("What It Does"),
            hasWorks: (c.textContent || "").includes("How It Works"),
            hasLoad: (c.textContent || "").includes("How to Load"),
            hasBtn: c.querySelector('.book-load-tape-btn') !== null
        }));
    }""")
    assert invalid_cards == [], f"Cards failing checks: {invalid_cards}"

    # Test category filter 'showcase' -> exactly 4 cards (0100..0103)
    page.click('.book-cat-btn[data-cat="showcase"]')
    wait_for(
        lambda: page.locator("#manual-tape-cards .book-tape-card").count() == 4,
        what="4 showcase demo cards",
    )
    showcase_ids = page.evaluate(
        "() => Array.from(document.querySelectorAll('#manual-tape-cards .book-tape-card')).map(c => c.getAttribute('data-tape-id'))"
    )
    assert showcase_ids == ["0100", "0101", "0102", "0103"]

    # Reset filter to 'all' -> 33 cards
    page.click('.book-cat-btn[data-cat="all"]')
    wait_for(
        lambda: page.locator("#manual-tape-cards .book-tape-card").count() == 33,
        what="33 cards after reset to all",
    )

    # Test search filter with 'Pac-Man' -> exactly card 0017
    page.fill("#manual-search-input", "Pac-Man")
    wait_for(
        lambda: page.locator("#manual-tape-cards .book-tape-card").count() == 1,
        what="1 search result for Pac-Man",
    )
    assert page.locator("#manual-tape-cards .book-tape-card").get_attribute("data-tape-id") == "0017"

    # Clear search input -> back to 33 cards
    page.fill("#manual-search-input", "")
    wait_for(
        lambda: page.locator("#manual-tape-cards .book-tape-card").count() == 33,
        what="33 cards after search cleared",
    )

    # Click 'Select Slot in Tape Deck' for program 0017
    page.click('.book-load-tape-btn[data-load-tape="0017"]')

    # Verify modal closed
    wait_for(
        lambda: page.locator("#manual-modal").evaluate("el => el.classList.contains('hidden')"),
        what="manual modal closed after loading tape",
    )

    # Verify Tape tab is selected and deck input & label are updated to 0017
    wait_for(
        lambda: page.locator("#tab-tape").evaluate("el => el.classList.contains('active')"),
        what="tab-tape active",
    )
    wait_for(
        lambda: page.input_value("#tape-file-id-input") == "0017",
        what="tape-file-id-input updated to 0017",
    )
    wait_for(
        lambda: page.inner_text("#cassette-label-id") == "FILE 0017",
        what="cassette-label-id updated to FILE 0017",
    )


def test_mobile_responsive_layout_and_manual_drawer(panel):
    panel.open()
    page = panel.page

    # Resize viewport to mobile (iPhone SE: 375x667)
    page.set_viewport_size({"width": 375, "height": 667})
    page.evaluate("window.dispatchEvent(new Event('resize'))")

    # Verify main workspace switches to column layout
    wait_for(
        lambda: page.evaluate("getComputedStyle(document.getElementById('main-workspace')).flexDirection") == "column",
        what="main-workspace flex-direction column",
    )

    # Verify inspector section is positioned below the hardware panel section
    hw_box = page.locator("#hardware-panel-section").bounding_box()
    insp_box = page.locator("#inspector-section").bounding_box()
    assert hw_box is not None and insp_box is not None
    assert insp_box["y"] >= hw_box["y"] + hw_box["height"] - 2, (
        f"Inspector not below hardware: hw_bottom={hw_box['y'] + hw_box['height']}, insp_top={insp_box['y']}"
    )

    # Verify .chassis-outer-shell bounding box fits within the 375x667 viewport (both width AND single-screen height)
    chassis_box = page.locator(".chassis-outer-shell").bounding_box()
    assert chassis_box is not None
    assert chassis_box["x"] >= 0, f"Chassis extends beyond left: {chassis_box}"
    assert chassis_box["x"] + chassis_box["width"] <= 375, (
        f"Chassis width {chassis_box['width']} at x={chassis_box['x']} overflows 375px viewport"
    )
    assert chassis_box["y"] <= 110, (
        f"Too much wasted vertical space before board on mobile: board starts at y={chassis_box['y']}"
    )
    assert chassis_box["y"] + chassis_box["height"] <= 667, (
        f"Board bottom {chassis_box['y'] + chassis_box['height']} overflows 667px mobile screen height"
    )

    # Verify layout also fits on Pixel 7 (412x915) and Android Chrome visible viewport (384x720)
    for vw, vh in [(384, 720), (412, 915)]:
        page.set_viewport_size({"width": vw, "height": vh})
        page.evaluate("window.dispatchEvent(new Event('resize'))")
        wait_for(
            lambda: (
                page.locator(".chassis-outer-shell").bounding_box()["x"]
                + page.locator(".chassis-outer-shell").bounding_box()["width"]
                <= vw
                and page.locator(".chassis-outer-shell").bounding_box()["y"]
                + page.locator(".chassis-outer-shell").bounding_box()["height"]
                <= vh
            ),
            what=f"chassis fits within single screen {vw}x{vh}",
        )

    # Switch back to 375x667 to test mobile User's Manual drawer
    page.set_viewport_size({"width": 375, "height": 667})
    page.evaluate("window.dispatchEvent(new Event('resize'))")

    # Open manual modal
    page.click("#btn-manual")
    wait_for(
        lambda: not page.locator("#manual-modal").evaluate("el => el.classList.contains('hidden')"),
        what="manual modal visible on mobile",
    )

    # Verify full-screen width modal
    shell_box = page.locator("#manual-modal .book-shell").bounding_box()
    assert shell_box is not None
    assert shell_box["width"] <= 375

    # Toggle mobile TOC sidebar drawer
    sidebar = page.locator("#manual-sidebar")
    assert not sidebar.evaluate("el => el.classList.contains('mobile-toc-open')")

    page.click("#btn-manual-toc-toggle")
    wait_for(
        lambda: sidebar.evaluate("el => el.classList.contains('mobile-toc-open')"),
        what="sidebar mobile-toc-open after toggle click",
    )

    # Selecting a chapter should close the mobile drawer and show that chapter
    page.click('.book-toc-btn[data-chapter="ch3"]')
    wait_for(
        lambda: not sidebar.evaluate("el => el.classList.contains('mobile-toc-open')"),
        what="sidebar mobile-toc-open removed after chapter selection",
    )
    assert page.is_visible("#manual-chapter-ch3")
