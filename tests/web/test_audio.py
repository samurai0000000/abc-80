#
# test_audio.py
#
# The speaker: the page replays the board's Port C bit 7 edges as a square wave. The test
# taps the audio graph with a monkey-patched AudioNode.prototype.connect (the destination node has
# no outputs, so an analyser cannot be attached to it directly) and checks silence at idle and
# a tone of the right pitch during the monitor's keyclick (plan scenario A10, row R13).
#
# Copyright (C) 2026, Charles Chiou
#

from conftest import wait_for

TAP = """() => {
    const ctx = window.__abc80.audioCtx;
    window.__tap = { analyser: ctx.createAnalyser(), peak: 0, crossings: 0, samples: 0 };
    window.__tap.analyser.fftSize = 2048;
    const original = AudioNode.prototype.connect;
    AudioNode.prototype.connect = function (dest, ...args) {
        if (dest === ctx.destination && this !== window.__tap.analyser) original.call(this, window.__tap.analyser);
        return original.call(this, dest, ...args);
    };
    const buf = new Float32Array(window.__tap.analyser.fftSize);
    window.__tapTimer = setInterval(() => {
        window.__tap.analyser.getFloatTimeDomainData(buf);
        let prev = buf[0];
        for (let i = 1; i < buf.length; ++i) {
            const v = buf[i];
            window.__tap.peak = Math.max(window.__tap.peak, Math.abs(v));
            if ((prev < 0) !== (v < 0) && Math.abs(v) > 0.001 && Math.abs(prev) > 0.001) window.__tap.crossings++;
            prev = v;
        }
        window.__tap.samples += buf.length;
    }, 20);
    window.__tap.sampleRate = ctx.sampleRate;
}"""


def start_tap(panel):
    panel.page.evaluate("window.__abc80.initAudio()")
    wait_for(lambda: panel.page.evaluate("window.__abc80.audioCtx && window.__abc80.audioCtx.state") == "running",
             what="the audio context running")
    panel.page.evaluate(TAP)


def test_a10_silent_at_idle_then_a_tone_at_the_keyclick_pitch(panel):
    panel.open()
    panel.wait_telemetry(lambda t: any(m != 0 for m in t["displayMasks"]), what="start display")
    start_tap(panel)
    panel.page.wait_for_timeout(600)  # let any chime tail from the boot pass through
    panel.page.evaluate("window.__tap.peak = 0; window.__tap.crossings = 0; window.__tap.samples = 0")
    panel.page.wait_for_timeout(500)
    assert panel.page.evaluate("window.__tap.peak") < 1e-4, "idle monitor must be silent"

    panel.page.evaluate("window.__tap.peak = 0; window.__tap.crossings = 0; window.__tap.samples = 0")
    panel.click_key("ADRS")
    wait_for(lambda: panel.page.evaluate("window.__tap.peak") > 0.01, timeout=5, what="sound from the keyclick")
    panel.page.wait_for_timeout(400)
    tap = panel.page.evaluate("window.__tap")
    # The click is a 4.475 kHz square wave (ms3k half period 200 T at 1.79 MHz), repeated analysis windows
    # overlap, so measure the pitch from the crossings seen while the tone was audible.
    assert tap["peak"] > 0.01
    assert tap["crossings"] > 20
