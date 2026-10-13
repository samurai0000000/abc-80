#
# test_components.py
#
# The board components show only the custom yellow inspection box (no native browser tooltip), carry the
# authentic names from the book, and the speaker is the ferro-magnetic dynamic speaker, not a piezo disk.
#
# Copyright (C) 2026, Charles Chiou
#

import json
from pathlib import Path

from conftest import ROOT, wait_for

_cand = ROOT / "plan" / "plan_abc80_improvements.artifacts" / "component_metadata_replacement.json"
if not _cand.exists():
    _cand = ROOT / "plan" / "done" / "plan_abc80_improvements.artifacts" / "component_metadata_replacement.json"
REPLACEMENT = json.loads(_cand.read_text())


def test_components_have_no_native_title_attribute(panel):
    panel.open()
    page = panel.page
    bad = page.evaluate(
        """ids => ids.filter(id => { const el = document.getElementById(id); return !el || el.hasAttribute('title'); })""",
        sorted(REPLACEMENT))
    assert bad == [], bad


def hover_tooltip(panel, element_id):
    page = panel.page
    page.evaluate("id => document.getElementById(id).scrollIntoView({block: 'center', inline: 'center'})", element_id)
    box = page.evaluate("""id => { const r = document.getElementById(id).getBoundingClientRect();
                                    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; }""", element_id)
    page.mouse.move(box["x"] - 5, box["y"] - 5)
    page.mouse.move(box["x"], box["y"])
    wait_for(lambda: page.evaluate("document.getElementById('component-tooltip-box').classList.contains('visible')"),
             what="tooltip for " + element_id)
    return page.inner_text("#component-tooltip-box")


def test_hovering_shows_the_book_names(panel):
    panel.open()
    for element_id in ("ic-u1", "ic-u4", "ic-u6", "disc-speaker", "disc-q17"):
        text = hover_tooltip(panel, element_id)
        refdes, name = REPLACEMENT[element_id][0], REPLACEMENT[element_id][1]
        assert refdes in text, (element_id, text)
        assert name in text, (element_id, text)


def test_the_speaker_is_not_a_piezo_disk(panel):
    panel.open()
    page = panel.page
    assert page.evaluate("document.getElementById('disc-speaker') !== null")
    assert page.evaluate("document.getElementById('disc-piezo-disk') === null")
    assert "piezo" not in page.content().lower()
