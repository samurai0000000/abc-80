#
# conftest.py
#
# Fixtures for the web front panel tests: a real abc80_server process, a real Chromium, and
# helpers that wait on observable state (never fixed sleeps).
#
# Copyright (C) 2026, Charles Chiou
#

import os
import select
import signal
import subprocess
import time
from pathlib import Path

import pytest
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = os.environ.get("BUILD_DIR", "build")
GOLDEN_DIR = Path(__file__).resolve().parent / "goldens"


def pytest_addoption(parser):
    parser.addoption("--update-goldens", action="store_true", default=False,
                     help="write the golden screenshots instead of comparing against them")


def wait_for(predicate, timeout=10.0, poll=0.05, what="condition"):
    """Polls predicate until it returns a truthy value; fails the test with `what` on timeout."""
    end = time.monotonic() + timeout
    value = predicate()
    while not value:
        if time.monotonic() >= end:
            raise AssertionError("timed out after %.1fs waiting for %s" % (timeout, what))
        time.sleep(poll)
        value = predicate()
    return value


class Server:
    def __init__(self, process, port):
        self.process = process
        self.port = port
        self.url = "http://127.0.0.1:%d/" % port

    @property
    def pid(self):
        return self.process.pid


@pytest.fixture(scope="session")
def server():
    binary = ROOT / BUILD_DIR / "abc80_server"
    assert binary.exists(), "%s is missing; run `make` first" % binary
    process = subprocess.Popen(
        [str(binary), "--port", "0", "--boot-turbo", "--root", str(ROOT), "--grace", "2", "--max-sessions", "32"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=str(ROOT))
    port = None
    end = time.monotonic() + 60
    buffered = ""
    while port is None and time.monotonic() < end:
        if process.poll() is not None:
            raise AssertionError("abc80_server exited early: %s" % process.stderr.read())
        ready, _, _ = select.select([process.stdout], [], [], 0.2)
        if ready:
            line = process.stdout.readline()
            buffered += line
            if line.startswith("LISTENING "):
                port = int(line.split()[1])
    assert port is not None, "abc80_server never printed LISTENING; output: %r" % buffered
    yield Server(process, port)
    process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        process.kill()
        raise AssertionError("abc80_server did not exit on SIGTERM")


@pytest.fixture(scope="session")
def browser():
    with sync_playwright() as p:
        b = p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required"])
        yield b
        b.close()


@pytest.fixture
def context(browser):
    ctx = browser.new_context(viewport={"width": 1600, "height": 1000})
    yield ctx
    ctx.close()


class Panel:
    """One open tab on the front panel, with helpers that read the live telemetry."""

    def __init__(self, page, server):
        self.page = page
        self.server = server
        self.errors = []
        page.on("console", lambda m: self.errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: self.errors.append(str(e)))

    def open(self, rom=None):
        self.page.goto(self.server.url, wait_until="domcontentloaded")
        if rom is not None:
            self.page.select_option("#rom-select", rom)
        self.wait_telemetry()
        return self

    def telemetry(self):
        return self.page.evaluate("window.__abc80 && window.__abc80.lastTelemetry")

    def wait_telemetry(self, predicate=None, timeout=15.0, what="telemetry"):
        def check():
            t = self.telemetry()
            if t and (predicate is None or predicate(t)):
                return t
            return None
        return wait_for(check, timeout=timeout, what=what)

    def digits(self):
        t = self.telemetry()
        return "".join(t["displayDigits"]) if t else ""

    def lit(self):
        t = self.telemetry()
        return bool(t) and any(m != 0 for m in t["displayMasks"])

    def click_key(self, name, delay=50):
        """A real mouse click: mousedown, hold `delay` ms, mouseup."""
        self.page.click('button[data-key="%s"]' % name, delay=delay)

    def type_keys(self, *names, delay=50):
        for name in names:
            self.click_key(name, delay=delay)

    def wait_digits(self, predicate, timeout=15.0, what="display"):
        return wait_for(lambda: predicate(self.digits()), timeout=timeout, what=what)


@pytest.fixture
def panel(context, server):
    p = Panel(context.new_page(), server)
    yield p
    assert p.errors == [], "JavaScript console errors: %r" % p.errors
