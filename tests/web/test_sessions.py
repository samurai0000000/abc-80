#
# test_sessions.py
#
# Sessions: a reload keeps the board, two tabs are isolated, and nothing runs when nobody watches
# (plan scenarios A6, A7, A8).
#
# Copyright (C) 2026, Charles Chiou
#

import psutil

from conftest import Panel, wait_for


def test_a6_reload_keeps_the_board(panel):
    panel.open()
    panel.type_keys("ADRS", "7", "8")
    panel.wait_digits(lambda d: "78" in d, what="typed digits on the display")
    before = panel.telemetry()
    token = panel.page.evaluate("window.__abc80.token")
    panel.page.reload(wait_until="domcontentloaded")
    panel.wait_telemetry(lambda t: "78" in "".join(t["displayDigits"]), what="the same display after the reload")
    assert panel.page.evaluate("window.__abc80.token") == token
    assert panel.telemetry()["cycles"] >= before["cycles"], "emulated time did not restart"


def test_a7_two_tabs_are_isolated(context, server):
    a = Panel(context.new_page(), server).open()
    b = Panel(context.new_page(), server).open()
    try:
        assert a.page.evaluate("window.__abc80.token") != b.page.evaluate("window.__abc80.token")
        before = b.telemetry()["displayMasks"]
        a.type_keys("ADRS", "1", "2", "3", "4")
        a.wait_digits(lambda d: d.startswith("1234"), what="tab A address")
        assert not b.digits().startswith("1234")
        assert b.telemetry()["displayMasks"] == before
        # and the other way round
        b.type_keys("ADRS", "9", "9")
        b.wait_digits(lambda d: "99" in d, what="tab B address")
        assert a.digits().startswith("1234")
    finally:
        assert a.errors == [] and b.errors == []


def test_a8_an_idle_server_uses_no_cpu(context, server):
    p = Panel(context.new_page(), server).open()
    p.type_keys("ADRS", "5")
    p.wait_digits(lambda d: "5" in d, what="digit typed")
    p.page.close()
    proc = psutil.Process(server.pid)
    wait_for(lambda: server_attached(server) == 0, timeout=10, what="the tab to detach")
    proc.cpu_percent(interval=None)
    cpu = proc.cpu_percent(interval=2.0)
    assert cpu < 2.0, "idle server used %.1f%% of a core" % cpu


def server_attached(server):
    import json
    import urllib.request
    with urllib.request.urlopen(server.url + "api/status", timeout=5) as r:
        return json.loads(r.read())["attached"]
