/*
 * app.js - ABC-80 Single-Board Trainer Web Controller & Host WebSocket Client
 *
 * Copyright (C) 2026, Charles Chiou
 */

(function () {
    'use strict';

    const SVG_7SEG = `
        <svg class="seg-svg" viewBox="0 0 32 52">
            <polygon class="seg-path seg-a" points="8.15,3.50 20.35,3.50 23.95,7.10 4.55,7.10" />
            <polygon class="seg-path seg-b" points="20.90,7.65 24.50,4.05 24.50,24.95 20.90,23.15" />
            <polygon class="seg-path seg-c" points="20.90,27.85 24.50,26.05 24.50,46.95 20.90,43.35" />
            <polygon class="seg-path seg-d" points="4.55,43.90 23.95,43.90 20.35,47.50 8.15,47.50" />
            <polygon class="seg-path seg-e" points="4.00,26.05 7.60,27.85 7.60,43.35 4.00,46.95" />
            <polygon class="seg-path seg-f" points="4.00,4.05 7.60,7.65 7.60,23.15 4.00,24.95" />
            <polygon class="seg-path seg-g" points="4.55,25.50 6.35,23.70 22.15,23.70 23.95,25.50 22.15,27.30 6.35,27.30" />
            <circle class="seg-path seg-dp" cx="28.5" cy="46.0" r="2.0" />
        </svg>
    `;

    // Page state. Everything shown comes from the server's board; nothing is simulated here.
    const state = {
        soundEnabled: true,
        wsConnected: false,
        lastTelemetry: null,
        registers: {
            pc: 0, sp: 0, af: 0, bc: 0, de: 0, hl: 0, ix: 0, iy: 0,
            flags: { s: 0, z: 0, h: 0, pv: 0, n: 0, c: 0 }
        },
        cycles: 0,
        tapeState: 'STOPPED'
    };

    let isCalibrating = false;

    // ==========================================================================
    // Host connection (one WebSocket session = one emulated board; plan Section 2.4)
    // ==========================================================================

    const CPU_HZ = 1790000;                      // the board's Z80 clock
    const FRAME_TSTATES = 29833;                 // T-states per 60 Hz telemetry frame
    const DIGIT_IDS = ['digit-a3', 'digit-a2', 'digit-a1', 'digit-a0', 'digit-d1', 'digit-d0'];

    let ws = null;
    let reconnectDelayMs = 1000;

    function storage() {
        try { return window.sessionStorage; } catch (e) { return null; }
    }

    // A random token kept in sessionStorage: a reload of this tab reattaches to the same board,
    // another tab gets its own.
    function sessionToken() {
        const store = storage();
        let token = store ? store.getItem('abc80_token') : null;
        if (!token) {
            const bytes = new Uint8Array(18);
            window.crypto.getRandomValues(bytes);
            token = Array.from(bytes, b => 'abcdefghijklmnopqrstuvwxyz0123456789'[b % 36]).join('');
            if (store) store.setItem('abc80_token', token);
        }
        return token;
    }

    function selectedRom() {
        const select = document.getElementById('rom-select');
        const store = storage();
        const saved = store ? store.getItem('abc80_rom') : null;
        if (select && select.value) return select.value;
        return saved === '1993' ? '1993' : 'monitor';
    }

    function setHostStatus(online, text) {
        const dot = document.getElementById('host-ws-dot');
        const label = document.getElementById('host-ws-text');
        if (dot) dot.className = 'status-dot ' + (online ? 'dot-connected' : 'dot-offline');
        if (label) label.textContent = text;
    }

    function scheduleReconnect() {
        setTimeout(connectHostWebSocket, reconnectDelayMs);
        reconnectDelayMs = Math.min(reconnectDelayMs * 2, 10000);
    }

    function connectHostWebSocket() {
        const url = (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws';
        try {
            ws = new WebSocket(url);
        } catch (e) {
            setHostStatus(false, 'HOST: OFFLINE');
            scheduleReconnect();
            return;
        }
        ws.onopen = function () {
            state.wsConnected = true;
            reconnectDelayMs = 1000;
            setHostStatus(true, 'HOST: ONLINE');
            ws.send(JSON.stringify({ type: 'hello', token: sessionToken(), rom: selectedRom() }));
        };
        ws.onmessage = function (event) {
            let msg;
            try { msg = JSON.parse(event.data); } catch (e) { return; }
            if (msg.type === 'telemetry') applyTelemetry(msg);
            else if (msg.type === 'error') console.warn('[ABC-80 host] ' + msg.reason);
        };
        ws.onclose = function () {
            state.wsConnected = false;
            setHostStatus(false, 'HOST: OFFLINE');
            scheduleReconnect();
        };
        ws.onerror = function () {
            try { ws.close(); } catch (e) {}
        };
    }

    function sendHostCommand(cmd) {
        if (state.wsConnected && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(cmd));
        }
    }

    // ---- Telemetry: display, LEDs, registers, speaker ----

    function setSegmentMask(digitElement, mask) {
        if (!digitElement) return;
        const segClasses = ['seg-a', 'seg-b', 'seg-c', 'seg-d', 'seg-e', 'seg-f', 'seg-g', 'seg-dp'];
        segClasses.forEach((cls, idx) => {
            const path = digitElement.querySelector('.' + cls);
            if (path) path.classList.toggle('active', (mask & (1 << idx)) !== 0);
        });
    }

    // Brightness 0-1, exposed as data-level and the --led-level CSS variable.
    function setLedLevel(el, level) {
        if (!el) return;
        const v = Math.max(0, Math.min(1, Number(level) || 0));
        el.dataset.level = v.toFixed(3);
        el.style.setProperty('--led-level', String(v));
    }

    function setLedOn(el, on) {
        if (el) el.classList.toggle('active', !!on);
    }

    function applyTelemetry(msg) {
        state.lastTelemetry = msg;
        if (Array.isArray(msg.displayMasks)) {
            DIGIT_IDS.forEach((id, idx) => {
                if (msg.displayMasks[idx] !== undefined) setSegmentMask(document.getElementById(id), msg.displayMasks[idx]);
            });
        }
        if (msg.leds) {
            setLedOn(document.getElementById('disc-led-ep'), msg.leds.ep);
            setLedOn(document.getElementById('disc-led-halt'), msg.leds.halt);
            setLedLevel(document.getElementById('disc-led-audio-grn'), msg.leds.speaker);
        }
        if (msg.registers) {
            Object.assign(state.registers, msg.registers);
        }
        if (msg.cycles !== undefined) state.cycles = msg.cycles;
        updateInspector();
        const select = document.getElementById('rom-select');
        if (select && msg.rom && select.value !== msg.rom) {
            select.value = msg.rom;
            const store = storage();
            if (store) store.setItem('abc80_rom', msg.rom);
        }
        scheduleSpeaker(msg.speakerLevel, msg.speakerEdges);
    }

    // ---- Speaker: replay the board's Port C bit 7 edges as a square wave ----

    let audioCtx = null;
    let speakerNextStart = 0;

    function initAudio() {
        if (!audioCtx) {
            const AudioContextClass = window.AudioContext || window.webkitAudioContext;
            if (AudioContextClass) audioCtx = new AudioContextClass();
        }
        if (audioCtx && audioCtx.state === 'suspended') {
            audioCtx.resume().catch(function () {});
        }
    }

    // Each telemetry frame is exactly FRAME_TSTATES of emulated time. The edge offsets (in T-states)
    // become sample positions in a one-frame buffer, played back to back on a continuous timeline.
    function scheduleSpeaker(endLevel, edges) {
        if (!audioCtx || !state.soundEnabled || audioCtx.state !== 'running') {
            speakerNextStart = 0;
            return;
        }
        const frameSeconds = FRAME_TSTATES / CPU_HZ;
        const now = audioCtx.currentTime;
        if (speakerNextStart < now + 0.02) speakerNextStart = now + 0.05;  // (re)start with a small buffer
        const start = speakerNextStart;
        speakerNextStart += frameSeconds;
        if (!edges || edges.length === 0) return;  // a steady level makes no sound

        const sr = audioCtx.sampleRate;
        const length = Math.ceil(frameSeconds * sr) + 1;
        const buffer = audioCtx.createBuffer(1, length, sr);
        const data = buffer.getChannelData(0);
        let level = !!endLevel;
        if (edges.length % 2 === 1) level = !level;  // the level at the start of the frame
        let index = 0;
        for (let i = 0; i < edges.length; ++i) {
            const stop = Math.min(length, Math.round(edges[i] / CPU_HZ * sr));
            data.fill(level ? 1 : -1, index, stop);
            index = Math.max(index, stop);
            level = !level;
        }
        data.fill(level ? 1 : -1, index, length);

        const source = audioCtx.createBufferSource();
        const gain = audioCtx.createGain();
        gain.gain.value = 0.06;
        source.buffer = buffer;
        source.connect(gain);
        gain.connect(audioCtx.destination);
        source.start(start);
    }

    // ---- Keys and reset ----

    const KEY_NAME_FOR_BUTTON = { 'GO EXEC': 'GO' };

    function initDisplayDigits() {
        DIGIT_IDS.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.innerHTML = SVG_7SEG;
        });
    }

    function handleKeypadPress(key) {
        initAudio();
        if (key === 'RST') {
            handleHardwareReset();
            return;
        }
        const btn = document.querySelector('.tactile-switch[data-key="' + key + '"], .overlay-key[data-key="' + key + '"], .key-btn[data-key="' + key + '"]');
        if (btn) {
            btn.classList.add('key-pressed');
            setTimeout(() => btn.classList.remove('key-pressed'), 120);
        }
        sendHostCommand({ type: 'key', key: KEY_NAME_FOR_BUTTON[key] || key, action: 'down' });
    }

    function handleKeypadRelease(key) {
        if (key === 'RST') return;
        sendHostCommand({ type: 'key', key: KEY_NAME_FOR_BUTTON[key] || key, action: 'up' });
    }

    // The reset switch is not part of the key matrix: it resets the CPU and the PPI.
    function handleHardwareReset() {
        initAudio();
        sendHostCommand({ type: 'reset' });
        const btn = document.getElementById('btn-hw-rst');
        if (btn) {
            btn.classList.add('key-pressed');
            setTimeout(() => btn.classList.remove('key-pressed'), 150);
        }
    }

    function changeRom(name) {
        const store = storage();
        if (store) store.setItem('abc80_rom', name);
        sendHostCommand({ type: 'setRom', rom: name });
    }

    // For tests and the console.
    window.__abc80 = {
        initAudio: initAudio,
        get audioCtx() { return audioCtx; },
        get lastTelemetry() { return state.lastTelemetry; },
        applyTelemetry: applyTelemetry,   // feed a telemetry frame to the renderer (test hook)
        disconnect: function () { reconnectDelayMs = 3600000; if (ws) { ws.onmessage = null; ws.close(); } },  // stop live frames (test hook)
        get token() { return sessionToken(); }
    };

    let scopeCanvas, scopeCtx, scopePhase = 0;
    function initScope() {
        scopeCanvas = document.getElementById('tape-scope-canvas');
        if (scopeCanvas) {
            scopeCtx = scopeCanvas.getContext('2d');
            requestAnimationFrame(drawScope);
        }
    }

    function drawScope() {
        if (!scopeCtx) return;
        const w = scopeCanvas.width;
        const h = scopeCanvas.height;
        scopeCtx.fillStyle = '#020406';
        scopeCtx.fillRect(0, 0, w, h);

        scopeCtx.strokeStyle = '#0f172a';
        scopeCtx.lineWidth = 1;
        scopeCtx.beginPath();
        scopeCtx.moveTo(0, h / 2); scopeCtx.lineTo(w, h / 2);
        scopeCtx.stroke();

        scopeCtx.strokeStyle = state.tapeState === 'PLAYING' ? '#38bdf8' : (state.tapeState === 'RECORDING' ? '#ef4444' : '#1e293b');
        scopeCtx.lineWidth = 1.5;
        scopeCtx.beginPath();
        const freq = state.tapeState === 'PLAYING' ? 0.15 : (state.tapeState === 'RECORDING' ? 0.3 : 0.02);
        const amp = state.tapeState === 'STOPPED' ? 2 : (h / 2 - 8);

        for (let x = 0; x < w; x++) {
            const y = h / 2 + Math.sin(x * freq + scopePhase) * amp;
            if (x === 0) scopeCtx.moveTo(x, y);
            else scopeCtx.lineTo(x, y);
        }
        scopeCtx.stroke();

        scopePhase += state.tapeState === 'STOPPED' ? 0.02 : 0.25;
        requestAnimationFrame(drawScope);
    }

    function initInspector() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
                btn.classList.add('active');
                const pane = document.getElementById(btn.getAttribute('data-tab'));
                if (pane) pane.classList.add('active');
            });
        });

        // The server does not send memory contents, so these panels say so instead of showing sample data.
        const disasmContainer = document.getElementById('disasm-container');
        if (disasmContainer) {
            disasmContainer.innerHTML = '<div class="disasm-line"><span class="disasm-operands">Live disassembly is not provided by this front panel.</span></div>';
        }

        populateHexEditor();
        updateInspector();
    }

    function populateHexEditor() {
        const hexContainer = document.getElementById('hex-editor-table');
        if (!hexContainer) return;
        hexContainer.innerHTML = '<div class="hex-row"><span class="hex-ascii">The memory view is not provided by this front panel.</span></div>';
    }

    function updateInspector() {
        document.getElementById('cycle-counter').textContent = state.cycles.toLocaleString();
        document.getElementById('reg-pc').textContent = '0x' + state.registers.pc.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-sp').textContent = '0x' + state.registers.sp.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-af').textContent = '0x' + state.registers.af.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-bc').textContent = '0x' + state.registers.bc.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-de').textContent = '0x' + state.registers.de.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-hl').textContent = '0x' + state.registers.hl.toString(16).toUpperCase().padStart(4, '0');
        // Flags come from the low byte of AF: S Z - H - P/V N C.
        const flags = state.registers.af & 0xFF;
        [['flag-s', 0x80], ['flag-z', 0x40], ['flag-h', 0x10], ['flag-pv', 0x04], ['flag-n', 0x02], ['flag-c', 0x01]].forEach(([id, mask]) => {
            const el = document.getElementById(id);
            if (el) el.classList.toggle('active', (flags & mask) !== 0);
        });
    }

    function keyNameForEvent(e) {
        if (/^[0-9a-fA-F]$/.test(e.key)) return e.key.toUpperCase();
        if (e.key === '+' || e.key === 'ArrowRight') return '+';
        if (e.key === '-' || e.key === ' ' || e.key === 'F10') return '-';
        if (e.key === 'Enter') return 'GO EXEC';
        const k = e.key.toLowerCase();
        if (k === 'g') return 'GO EXEC';
        if (k === 'm') return 'ADRS';   // A and D are hex digits, so ADRS and DATA use M and N
        if (k === 'n') return 'DATA';
        if (k === 'w') return 'TO TAPE';
        if (k === 'l') return 'FROM TAPE';
        return null;
    }

    function setupKeyboardListeners() {
        const heldByCode = {};  // physical key -> board key, so a release always matches its press

        window.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
            if (e.key === 'Escape' || e.key === 'F5') {
                e.preventDefault();
                if (!e.repeat) handleHardwareReset();
                return;
            }
            if (e.key === 'F12') {
                e.preventDefault();
                document.getElementById('btn-toggle-inspector').click();
                return;
            }
            const name = keyNameForEvent(e);
            if (name === null) return;
            e.preventDefault();
            if (e.repeat || heldByCode[e.code]) return;  // auto-repeat is not a new press
            heldByCode[e.code] = name;
            handleKeypadPress(name);
        });

        window.addEventListener('keyup', (e) => {
            const name = heldByCode[e.code];
            if (!name) return;
            delete heldByCode[e.code];
            handleKeypadRelease(name);
        });

        window.addEventListener('blur', () => {
            Object.keys(heldByCode).forEach(code => {
                handleKeypadRelease(heldByCode[code]);
                delete heldByCode[code];
            });
        });
    }

    // ==========================================================================
    // Viewport Pan & Zoom Navigation Engine
    // ==========================================================================
    const viewportState = {
        scale: 1.0,
        minScale: 0.5,
        maxScale: 4.5,
        panX: 0,
        panY: 0,
        isPanning: false,
        startX: 0,
        startY: 0,
        startPanX: 0,
        startPanY: 0,
        panStep: 60,
    };

    function updateBoardTransform() {
        const board = document.getElementById('board-container');
        if (board) {
            board.style.transform = `translate(${viewportState.panX}px, ${viewportState.panY}px) scale(${viewportState.scale})`;
        }
        const badge = document.getElementById('view-zoom-badge');
        if (badge) {
            badge.textContent = `${Math.round(viewportState.scale * 100)}%`;
        }
    }

    function setZoom(newScale, focalX, focalY) {
        const clamped = Math.max(viewportState.minScale, Math.min(viewportState.maxScale, newScale));
        if (Math.abs(clamped - viewportState.scale) < 0.001) return;

        if (focalX !== undefined && focalY !== undefined) {
            const ratio = clamped / viewportState.scale;
            viewportState.panX = focalX - ratio * (focalX - viewportState.panX);
            viewportState.panY = focalY - ratio * (focalY - viewportState.panY);
        }
        viewportState.scale = Math.round(clamped * 100) / 100;
        updateBoardTransform();
    }

    function zoomIn() {
        const panel = document.getElementById('hardware-panel-section');
        const rect = panel ? panel.getBoundingClientRect() : { width: 800, height: 600, left: 0, top: 0 };
        setZoom(viewportState.scale * 1.25, rect.width / 3, rect.height / 2);
    }

    function zoomOut() {
        const panel = document.getElementById('hardware-panel-section');
        const rect = panel ? panel.getBoundingClientRect() : { width: 800, height: 600, left: 0, top: 0 };
        setZoom(viewportState.scale * 0.8, rect.width / 3, rect.height / 2);
    }

    function resetZoom() {
        viewportState.scale = 1.0;
        viewportState.panX = 0;
        viewportState.panY = 0;
        updateBoardTransform();
    }

    function panBy(dx, dy) {
        viewportState.panX += dx;
        viewportState.panY += dy;
        updateBoardTransform();
    }

    function initViewportNavigation() {
        const panel = document.getElementById('hardware-panel-section');
        if (!panel) return;

        // Button controls
        const btnIn = document.getElementById('btn-zoom-in');
        const btnOut = document.getElementById('btn-zoom-out');
        const btnReset = document.getElementById('btn-zoom-reset');
        const btnUp = document.getElementById('btn-pan-up');
        const btnDown = document.getElementById('btn-pan-down');
        const btnLeft = document.getElementById('btn-pan-left');
        const btnRight = document.getElementById('btn-pan-right');
        const btnCenter = document.getElementById('btn-pan-center');

        if (btnIn) btnIn.addEventListener('click', zoomIn);
        if (btnOut) btnOut.addEventListener('click', zoomOut);
        if (btnReset) btnReset.addEventListener('click', resetZoom);
        if (btnUp) btnUp.addEventListener('click', () => panBy(0, viewportState.panStep));
        if (btnDown) btnDown.addEventListener('click', () => panBy(0, -viewportState.panStep));
        if (btnLeft) btnLeft.addEventListener('click', () => panBy(viewportState.panStep, 0));
        if (btnRight) btnRight.addEventListener('click', () => panBy(-viewportState.panStep, 0));
        if (btnCenter) btnCenter.addEventListener('click', resetZoom);

        // Mouse wheel zoom
        panel.addEventListener('wheel', (e) => {
            e.preventDefault();
            const rect = panel.getBoundingClientRect();
            const focalX = e.clientX - rect.left;
            const focalY = e.clientY - rect.top;
            const factor = e.deltaY < 0 ? 1.15 : 0.85;
            setZoom(viewportState.scale * factor, focalX, focalY);
        }, { passive: false });

        // Canvas drag panning
        panel.addEventListener('pointerdown', (e) => {
            if (e.target.closest('#canvas-view-controls') || e.target.closest('#ic-calibration-hud')) return;
            if (isCalibrating && e.target.closest('.calibratable-component, .ic-socket-package')) return;
            if (e.target.closest('button, input, select, a')) return;

            if (e.button === 0 || e.button === 1) {
                viewportState.isPanning = true;
                viewportState.startX = e.clientX;
                viewportState.startY = e.clientY;
                viewportState.startPanX = viewportState.panX;
                viewportState.startPanY = viewportState.panY;
                panel.classList.add('is-panning');
                panel.setPointerCapture(e.pointerId);
            }
        });

        panel.addEventListener('pointermove', (e) => {
            if (!viewportState.isPanning) return;
            const dx = e.clientX - viewportState.startX;
            const dy = e.clientY - viewportState.startY;
            viewportState.panX = viewportState.startPanX + dx;
            viewportState.panY = viewportState.startPanY + dy;
            updateBoardTransform();
        });

        function stopPanning(e) {
            if (viewportState.isPanning) {
                viewportState.isPanning = false;
                panel.classList.remove('is-panning');
                try { panel.releasePointerCapture(e.pointerId); } catch (_) {}
            }
        }
        panel.addEventListener('pointerup', stopPanning);
        panel.addEventListener('pointercancel', stopPanning);

        // Keyboard hotkeys with Alt modifier
        window.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;

            if (e.altKey) {
                if (e.key === 'ArrowUp') { e.preventDefault(); panBy(0, viewportState.panStep); }
                else if (e.key === 'ArrowDown') { e.preventDefault(); panBy(0, -viewportState.panStep); }
                else if (e.key === 'ArrowLeft') { e.preventDefault(); panBy(viewportState.panStep, 0); }
                else if (e.key === 'ArrowRight') { e.preventDefault(); panBy(-viewportState.panStep, 0); }
                else if (e.key === '=' || e.key === '+') { e.preventDefault(); zoomIn(); }
                else if (e.key === '-' || e.key === '_') { e.preventDefault(); zoomOut(); }
                else if (e.key === '0') { e.preventDefault(); resetZoom(); }
            }
        });

        updateBoardTransform();
    }

    // =========================================================================
    // Developer Tools Visibility Controller (Hotkeys & Query Flags)
    // =========================================================================
    function initDevToolsVisibility() {
        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.get('dev') === '1' || urlParams.get('calib') === '1') {
            document.body.classList.add('dev-tools-visible');
        }

        window.addEventListener('keydown', (e) => {
            if (e.shiftKey && e.altKey && (e.key === 'C' || e.key === 'c')) {
                e.preventDefault();
                document.body.classList.toggle('dev-tools-visible');
                const isVisible = document.body.classList.contains('dev-tools-visible');
                console.log(`[ABC-80 DevTools] Calibration & Viewport controls ${isVisible ? 'visible' : 'hidden'}`);
            }
        });
    }

    // =========================================================================
    // Interactive Component Inspection Metadata (CAD Yellow Card)
    // =========================================================================
    const COMPONENT_METADATA = {
        // --- DIP ICs & Sockets ---
        'ic-u1': { refdes: 'U1', name: 'Z80A CPU (Z8400 / 4.000 MHz)', pkg: 'DIP-40', specs: '8-bit Microprocessor @ 4.000 MHz', role: 'Central Processing Unit executing monitor ROM & user programs' },
        'ic-u2': { refdes: 'U2', name: '2732 UV-Erasable EPROM', pkg: 'DIP-24 Ceramic', specs: '4K x 8 UV-Erasable ROM (0000H-0FFFH)', role: 'System Monitor Firmware & Subroutine Library' },
        'ic-u3': { refdes: 'U3', name: '2732 EPROM Programmer Socket', pkg: 'DIP-24 Ladder Socket', specs: '4K x 8 User EPROM Target Socket', role: 'Target socket for on-board 2732 EPROM programming (28V Vpp)' },
        'ic-u4': { refdes: 'U4', name: '8255A-5 Programmable Peripheral Interface', pkg: 'DIP-40', specs: 'Programmable Peripheral Interface (Ports A, B, C)', role: 'Keypad matrix scanning, 7-seg display multiplexing, audio/tape I/O' },
        'ic-u5': { refdes: 'U5', name: '6116 Static RAM (2K x 8)', pkg: 'DIP-24', specs: '2K x 8 High-Speed Static RAM (1000H-17FFH)', role: 'User Work RAM, Display Buffer, and System Stack' },
        'ic-u6': { refdes: 'U6', name: '6116 SRAM Expansion Socket', pkg: 'DIP-24 Ladder Socket', specs: '2K x 8 Static RAM Expansion (1800H-1FFFH)', role: 'Expansion socket for extra 2K user memory and buffers' },
        'ic-u7': { refdes: 'U7', name: 'Empty Ladder Expansion Socket', pkg: 'DIP-28 / 24', specs: '28-Pin Universal Expansion Socket', role: 'Auxiliary peripheral, ROM, or RAM expansion socket' },
        'ic-u8': { refdes: 'U8', name: '74LS138 3-to-8 Demultiplexer', pkg: 'DIP-16', specs: 'High-Speed Schottky 3-to-8 Decoder', role: 'Memory address decoding (ROM/RAM chip select enables)' },
        'ic-u9': { refdes: 'U9', name: '74LS139 Dual 2-to-4 Decoder', pkg: 'DIP-16', specs: 'Low-Power Schottky Dual 2-to-4 Decoder', role: 'I/O port address decoding (8255 PPI and expansion select)' },
        'ic-u10': { refdes: 'U10', name: '74LS14 Hex Schmitt-Trigger Inverter', pkg: 'DIP-14', specs: 'Hex Inverting Gates with Schmitt Action', role: 'Cassette EAR comparator, reset debouncing & waveform shaping' },
        'ic-u11': { refdes: 'U11', name: '74LS04 Hex Inverter', pkg: 'DIP-14', specs: 'High-Speed TTL Hex Inverter Gates', role: 'Clock buffering, single-step logic & control line inversion' },
        'silk-abc80-eprom': { refdes: 'SILK', name: 'ABC-80 PCB Legend Silkscreen', pkg: 'Top White Silkscreen', specs: 'Z80 Educational Microcomputer Learning Kit', role: 'Board model identification and EPROM socket pin 1 indicator' },

        // --- TO-92 Transistors ---
        'disc-q1': { refdes: 'Q1', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 1 (Address A3) Common Cathode Driver' },
        'disc-q2': { refdes: 'Q2', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 2 (Address A2) Common Cathode Driver' },
        'disc-q3': { refdes: 'Q3', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 3 (Address A1) Common Cathode Driver' },
        'disc-q4': { refdes: 'Q4', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 4 (Address A0) Common Cathode Driver' },
        'disc-q5': { refdes: 'Q5', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 5 (Data D1) Common Cathode Driver' },
        'disc-q6': { refdes: 'Q6', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Display Digit 6 (Data D0) Common Cathode Driver' },
        'disc-q7': { refdes: 'Q7', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Speaker Piezo Buzzer Transistor Driver' },
        'disc-q8': { refdes: 'Q8', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Cassette MIC Output Signal Driver' },
        'disc-q9': { refdes: 'Q9', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 0 Active-Low Driver' },
        'disc-q10': { refdes: 'Q10', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 1 Active-Low Driver' },
        'disc-q11': { refdes: 'Q11', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 2 Active-Low Driver' },
        'disc-q12': { refdes: 'Q12', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 3 Active-Low Driver' },
        'disc-q13': { refdes: 'Q13', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 4 Active-Low Driver' },
        'disc-q14': { refdes: 'Q14', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 5 Active-Low Driver' },
        'disc-q15': { refdes: 'Q15', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 6 Active-Low Driver' },
        'disc-q16': { refdes: 'Q16', name: '2SA1015 PNP BJT', pkg: 'TO-92', specs: 'Vceo=-50V, Ic=-150mA, hFE=70-400', role: 'Keypad Matrix Column 7 Active-Low Driver' },
        'disc-q17': { refdes: 'Q17', name: '2SC1815 NPN BJT', pkg: 'TO-92', specs: 'Vceo=50V, Ic=150mA, hFE=70-700', role: 'Single-Step Mode Hardware Inverter / Trigger' },

        // --- Bus Pull-Up Resistors (18x 10k) ---
        'disc-r-pu1': { refdes: 'RP1.1', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu2': { refdes: 'RP1.2', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu3': { refdes: 'RP1.3', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu4': { refdes: 'RP1.4', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu5': { refdes: 'RP1.5', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu6': { refdes: 'RP1.6', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu7': { refdes: 'RP1.7', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu8': { refdes: 'RP1.8', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu9': { refdes: 'RP1.9', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu10': { refdes: 'RP1.10', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu11': { refdes: 'RP1.11', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu12': { refdes: 'RP1.12', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu13': { refdes: 'RP1.13', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu14': { refdes: 'RP1.14', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu15': { refdes: 'RP1.15', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu16': { refdes: 'RP1.16', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu17': { refdes: 'RP1.17', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },
        'disc-r-pu18': { refdes: 'RP1.18', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Z80 Bus Pull-Up to +5V Rail' },

        // --- Keypad Column Pull-Up Resistors (8x 3.3k) ---
        'disc-r-q9': { refdes: 'R(Q9)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q10': { refdes: 'R(Q10)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q11': { refdes: 'R(Q11)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q12': { refdes: 'R(Q12)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q13': { refdes: 'R(Q13)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q14': { refdes: 'R(Q14)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q15': { refdes: 'R(Q15)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },
        'disc-r-q16': { refdes: 'R(Q16)', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Keypad Column Driver Base Pull-Up Resistor' },

        // --- Programmer & Clock Bias Resistors ---
        'disc-res-28v': { refdes: 'R-28V', name: '33Ω Carbon Film Resistor', pkg: 'Axial 1/2W', specs: '33Ω ±5% (Orange-Orange-Black-Gold)', role: '28V EPROM Programming Vpp Current Limiter' },
        'disc-led-ep': { refdes: 'LED-EP', name: '3mm Red LED', pkg: 'T-1 3mm', specs: 'Vf=2.0V, If=20mA (Ruby Red)', role: 'EPROM Programming Active Indicator LED' },
        'disc-r-ep-1k8': { refdes: 'R-EP', name: '1.8kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '1.8kΩ ±5% (Brown-Gray-Red-Gold)', role: 'EPROM Programming LED Indicator Current Limiter' },
        'disc-r-prog1': { refdes: 'R-PROG1', name: '1.8kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '1.8kΩ ±5% (Brown-Gray-Red-Gold)', role: 'Programmer Logic Bias Resistor' },
        'disc-r-prog2': { refdes: 'R-PROG2', name: '3.3kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '3.3kΩ ±5% (Orange-Orange-Red-Gold)', role: 'Programmer Logic Bias Resistor' },
        'disc-r-prog3': { refdes: 'R-PROG3', name: '560Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '560Ω ±5% (Green-Blue-Brown-Gold)', role: 'Programmer Pulse Shaping Resistor' },
        'disc-r-prog-10k': { refdes: 'R-PROG-10K', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Programmer Level Pull-Down Resistor' },
        'disc-r-prog4': { refdes: 'R-PROG4', name: '47kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '47kΩ ±5% (Yellow-Violet-Orange-Gold)', role: 'Programmer Vpp Control Gate Resistor' },
        'disc-r-clock': { refdes: 'R-CLK', name: '330Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '330Ω ±5% (Orange-Orange-Brown-Gold)', role: 'Crystal Oscillator Bias Feedback Resistor' },

        // --- Reset & Clock Subsystem Passives ---
        'disc-cap-50p': { refdes: 'C-50P', name: '50pF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '50pF 50V (Marked 50)', role: 'Crystal Oscillator Tank Fine-Tuning Capacitor' },
        'disc-cap-101a': { refdes: 'C-101A', name: '100pF Ceramic Disc Capacitor (H)', pkg: 'Radial Disc', specs: '100pF 50V (Marked 101)', role: 'Crystal Oscillator Tank Load Capacitor' },
        'disc-cap-101b': { refdes: 'C-101B', name: '100pF Ceramic Disc Capacitor (V)', pkg: 'Radial Disc', specs: '100pF 50V (Marked 101)', role: 'Reset Circuit Debouncing Filter Capacitor' },
        'disc-cap-10u': { refdes: 'C-10U', name: '10µF 16V Radial Electrolytic Capacitor', pkg: 'Radial Can', specs: '10µF 16V (+ Top, - Bottom)', role: 'Power-On Reset RC Timing Capacitor' },
        'disc-r-rst-10k': { refdes: 'R-RST-10K', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Reset Timing RC Pull-Up Resistor' },
        'disc-d-rst': { refdes: 'D-RST', name: '1N914 Fast Switching Diode', pkg: 'DO-35 Glass', specs: 'Vr=100V, If=200mA, trr=4ns (Cathode Top)', role: 'Power-Off Fast Discharge Diode for Reset Capacitor' },

        // --- Power & Cassette Header Subsystem Passives ---
        'disc-r-ear-330': { refdes: 'R-EAR-330', name: '330Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '330Ω ±5% (Orange-Orange-Brown-Gold)', role: 'Cassette EAR Input Current Limiting Resistor' },
        'disc-d-ear': { refdes: 'D-EAR', name: '1N914 Fast Switching Diode', pkg: 'DO-35 Glass', specs: 'Vr=100V, If=200mA, trr=4ns (Cathode Top)', role: 'Cassette EAR Input Signal Clamp Diode' },
        'disc-cap-ear-203': { refdes: 'C-EAR-203', name: '0.02µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.02µF 50V (Marked 203)', role: 'Cassette EAR Audio AC Coupling & Noise Filter' },
        'disc-cap-mic-203': { refdes: 'C-MIC-203', name: '0.02µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.02µF 50V (Marked 203)', role: 'Cassette MIC Output DC Blocking Filter' },
        'disc-r-mic-10k1': { refdes: 'R-MIC1', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Cassette Header Attenuator Resistor #1' },
        'disc-r-mic-10k2': { refdes: 'R-MIC2', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Cassette Header Attenuator Resistor #2' },
        'disc-r-mic-10k3': { refdes: 'R-MIC3', name: '10kΩ Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '10kΩ ±5% (Brown-Black-Orange-Gold)', role: 'Cassette Header Impedance Match Resistor #3' },
        'disc-cap-dc-203': { refdes: 'C-DC-203', name: '0.02µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.02µF 50V (Marked 203)', role: 'DC Power Input High-Frequency Noise Bypass' },
        'disc-cap-dc-100u': { refdes: 'C-DC-100U', name: '100µF 16V Radial Electrolytic Capacitor', pkg: 'Radial Can', specs: '100µF 16V Aluminum Blue Can (- Top, + Bottom)', role: 'DC Input Smoothing & Ripple Suppression' },
        'disc-cap-7805-203': { refdes: 'C-7805-203', name: '0.02µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.02µF 50V (Marked 203)', role: 'LM7805 +5V Regulator Output HF Decoupling' },

        // --- Display Segment Resistors (8x 120R) ---
        'disc-r-seg1': { refdes: 'R-SEG1', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment A)' },
        'disc-r-seg2': { refdes: 'R-SEG2', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment B)' },
        'disc-r-seg3': { refdes: 'R-SEG3', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment C)' },
        'disc-r-seg4': { refdes: 'R-SEG4', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment D)' },
        'disc-r-seg5': { refdes: 'R-SEG5', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment E)' },
        'disc-r-seg6': { refdes: 'R-SEG6', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment F)' },
        'disc-r-seg7': { refdes: 'R-SEG7', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Segment G)' },
        'disc-r-seg8': { refdes: 'R-SEG8', name: '120Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '120Ω ±5% (Brown-Red-Brown-Gold)', role: '7-Segment Display Anode Limiter (Decimal Point DP)' },

        // --- Capacitors ---
        'disc-cap-c2': { refdes: 'C2', name: '100µF 16V Radial Electrolytic Capacitor', pkg: 'Radial Can', specs: '100µF 16V Aluminum (Silver/Black Can)', role: '+5V Rail Bulk Filter & Voltage Ripple Suppression' },
        'disc-cap-c1': { refdes: 'C1', name: '100µF/220µF 16V Radial Electrolytic Capacitor', pkg: 'Radial Can', specs: '100µF/220µF 16V (Silver/Black Can)', role: 'Raw DC Input Filter near 7805 Regulator' },
        'disc-tantalum-cp': { refdes: 'CP', name: '10µF 25V Tantalum Bead Capacitor', pkg: 'Dipped Tantalum', specs: '10µF 25V Solid Electrolytic (Red/Orange Bead)', role: 'High-Frequency VCC Rail Transient Decoupling' },
        'disc-cap-vcc1': { refdes: 'C-VCC1', name: '0.01µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.01µF 50V (Marked 103)', role: 'Local IC VCC Bypass & Switching Spike Suppression' },
        'disc-cap-vcc2': { refdes: 'C-VCC2', name: '0.01µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.01µF 50V (Marked 103)', role: 'Local IC VCC Bypass & Switching Spike Suppression' },
        'disc-cap-vcc3': { refdes: 'C-VCC3', name: '0.01µF Ceramic Disc Capacitor', pkg: 'Radial Disc', specs: '0.01µF 50V (Marked 103)', role: 'Local IC VCC Bypass & Switching Spike Suppression' },

        // --- Power, Jacks, Switches & Ports ---
        'disc-lm7805': { refdes: 'IC-7805', name: 'LM7805 +5V 1A Voltage Regulator', pkg: 'TO-220 + Black Heatsink', specs: 'Vin=9V, Vout=+5.0V @ 1.0A Max', role: 'Main System Linear +5V Power Supply Regulator' },
        'disc-jack-dc': { refdes: 'J-DC', name: '9V DC Barrel Power Jack', pkg: '2.1mm Center-Pin PCB Jack', specs: '2.1mm Pin / 5.5mm Barrel, 9V DC In', role: 'Main DC Power Input Connector (from AC/DC Adapter)' },
        'disc-jack-ear': { refdes: 'J-EAR', name: '3.5mm Mono Audio Jack (EAR)', pkg: 'Right-Angle 3.5mm Phone Jack', specs: 'Switched Mono 3.5mm Socket', role: 'Cassette Tape Audio Data Input (Read Interface)' },
        'disc-jack-mic': { refdes: 'J-MIC', name: '3.5mm Mono Audio Jack (MIC)', pkg: 'Right-Angle 3.5mm Phone Jack', specs: 'Switched Mono 3.5mm Socket', role: 'Cassette Tape Audio Data Output (Write Interface)' },
        'disc-hw-rst': { refdes: 'SW-RST', name: 'Tactile Pushbutton Reset Switch ($909)', pkg: '6x6mm SPST-NO Tactile', specs: 'Momentary Push Button with Red Plunger', role: 'Master System Hardware Reset (Pulls /RESET Low)' },
        'disc-int-header': { refdes: 'J-INT', name: 'Single-Step Interrupt Jumper Header', pkg: '2-Pin 0.1\" Gold Header', specs: '0.1\" (2.54mm) Berg Strip Pins', role: 'Hardware Interrupt & Single-Step Control Jumper' },
        'disc-clock-section': { refdes: 'XTAL-1', name: '4.000 MHz Quartz Crystal Unit', pkg: 'HC-49/U Metal Can', specs: 'Fundamental 4.000000 MHz ±30ppm', role: 'Master Clock Oscillator Frequency Standard' },

        // --- 7-Segment LED Displays (6x) ---
        'disc-disp-a3': { refdes: 'DISP-A3', name: '7-Segment LED Display (Address A3)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Address Display Digit 3 (Bits A15-A12)' },
        'disc-disp-a2': { refdes: 'DISP-A2', name: '7-Segment LED Display (Address A2)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Address Display Digit 2 (Bits A11-A8)' },
        'disc-disp-a1': { refdes: 'DISP-A1', name: '7-Segment LED Display (Address A1)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Address Display Digit 1 (Bits A7-A4)' },
        'disc-disp-a0': { refdes: 'DISP-A0', name: '7-Segment LED Display (Address A0)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Address Display Digit 0 (Bits A3-A0)' },
        'disc-disp-d1': { refdes: 'DISP-D1', name: '7-Segment LED Display (Data D1)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Data Display High Digit (Bits D7-D4)' },
        'disc-disp-d0': { refdes: 'DISP-D0', name: '7-Segment LED Display (Data D0)', pkg: 'DIP-10 Module', specs: 'Ruby Red Common Cathode 0.5\" Digit', role: 'Data Display Low Digit (Bits D3-D0)' },

        // --- Audio / Speaker ---
        'disc-piezo-disk': { refdes: 'SP-1', name: 'Piezoelectric Speaker Buzzer Disk', pkg: '30mm Brass Disk', specs: 'Resonant Freq ~2.4 kHz, SPL > 75dB', role: 'Acoustic Sound Output Transducer (System Beeps & Tones)' },
        'disc-r-sp33': { refdes: 'R-SP33', name: '33Ω Carbon Film Resistor', pkg: 'Axial 1/2W', specs: '33Ω ±5% (Orange-Orange-Black-Gold)', role: 'Speaker Transducer Drive Current Limiter' },
        'disc-r-sp330a': { refdes: 'R-SP330A', name: '330Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '330Ω ±5% (Orange-Orange-Brown-Gold)', role: 'Audio Monitor Green LED Current Limiter' },
        'disc-r-sp330b': { refdes: 'R-SP330B', name: '330Ω Carbon Film Resistor', pkg: 'Axial 1/4W', specs: '330Ω ±5% (Orange-Orange-Brown-Gold)', role: 'Audio Monitor Red LED Current Limiter' },
        'disc-led-audio-grn': { refdes: 'LED-AUD-GRN', name: '3mm Green LED', pkg: 'T-1 3mm', specs: 'Vf=2.2V, If=20mA (Emerald Green)', role: 'Audio Output Positive Phase Monitor LED' },
        'disc-led-halt': { refdes: 'LED-HALT', name: '3mm Red LED', pkg: 'T-1 3mm', specs: 'Vf=2.0V, If=20mA (Ruby Red)', role: 'Audio Output Negative Phase Monitor LED' },

        // --- Dual 3x4 Keypad Modules ---
        'disc-keypad-left': { refdes: 'KEY-L', name: 'Hexadecimal Keypad Module (Left 3x4)', pkg: 'Molded Ivory Bezel', specs: '12 Tactile Hex Keys (0-9, A, B)', role: 'Hexadecimal Address/Data Matrix Entry (Left 3x4 Array)' },
        'disc-keypad-right': { refdes: 'KEY-R', name: 'Function & Hex Keypad Module (Right 3x4)', pkg: 'Molded Ivory Bezel', specs: '12 Tactile Function/Hex Keys (C-F, ADRS, DATA, +, -, GO, TAPE, RST)', role: 'System Command & High-Hex Matrix Entry (Right 3x4 Array)' }
    };

    const KEYPAD_METADATA = {
        '0': { refdes: 'KEY 0', name: 'Hexadecimal Key 0', pkg: 'Key Switch', specs: 'Code $00', role: 'Input hex nibble 0 / memory data/address entry' },
        '1': { refdes: 'KEY 1', name: 'Hexadecimal Key 1', pkg: 'Key Switch', specs: 'Code $01', role: 'Input hex nibble 1 / memory data/address entry' },
        '2': { refdes: 'KEY 2', name: 'Hexadecimal Key 2', pkg: 'Key Switch', specs: 'Code $02', role: 'Input hex nibble 2 / memory data/address entry' },
        '3': { refdes: 'KEY 3', name: 'Hexadecimal Key 3', pkg: 'Key Switch', specs: 'Code $03', role: 'Input hex nibble 3 / memory data/address entry' },
        '4': { refdes: 'KEY 4', name: 'Hexadecimal Key 4', pkg: 'Key Switch', specs: 'Code $04', role: 'Input hex nibble 4 / memory data/address entry' },
        '5': { refdes: 'KEY 5', name: 'Hexadecimal Key 5', pkg: 'Key Switch', specs: 'Code $05', role: 'Input hex nibble 5 / memory data/address entry' },
        '6': { refdes: 'KEY 6', name: 'Hexadecimal Key 6', pkg: 'Key Switch', specs: 'Code $06', role: 'Input hex nibble 6 / memory data/address entry' },
        '7': { refdes: 'KEY 7', name: 'Hexadecimal Key 7', pkg: 'Key Switch', specs: 'Code $07', role: 'Input hex nibble 7 / memory data/address entry' },
        '8': { refdes: 'KEY 8', name: 'Hexadecimal Key 8', pkg: 'Key Switch', specs: 'Code $08', role: 'Input hex nibble 8 / memory data/address entry' },
        '9': { refdes: 'KEY 9', name: 'Hexadecimal Key 9', pkg: 'Key Switch', specs: 'Code $09', role: 'Input hex nibble 9 / memory data/address entry' },
        'A': { refdes: 'KEY A', name: 'Hexadecimal Key A', pkg: 'Key Switch', specs: 'Code $0A', role: 'Input hex nibble A / memory data/address entry' },
        'B': { refdes: 'KEY B', name: 'Hexadecimal Key B', pkg: 'Key Switch', specs: 'Code $0B', role: 'Input hex nibble B / memory data/address entry' },
        'C': { refdes: 'KEY C', name: 'Hexadecimal Key C', pkg: 'Key Switch', specs: 'Code $0C', role: 'Input hex nibble C / memory data/address entry' },
        'D': { refdes: 'KEY D', name: 'Hexadecimal Key D', pkg: 'Key Switch', specs: 'Code $0D', role: 'Input hex nibble D / memory data/address entry' },
        'E': { refdes: 'KEY E', name: 'Hexadecimal Key E', pkg: 'Key Switch', specs: 'Code $0E', role: 'Input hex nibble E / memory data/address entry' },
        'F': { refdes: 'KEY F', name: 'Hexadecimal Key F', pkg: 'Key Switch', specs: 'Code $0F', role: 'Input hex nibble F / memory data/address entry' },
        'ADRS': { refdes: 'KEY ADRS', name: 'Address Mode Key', pkg: 'Function Key', specs: 'Code $14', role: 'Select 16-bit address entry mode (displays address on A3-A0)' },
        'DATA': { refdes: 'KEY DATA', name: 'Data Mode Key', pkg: 'Function Key', specs: 'Code $12', role: 'Select 8-bit data entry mode (displays data byte on D1-D0)' },
        '+': { refdes: 'KEY +', name: 'Address Increment Key', pkg: 'Function Key', specs: 'Code $13', role: 'Step to next sequential memory location (PC/Address + 1)' },
        '-': { refdes: 'KEY -', name: 'Single-Step / Decrement Key', pkg: 'Function Key', specs: 'Code $10', role: 'Execute single instruction step / decrement address' },
        'GO EXEC': { refdes: 'KEY GO', name: 'Execute / Run Key', pkg: 'Function Key', specs: 'Code $11', role: 'Jump to specified 16-bit address and begin program execution' },
        'RST': { refdes: 'KEY RST', name: 'System Reset Key', pkg: 'Tactile Plunger', specs: 'Code $18', role: 'Master system hardware / software reset ($909)' },
        'TO TAPE': { refdes: 'KEY TO TAPE', name: 'Cassette Write Key', pkg: 'Function Key', specs: 'Code $17', role: 'Save specified memory buffer to audio cassette tape' },
        'FROM TAPE': { refdes: 'KEY FROM TAPE', name: 'Cassette Read Key', pkg: 'Function Key', specs: 'Code $16', role: 'Load program data from audio cassette tape into memory' }
    };

    function initComponentInspectionTooltips() {
        const tooltip = document.getElementById('component-tooltip-box');
        if (!tooltip) return;

        let activeTarget = null;

        function showTooltipForElement(target, e) {
            // Keypad keys show no hover text (it got in the way of using them). Everything else keeps its tooltip.
            if (target.closest('.key-btn, .tactile-switch, .overlay-key, .keypad-bezel-module')) {
                hideTooltip();
                return;
            }
            const compEl = target.closest('.calibratable-component, .ic-socket-package, #silk-abc80-eprom');
            if (!compEl) {
                hideTooltip();
                return;
            }
            const id = compEl.id;
            const meta = COMPONENT_METADATA[id] || {
                refdes: compEl.getAttribute('data-label') || id,
                name: compEl.getAttribute('title') || compEl.getAttribute('data-label') || id,
                pkg: 'Discrete Component',
                specs: 'Standard PCB Footprint',
                role: 'ABC-80 Hardware Element'
            };

            tooltip.innerHTML = `
                <div class="tooltip-header">
                    <span class="tooltip-refdes">${meta.refdes}</span>
                    <span class="tooltip-pkg">${meta.pkg}</span>
                </div>
                <div class="tooltip-name">${meta.name}</div>
                ${meta.specs ? `<div class="tooltip-specs">${meta.specs}</div>` : ''}
                ${meta.role ? `<div class="tooltip-role">${meta.role}</div>` : ''}
            `;

            tooltip.style.display = 'block';
            tooltip.classList.add('visible');
            positionTooltip(e);
        }

        function positionTooltip(e) {
            const pad = 14;
            let left = e.clientX + pad;
            let top = e.clientY + pad;

            const rect = tooltip.getBoundingClientRect();
            if (left + rect.width > window.innerWidth - 12) {
                left = e.clientX - rect.width - pad;
            }
            if (top + rect.height > window.innerHeight - 12) {
                top = e.clientY - rect.height - pad;
            }
            if (left < 8) left = 8;
            if (top < 8) top = 8;

            tooltip.style.left = left + 'px';
            tooltip.style.top = top + 'px';
        }

        function hideTooltip() {
            activeTarget = null;
            tooltip.classList.remove('visible');
            tooltip.style.display = 'none';
        }

        const boardContainer = document.getElementById('board-container') || document.querySelector('.trainer-chassis');
        if (boardContainer) {
            boardContainer.addEventListener('mousemove', (e) => {
                if (isCalibrating) {
                    hideTooltip();
                    return;
                }
                const candidate = e.target.closest('.calibratable-component, .ic-socket-package, #silk-abc80-eprom, [data-key]');
                if (candidate) {
                    activeTarget = candidate;
                    showTooltipForElement(candidate, e);
                } else if (activeTarget) {
                    hideTooltip();
                }
            });

            boardContainer.addEventListener('mouseleave', () => {
                hideTooltip();
            });
        }
    }

    function initAll() {
        initDisplayDigits();
        initInspector();
        initScope();
        initViewportNavigation();
        setupKeyboardListeners();
        connectHostWebSocket();

        // Matrix keys press on mousedown/touchstart and release on mouseup/mouseleave/touchend/touchcancel.
        // The reset keys are not matrix keys: they only reset.
        // The keypad bezels (the frames around the keys) show no native hover text either.
        document.querySelectorAll('.keypad-bezel-module').forEach(module => {
            const label = module.getAttribute('title');
            if (label) {
                module.setAttribute('aria-label', label);
                module.removeAttribute('title');
            }
        });

        document.querySelectorAll('.tactile-switch, .overlay-key, .key-btn').forEach(btn => {
            const key = btn.getAttribute('data-key');
            if (!key) return;
            // No native hover text on keypad keys; keep it as the accessible name.
            const label = btn.getAttribute('title');
            if (label) {
                btn.setAttribute('aria-label', label);
                btn.removeAttribute('title');
            }
            if (key === 'RST') {
                btn.addEventListener('click', () => { if (!isCalibrating) handleHardwareReset(); });
                return;
            }
            const down = (e) => { if (isCalibrating) return; if (e.cancelable) e.preventDefault(); handleKeypadPress(key); };
            const up = () => handleKeypadRelease(key);
            btn.addEventListener('mousedown', down);
            btn.addEventListener('touchstart', down, { passive: false });
            btn.addEventListener('mouseup', up);
            btn.addEventListener('mouseleave', up);
            btn.addEventListener('touchend', up);
            btn.addEventListener('touchcancel', up);
        });

        const romSelect = document.getElementById('rom-select');
        if (romSelect) {
            const store = storage();
            const saved = store ? store.getItem('abc80_rom') : null;
            if (saved === '1993' || saved === 'monitor') romSelect.value = saved;
            romSelect.addEventListener('change', () => changeRom(romSelect.value));
        }

        // Tactile Reset Button (#disc-hw-rst and #btn-hw-rst)
        const hwRstBtn = document.getElementById('btn-hw-rst');
        const hwRstComp = document.getElementById('disc-hw-rst');
        if (hwRstBtn) {
            hwRstBtn.addEventListener('click', (e) => {
                if (isCalibrating) return;
                e.stopPropagation();
                handleHardwareReset();
            });
        }
        if (hwRstComp) {
            hwRstComp.addEventListener('click', (e) => {
                if (isCalibrating) return;
                handleHardwareReset();
            });
        }

        // Developer tools visibility hotkey and URL query parameter
        initDevToolsVisibility();

        // Interactive Component Inspection Tooltips
        initComponentInspectionTooltips();

        const toggleInspBtn = document.getElementById('btn-toggle-inspector');
        const inspectorSection = document.getElementById('inspector-section');
        if (toggleInspBtn && inspectorSection) {
            toggleInspBtn.addEventListener('click', () => {
                inspectorSection.classList.toggle('inspector-collapsed');
            });
        }

        const soundBtn = document.getElementById('btn-sound-toggle');
        if (soundBtn) {
            soundBtn.addEventListener('click', () => {
                state.soundEnabled = !state.soundEnabled;
                soundBtn.classList.toggle('sound-active', state.soundEnabled);
            });
        }

        // Cassette tape is not supported by this front panel.
        ['btn-tape-play', 'btn-tape-rec', 'btn-tape-stop', 'btn-tape-rew'].forEach(id => {
            const btn = document.getElementById(id);
            if (btn) {
                btn.disabled = true;
                btn.title = 'Cassette tape is not supported';
            }
        });

        const helpBtn = document.getElementById('btn-help');
        const helpModal = document.getElementById('help-modal');
        const closeModalBtn = document.getElementById('btn-close-modal');
        if (helpBtn && helpModal && closeModalBtn) {
            helpBtn.addEventListener('click', () => helpModal.classList.remove('hidden'));
            closeModalBtn.addEventListener('click', () => helpModal.classList.add('hidden'));
            helpModal.addEventListener('click', (e) => {
                if (e.target === helpModal) helpModal.classList.add('hidden');
            });
        }

        // Initialize Universal Component Calibration & Drag-and-Drop Placement Tool
        initUniversalCalibrationController();
    }

    // =========================================================================
    // Universal Component Calibration & Drag-and-Drop Placement Controller
    // =========================================================================
    const DEFAULT_CALIBRATION_COORDS = {
        // --- DIP ICs & Sockets (User Calibrated Footprints) ---
        'ic-u1':  { left: 67.8, top: 21.4, width: 50.0, height: 154.0 },
        'ic-u2':  { left: 153.0, top: 90.0, width: 50.0, height: 92.0 },
        'ic-u3':  { left: 220.2, top: 89.4, width: 50.0, height: 92.0 },
        'ic-u4':  { left: 289.0, top: 88.0, width: 50.0, height: 92.0 },
        'ic-u5':  { left: 69.2, top: 195.2, width: 50.0, height: 154.0 },
        'ic-u6':  { left: 152.8, top: 195.2, width: 50.0, height: 154.0 },
        'ic-u7':  { left: 287.4, top: 255.0, width: 52.0, height: 90.0 },
        'ic-u8':  { left: 206.4, top: 22.0, width: 26.0, height: 56.0 },
        'ic-u9':  { left: 154.8, top: 27.2, width: 26.0, height: 52.0 },
        'ic-u10': { left: 237.8, top: 229.0, width: 26.0, height: 52.0 },
        'ic-u11': { left: 238.6, top: 286.0, width: 26.0, height: 36.0 },
        'silk-abc80-eprom': { left: 362.0, top: 194.0, width: 68.0, height: 32.0 },

        // --- TO-92 Transistors (+50% Enlarged) ---
        'disc-q1':  { left: 77.0, top: 363.5, width: 18.0, height: 15.0 },
        'disc-q2':  { left: 116.0, top: 362.5, width: 18.0, height: 15.0 },
        'disc-q3':  { left: 155.0, top: 362.5, width: 18.0, height: 15.0 },
        'disc-q4':  { left: 193.0, top: 362.0, width: 18.0, height: 15.0 },
        'disc-q5':  { left: 237.0, top: 362.4, width: 18.0, height: 15.0 },
        'disc-q6':  { left: 273.1, top: 362.4, width: 18.0, height: 15.0 },
        'disc-q7':  { left: 303.2, top: 412.4, width: 18.0, height: 12.0 },
        'disc-q8':  { left: 304.0, top: 427.4, width: 18.0, height: 12.0 },
        'disc-q9':  { left: 21.1, top: 233.5, width: 18.0, height: 12.0 },
        'disc-q10': { left: 21.4, top: 247.2, width: 18.0, height: 12.0 },
        'disc-q11': { left: 20.6, top: 277.8, width: 18.0, height: 12.0 },
        'disc-q12': { left: 21.0, top: 263.0, width: 18.0, height: 12.0 },
        'disc-q13': { left: 21.8, top: 292.0, width: 18.0, height: 12.0 },
        'disc-q14': { left: 21.4, top: 307.5, width: 18.0, height: 12.0 },
        'disc-q15': { left: 21.4, top: 324.1, width: 18.0, height: 12.0 },
        'disc-q16': { left: 21.8, top: 339.6, width: 18.0, height: 12.0 },
        'disc-q17': { left: 208.2, top: 245.5, width: 18.0, height: 15.0, rotation: 270 },

        // --- Bus Pull-Up Resistors (18x 10k) ---
        'disc-r-pu1':  { left: 36.0, top: 74.0, width: 22.0, height: 4.0 },
        'disc-r-pu2':  { left: 36.0, top: 82.0, width: 22.0, height: 4.0 },
        'disc-r-pu3':  { left: 36.0, top: 90.0, width: 22.0, height: 4.0 },
        'disc-r-pu4':  { left: 36.0, top: 98.0, width: 22.0, height: 4.0 },
        'disc-r-pu5':  { left: 36.0, top: 103.1, width: 22.0, height: 4.0 },
        'disc-r-pu6':  { left: 34.5, top: 113.4, width: 22.0, height: 4.0 },
        'disc-r-pu7':  { left: 36.0, top: 121.4, width: 22.0, height: 4.0 },
        'disc-r-pu8':  { left: 35.4, top: 129.4, width: 22.0, height: 4.0 },
        'disc-r-pu9':  { left: 36.0, top: 137.4, width: 22.0, height: 4.0 },
        'disc-r-pu10': { left: 35.4, top: 144.3, width: 22.0, height: 4.0 },
        'disc-r-pu11': { left: 36.6, top: 152.8, width: 22.0, height: 4.0 },
        'disc-r-pu12': { left: 35.6, top: 160.0, width: 22.0, height: 4.0 },
        'disc-r-pu13': { left: 37.9, top: 188.9, width: 22.0, height: 4.0 },
        'disc-r-pu14': { left: 37.3, top: 196.2, width: 22.0, height: 4.0 },
        'disc-r-pu15': { left: 37.1, top: 203.7, width: 22.0, height: 4.0 },
        'disc-r-pu16': { left: 36.1, top: 211.6, width: 22.0, height: 4.0 },
        'disc-r-pu17': { left: 37.0, top: 219.0, width: 22.0, height: 4.0 },
        'disc-r-pu18': { left: 40.5, top: 240.5, width: 22.0, height: 4.0 },

        // --- Keypad Column Pull-Up Resistors (8x 3.3k) ---
        'disc-r-q9':  { left: 36.4, top: 226.1, width: 19.0, height: 4.0 },
        'disc-r-q10': { left: 40.2, top: 256.0, width: 19.0, height: 4.0 },
        'disc-r-q11': { left: 41.4, top: 270.2, width: 19.0, height: 4.0 },
        'disc-r-q12': { left: 39.8, top: 286.6, width: 19.0, height: 4.0 },
        'disc-r-q13': { left: 41.4, top: 303.2, width: 19.0, height: 4.0 },
        'disc-r-q14': { left: 39.8, top: 318.7, width: 19.0, height: 4.0 },
        'disc-r-q15': { left: 39.8, top: 334.1, width: 19.0, height: 4.0 },
        'disc-r-q16': { left: 39.8, top: 350.4, width: 19.0, height: 4.0 },

        // --- Programmer, Clock & Line Resistors ---
        'disc-res-28v': { left: 257.8, top: 206.3, width: 24.0, height: 5.0 },
        'disc-led-ep': { left: 323.2, top: 205.8, width: 12.0, height: 12.0, rotation: 0 },
        'disc-r-ep-1k8': { left: 332.8, top: 221.9, width: 20.0, height: 4.0, rotation: 90 },
        'disc-r-prog1': { left: 202.2, top: 288.5, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-prog2': { left: 216.3, top: 277.3, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-prog3': { left: 209.9, top: 304.0, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-prog-10k': { left: 202.2, top: 318.0, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-prog4': { left: 263.1, top: 295.2, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-clock': { left: 365.3, top: 120.8, width: 20.0, height: 4.0 },

        // --- Reset & Clock Subsystem Passives ---
        'disc-cap-50p': { left: 238.8, top: 23.0, width: 14.0, height: 10.0, rotation: 90 },
        'disc-cap-101a': { left: 238.0, top: 65.7, width: 14.0, height: 10.0, rotation: 270 },
        'disc-cap-101b': { left: 248.8, top: 63.6, width: 10.0, height: 14.0 },
        'disc-cap-10u': { left: 259.2, top: 57.5, width: 14.0, height: 18.0 },
        'disc-r-rst-10k': { left: 266.8, top: 63.6, width: 20.0, height: 4.0, rotation: 90 },
        'disc-d-rst': { left: 274.9, top: 63.0, width: 20.0, height: 5.0, rotation: 90 },

        // --- Power & Cassette Header Subsystem Passives ---
        'disc-r-ear-330': { left: 282.8, top: 63.6, width: 20.0, height: 4.0, rotation: 90 },
        'disc-d-ear': { left: 290.9, top: 63.0, width: 20.0, height: 5.0, rotation: 90 },
        'disc-cap-ear-203': { left: 298.1, top: 72.9, width: 14.0, height: 10.0 },
        'disc-cap-mic-203': { left: 310.0, top: 63.6, width: 10.0, height: 14.0 },
        'disc-r-mic-10k1': { left: 320.0, top: 63.6, width: 20.0, height: 4.0, rotation: 90 },
        'disc-r-mic-10k2': { left: 326.0, top: 63.6, width: 20.0, height: 4.0, rotation: 90 },
        'disc-r-mic-10k3': { left: 332.0, top: 63.6, width: 20.0, height: 4.0, rotation: 90 },
        'disc-cap-dc-203': { left: 346.0, top: 63.6, width: 10.0, height: 14.0 },
        'disc-cap-dc-100u': { left: 358.0, top: 57.5, width: 18.0, height: 24.0 },
        'disc-cap-7805-203': { left: 382.0, top: 76.0, width: 14.0, height: 10.0 },

        // --- Display Segment Resistors (8x 120R) ---
        'disc-r-seg1': { left: 29.3, top: 377.2, width: 22.0, height: 4.0 },
        'disc-r-seg2': { left: 28.8, top: 384.5, width: 22.0, height: 4.0 },
        'disc-r-seg3': { left: 29.3, top: 392.9, width: 22.0, height: 4.0 },
        'disc-r-seg4': { left: 29.3, top: 400.8, width: 22.0, height: 4.0 },
        'disc-r-seg5': { left: 28.8, top: 408.0, width: 22.0, height: 4.0 },
        'disc-r-seg6': { left: 29.8, top: 416.5, width: 22.0, height: 4.0 },
        'disc-r-seg7': { left: 29.8, top: 423.8, width: 22.0, height: 4.0 },
        'disc-r-seg8': { left: 29.3, top: 431.3, width: 22.0, height: 4.0 },

        // --- Capacitors ---
        'disc-cap-c2': { left: 15.7, top: 4.6, width: 22.0, height: 26.0 },
        'disc-cap-c1': { left: 408.8, top: 13.3, width: 18.0, height: 24.0 },
        'disc-tantalum-cp': { left: 355.4, top: 128.3, width: 18.0, height: 24.0 },
        'disc-cap-vcc1': { left: 401.2, top: 136.6, width: 8.0, height: 8.0 },
        'disc-cap-vcc2': { left: 390.8, top: 132.0, width: 8.0, height: 8.0 },
        'disc-cap-vcc3': { left: 396.9, top: 121.8, width: 8.0, height: 8.0 },

        // --- Power, Jacks, Switches & Ports ---
        'disc-lm7805': { left: 372.3, top: 43.9, width: 49.0, height: 47.0 },
        'disc-jack-dc': { left: 367.1, top: 12.9, width: 29.0, height: 31.9 },
        'disc-jack-ear': { left: 291.8, top: 23.0, width: 36.0, height: 39.8 },
        'disc-jack-mic': { left: 327.0, top: 22.4, width: 37.0, height: 40.9 },
        'disc-hw-rst': { left: 257.1, top: 19.4, width: 32.0, height: 35.2 },
        'disc-int-header': { left: 33.6, top: 33.3, width: 14.0, height: 16.0 },
        'disc-clock-section': { left: 370.7, top: 139.4, width: 60.0, height: 22.0 },

        // --- 7-Segment LED Displays (6x DIP-10 Packages) ---
        'disc-disp-a3': { left: 60.3, top: 379.1, width: 38.0, height: 56.0 },
        'disc-disp-a2': { left: 99.7, top: 380.3, width: 38.0, height: 56.0 },
        'disc-disp-a1': { left: 138.3, top: 380.3, width: 38.0, height: 56.0 },
        'disc-disp-a0': { left: 177.7, top: 380.4, width: 38.0, height: 56.0 },
        'disc-disp-d1': { left: 219.5, top: 380.0, width: 38.0, height: 56.0 },
        'disc-disp-d0': { left: 259.1, top: 379.1, width: 38.0, height: 56.0 },

        // --- Audio / Speaker ---
        'disc-piezo-disk': { left: 339.2, top: 376.0, width: 50.0, height: 50.0 },
        'disc-r-sp33': { left: 414.1, top: 385.2, width: 22.0, height: 4.0, rotation: 90 },
        'disc-r-sp330a': { left: 295.2, top: 381.0, width: 20.0, height: 4.0, rotation: 90 },
        'disc-r-sp330b': { left: 314.1, top: 377.1, width: 20.0, height: 4.0, rotation: 90 },
        'disc-led-audio-grn': { left: 307.5, top: 370.5, width: 12.0, height: 12.0, rotation: 0 },
        'disc-led-halt': { left: 308.5, top: 395.0, width: 12.0, height: 12.0, rotation: 0 },

        // --- Dual 3x4 Keypad Modules ---
        'disc-keypad-left': { left: 86.0, top: 451.0, width: 160.0, height: 206.0 },
        'disc-keypad-right': { left: 249.0, top: 451.0, width: 160.0, height: 206.0 }
    };

    function initUniversalCalibrationController() {
        const board = document.getElementById('board-container') || document.querySelector('.trainer-chassis');
        const hud = document.getElementById('ic-calibration-hud');
        const calibBtn = document.getElementById('btn-calibration-mode');
        const closeBtn = document.getElementById('btn-close-calib');
        const compSelect = document.getElementById('calib-chip-select');
        const inputX = document.getElementById('calib-input-x');
        const inputY = document.getElementById('calib-input-y');
        const inputW = document.getElementById('calib-input-w');
        const inputH = document.getElementById('calib-input-h');
        const copyBtn = document.getElementById('btn-copy-css');
        const resetBtn = document.getElementById('btn-reset-positions');
        const stepBtns = document.querySelectorAll('.step-btn');
        const nudgeBtns = document.querySelectorAll('.nudge-btn');
        const btnSizeSmaller = document.getElementById('btn-size-smaller');
        const btnSizeBigger = document.getElementById('btn-size-bigger');
        const btnRotLeft = document.getElementById('btn-rot-left');
        const btnRotRight = document.getElementById('btn-rot-right');
        const rotBadge = document.getElementById('calib-rot-val');

        isCalibrating = false;
        let selectedCompId = 'ic-u1';
        let currentStep = 1.0;
        let currentCoords = Object.assign({}, DEFAULT_CALIBRATION_COORDS);

        // Load custom saved coordinates from localStorage if any
        try {
            const saved = localStorage.getItem('abc80_calibration_coords') || localStorage.getItem('abc80_ic_coords');
            if (saved) {
                const parsed = JSON.parse(saved);
                Object.assign(currentCoords, parsed);
            }
        } catch (e) {
            console.warn('[Calibration] Failed to load saved coordinates:', e);
        }

        // Clean up legacy monolithic display module key if present
        if (currentCoords['disc-display-module']) {
            delete currentCoords['disc-display-module'];
            saveCoords();
        }

        // Clean up legacy bogus cont0 socket if present
        if (currentCoords['disc-conto-socket']) {
            delete currentCoords['disc-conto-socket'];
            saveCoords();
        }

        // Auto-migrate Power & Cassette Header subsystem passives if missing
        const headerPassiveKeys = [
            'disc-r-ear-330', 'disc-d-ear', 'disc-cap-ear-203', 'disc-cap-mic-203',
            'disc-r-mic-10k1', 'disc-r-mic-10k2', 'disc-r-mic-10k3',
            'disc-cap-dc-203', 'disc-cap-dc-100u', 'disc-cap-7805-203'
        ];
        let headerMigrated = false;
        headerPassiveKeys.forEach(k => {
            if (!currentCoords[k] && DEFAULT_CALIBRATION_COORDS[k]) {
                currentCoords[k] = Object.assign({}, DEFAULT_CALIBRATION_COORDS[k]);
                headerMigrated = true;
            }
        });
        if (headerMigrated) {
            saveCoords();
        }

        // Auto-migrate displays to updated taller and wider dimensions
        const dispKeys = ['disc-disp-a3', 'disc-disp-a2', 'disc-disp-a1', 'disc-disp-a0', 'disc-disp-d1', 'disc-disp-d0'];
        let dispMigrated = false;
        dispKeys.forEach(k => {
            if (currentCoords[k] && (currentCoords[k].width < 35 || currentCoords[k].height < 50)) {
                currentCoords[k].width = 38.0;
                currentCoords[k].height = 56.0;
                dispMigrated = true;
            }
        });
        if (dispMigrated) {
            saveCoords();
        }

        // Auto-migrate legacy narrow U4 footprint in cached localStorage
        if (currentCoords['ic-u4'] && currentCoords['ic-u4'].width < 40) {
            currentCoords['ic-u4'].width = 50.0;
            currentCoords['ic-u4'].left = 289.0;
            currentCoords['ic-u4'].top = 88.0;
            saveCoords();
        }

        // Initialize and migrate dual 3x4 keypad modules to user calibrated coordinates
        if (!currentCoords['disc-keypad-left'] || currentCoords['disc-keypad-left'].left === 78.0) {
            currentCoords['disc-keypad-left'] = Object.assign({}, DEFAULT_CALIBRATION_COORDS['disc-keypad-left']);
            saveCoords();
        }
        if (!currentCoords['disc-keypad-right'] || currentCoords['disc-keypad-right'].left === 251.0) {
            currentCoords['disc-keypad-right'] = Object.assign({}, DEFAULT_CALIBRATION_COORDS['disc-keypad-right']);
            saveCoords();
        }

        // Auto-migrate disc-cap-ear-203 and disc-cap-c1 to frozen master coordinates
        if (currentCoords['disc-cap-ear-203'] && currentCoords['disc-cap-ear-203'].left === 291.0) {
            currentCoords['disc-cap-ear-203'] = Object.assign({}, DEFAULT_CALIBRATION_COORDS['disc-cap-ear-203']);
            saveCoords();
        }
        if (currentCoords['disc-cap-c1'] && currentCoords['disc-cap-c1'].top > 100) {
            currentCoords['disc-cap-c1'] = Object.assign({}, DEFAULT_CALIBRATION_COORDS['disc-cap-c1']);
            saveCoords();
        }

        // Apply coordinates to DOM
        function applyCoordinates() {
            Object.keys(currentCoords).forEach(id => {
                const el = document.getElementById(id);
                if (!el) return;
                const c = currentCoords[id];
                el.style.left = c.left + 'px';
                el.style.top = c.top + 'px';
                if (c.width !== undefined) el.style.width = c.width + 'px';
                if (c.height !== undefined) el.style.height = c.height + 'px';

                // Also update child DIP body, socket frame, TO-92, LED, audio jack, DC jack, reset button, or regulator
                const dip = el.querySelector('.dip-ic, .empty-ladder-socket, .socket-u3-frame, .socket-u7-frame, .dip-socket-frame, .silk-abc80-eprom-text, .to92-body, .discrete-led-3mm, .phone-jack-35mm, .dc-barrel-jack, .hw-reset-tactile-btn, .lm7805-regulator');
                if (dip && c.width !== undefined && c.height !== undefined) {
                    dip.style.width = c.width + 'px';
                    dip.style.height = c.height + 'px';
                }

                // Apply 90-degree component orientation
                if (c.rotation) {
                    el.style.transform = 'rotate(' + c.rotation + 'deg)';
                    el.style.transformOrigin = 'center center';
                } else {
                    el.style.transform = '';
                    el.style.transformOrigin = '';
                }
            });
        }

        applyCoordinates();

        // Update HUD display for the selected component
        function updateHUD() {
            if (!compSelect) return;
            compSelect.value = selectedCompId;
            const c = currentCoords[selectedCompId] || { left: 0, top: 0, width: 0, height: 0, rotation: 0 };
            if (inputX) inputX.value = Number(c.left.toFixed(1));
            if (inputY) inputY.value = Number(c.top.toFixed(1));
            if (inputW) inputW.value = Number((c.width || 0).toFixed(1));
            if (inputH) inputH.value = Number((c.height || 0).toFixed(1));
            if (rotBadge) rotBadge.textContent = (c.rotation || 0) + '°';

            // Highlight selected component across the board
            document.querySelectorAll('.ic-socket-package, .calibratable-component').forEach(p => {
                p.classList.toggle('selected-for-calibration', p.id === selectedCompId);
            });
        }

        function selectComponent(id) {
            if (!currentCoords[id]) return;
            selectedCompId = id;
            updateHUD();
        }

        function saveCoords() {
            try {
                localStorage.setItem('abc80_calibration_coords', JSON.stringify(currentCoords));
            } catch (e) {
                console.warn('[Calibration] Failed to save coordinates:', e);
            }
        }

        // Toggle calibration mode
        function setCalibrationMode(active) {
            isCalibrating = active;
            if (board) board.classList.toggle('calibration-active', active);
            if (hud) hud.classList.toggle('hidden', !active);
            if (calibBtn) calibBtn.classList.toggle('header-btn-active', active);
            if (active) {
                updateHUD();
            } else {
                document.querySelectorAll('.ic-socket-package, .calibratable-component').forEach(p => {
                    p.classList.remove('selected-for-calibration');
                });
            }
        }

        if (calibBtn) {
            calibBtn.addEventListener('click', () => setCalibrationMode(!isCalibrating));
        }

        if (closeBtn) {
            closeBtn.addEventListener('click', () => setCalibrationMode(false));
        }

        if (compSelect) {
            compSelect.addEventListener('change', (e) => selectComponent(e.target.value));
        }

        // Opacity / X-Ray slider for empty sockets & silkscreen overlay
        const opacitySlider = document.getElementById('calib-opacity-slider');
        const opacityVal = document.getElementById('calib-opacity-val');
        if (opacitySlider && opacityVal) {
            opacitySlider.addEventListener('input', (e) => {
                const val = parseFloat(e.target.value);
                opacityVal.textContent = Math.round(val * 100) + '%';
                document.querySelectorAll('.empty-ladder-socket, #silk-abc80-eprom').forEach(el => {
                    el.style.opacity = val;
                });
            });
        }

        // Manual coordinate input handlers
        function onInputChange() {
            const c = currentCoords[selectedCompId];
            if (!c) return;
            if (inputX) c.left = parseFloat(inputX.value) || 0;
            if (inputY) c.top = parseFloat(inputY.value) || 0;
            if (inputW) c.width = parseFloat(inputW.value) || 0;
            if (inputH) c.height = parseFloat(inputH.value) || 0;
            applyCoordinates();
            saveCoords();
        }

        [inputX, inputY, inputW, inputH].forEach(input => {
            if (input) input.addEventListener('input', onInputChange);
        });

        // Step buttons
        stepBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                stepBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                currentStep = parseFloat(btn.getAttribute('data-step')) || 1.0;
            });
        });

        // Nudge buttons
        function nudge(axis, delta) {
            const c = currentCoords[selectedCompId];
            if (!c) return;
            if (axis === 'x') c.left = Math.round((c.left + delta) * 10) / 10;
            if (axis === 'y') c.top = Math.round((c.top + delta) * 10) / 10;
            applyCoordinates();
            updateHUD();
            saveCoords();
        }

        nudgeBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const axis = btn.getAttribute('data-axis');
                const dir = parseFloat(btn.getAttribute('data-delta')) || 0;
                nudge(axis, dir * currentStep);
            });
        });

        // Proportional resizing (Envelope 5)
        function resizeSelected(delta) {
            const c = currentCoords[selectedCompId];
            if (!c) return;
            const el = document.getElementById(selectedCompId);
            let w = c.width !== undefined ? c.width : (el ? el.offsetWidth : 18);
            let h = c.height !== undefined ? c.height : (el ? el.offsetHeight : 12);
            if (!w || w <= 0) w = 18;
            if (!h || h <= 0) h = 12;
            const ratio = h / w;
            c.width = Math.max(4, Math.round((w + delta) * 10) / 10);
            c.height = Math.max(4, Math.round((h + delta * ratio) * 10) / 10);
            applyCoordinates();
            updateHUD();
            saveCoords();
        }

        // 90-degree component orientation engine (Envelope 6)
        function rotateSelected(deltaDeg) {
            const c = currentCoords[selectedCompId];
            if (!c) return;
            let rot = ((c.rotation || 0) + deltaDeg) % 360;
            if (rot < 0) rot += 360;
            c.rotation = Math.round(rot / 90) * 90 % 360;
            applyCoordinates();
            updateHUD();
            saveCoords();
        }

        if (btnSizeSmaller) {
            btnSizeSmaller.addEventListener('click', () => resizeSelected(-currentStep));
        }
        if (btnSizeBigger) {
            btnSizeBigger.addEventListener('click', () => resizeSelected(currentStep));
        }
        if (btnRotLeft) {
            btnRotLeft.addEventListener('click', () => rotateSelected(-90));
        }
        if (btnRotRight) {
            btnRotRight.addEventListener('click', () => rotateSelected(90));
        }

        // Keyboard arrow nudge & hotkeys
        window.addEventListener('keydown', (e) => {
            if (!isCalibrating) return;
            if (document.activeElement && document.activeElement.tagName === 'INPUT') return;

            if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key)) {
                e.preventDefault();

                let step = currentStep;
                if (e.shiftKey) step = 5.0;
                else if (e.altKey) step = 0.2;

                if (e.key === 'ArrowUp') nudge('y', -step);
                if (e.key === 'ArrowDown') nudge('y', step);
                if (e.key === 'ArrowLeft') nudge('x', -step);
                if (e.key === 'ArrowRight') nudge('x', step);
            } else if (e.key === '+' || e.key === '=') {
                e.preventDefault();
                resizeSelected(currentStep);
            } else if (e.key === '-' || e.key === '_') {
                e.preventDefault();
                resizeSelected(-currentStep);
            } else if (e.key === '[' || (e.key.toLowerCase() === 'r' && !e.shiftKey)) {
                e.preventDefault();
                rotateSelected(-90);
            } else if (e.key === ']' || (e.key.toLowerCase() === 'r' && e.shiftKey)) {
                e.preventDefault();
                rotateSelected(90);
            }
        });

        // Drag & Drop on all calibratable components and IC packages
        document.querySelectorAll('.ic-socket-package, .calibratable-component').forEach(comp => {
            comp.addEventListener('pointerdown', (e) => {
                if (!isCalibrating) return;
                e.preventDefault();
                e.stopPropagation();

                const targetId = comp.id;
                if (!targetId || !currentCoords[targetId]) return;

                selectComponent(targetId);
                comp.classList.add('is-dragging');

                const c = currentCoords[targetId];
                const startPointerX = e.clientX;
                const startPointerY = e.clientY;
                const origLeft = c.left;
                const origTop = c.top;

                function onPointerMove(moveEvent) {
                    const currentScale = (typeof viewportState !== 'undefined' && viewportState.scale) ? viewportState.scale : 1.0;
                    const dx = (moveEvent.clientX - startPointerX) / currentScale;
                    const dy = (moveEvent.clientY - startPointerY) / currentScale;
                    c.left = Math.round((origLeft + dx) * 10) / 10;
                    c.top = Math.round((origTop + dy) * 10) / 10;
                    applyCoordinates();
                    updateHUD();
                }

                function onPointerUp() {
                    comp.classList.remove('is-dragging');
                    window.removeEventListener('pointermove', onPointerMove);
                    window.removeEventListener('pointerup', onPointerUp);
                    saveCoords();
                }

                window.addEventListener('pointermove', onPointerMove);
                window.addEventListener('pointerup', onPointerUp);
            });
        });

        // Copy CSS Rules grouped by category
        if (copyBtn) {
            copyBtn.addEventListener('click', () => {
                let css = '/* ==========================================================================\n' +
                          '   Calibrated ABC-80 PCB Footprints & Placements (Page 98 Drilled Master)\n' +
                          '   ========================================================================== */\n\n';

                const categories = [
                    {
                        title: 'DIP ICs & Sockets',
                        ids: ['ic-u1', 'ic-u2', 'ic-u3', 'ic-u4', 'ic-u5', 'ic-u6', 'ic-u7', 'ic-u8', 'ic-u9', 'ic-u10', 'ic-u11'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n#${id} .dip-ic, #${id} .empty-ladder-socket, #${id} .socket-u3-frame, #${id} .socket-u7-frame { width: ${c.width}px !important; height: ${c.height}px !important; }\n\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Silkscreen Overlay',
                        ids: ['silk-abc80-eprom'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n#${id} .silk-abc80-eprom-text { width: ${c.width}px !important; height: ${c.height}px !important; }\n\n`;
                            return r;
                        }
                    },
                    {
                        title: 'TO-92 Transistors (+50% Enlarged)',
                        ids: ['disc-q1', 'disc-q2', 'disc-q3', 'disc-q4', 'disc-q5', 'disc-q6', 'disc-q7', 'disc-q8', 'disc-q9', 'disc-q10', 'disc-q11', 'disc-q12', 'disc-q13', 'disc-q14', 'disc-q15', 'disc-q16', 'disc-q17'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Bus Pull-Up Resistors (18x 10k)',
                        ids: ['disc-r-pu1', 'disc-r-pu2', 'disc-r-pu3', 'disc-r-pu4', 'disc-r-pu5', 'disc-r-pu6', 'disc-r-pu7', 'disc-r-pu8', 'disc-r-pu9', 'disc-r-pu10', 'disc-r-pu11', 'disc-r-pu12', 'disc-r-pu13', 'disc-r-pu14', 'disc-r-pu15', 'disc-r-pu16', 'disc-r-pu17', 'disc-r-pu18'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Keypad Column Pull-Up Resistors (8x 3.3k)',
                        ids: ['disc-r-q9', 'disc-r-q10', 'disc-r-q11', 'disc-r-q12', 'disc-r-q13', 'disc-r-q14', 'disc-r-q15', 'disc-r-q16'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Programmer & Clock Bias Resistors',
                        ids: ['disc-res-28v', 'disc-led-ep', 'disc-r-ep-1k8', 'disc-r-prog1', 'disc-r-prog2', 'disc-r-prog3', 'disc-r-prog-10k', 'disc-r-prog4', 'disc-r-clock'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Display Segment Resistors (8x 120R)',
                        ids: ['disc-r-seg1', 'disc-r-seg2', 'disc-r-seg3', 'disc-r-seg4', 'disc-r-seg5', 'disc-r-seg6', 'disc-r-seg7', 'disc-r-seg8'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Reset & Clock Subsystem Passives',
                        ids: ['disc-cap-50p', 'disc-cap-101a', 'disc-cap-101b', 'disc-cap-10u', 'disc-r-rst-10k', 'disc-d-rst'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Power & Cassette Header Subsystem Passives',
                        ids: [
                            'disc-r-ear-330', 'disc-d-ear', 'disc-cap-ear-203', 'disc-cap-mic-203',
                            'disc-r-mic-10k1', 'disc-r-mic-10k2', 'disc-r-mic-10k3',
                            'disc-cap-dc-203', 'disc-cap-dc-100u', 'disc-cap-7805-203'
                        ],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Capacitors',
                        ids: ['disc-cap-c2', 'disc-cap-c1', 'disc-tantalum-cp', 'disc-cap-vcc1', 'disc-cap-vcc2', 'disc-cap-vcc3'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Power, Jacks & Hardware',
                        ids: ['disc-lm7805', 'disc-jack-dc', 'disc-jack-ear', 'disc-jack-mic', 'disc-hw-rst', 'disc-int-header', 'disc-clock-section'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (c.width !== undefined && c.height !== undefined) r += ` width: ${c.width}px; height: ${c.height}px;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: '7-Segment LED Displays (6x)',
                        ids: ['disc-disp-a3', 'disc-disp-a2', 'disc-disp-a1', 'disc-disp-a0', 'disc-disp-d1', 'disc-disp-d0'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Audio / Speaker',
                        ids: ['disc-piezo-disk', 'disc-r-sp33', 'disc-r-sp330a', 'disc-r-sp330b', 'disc-led-audio-grn', 'disc-led-halt'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (id !== 'disc-piezo-disk' && c.width !== undefined && c.height !== undefined) {
                                r += ` width: ${c.width}px; height: ${c.height}px;`;
                            }
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    },
                    {
                        title: 'Dual 3x4 Keypad Modules',
                        ids: ['disc-keypad-left', 'disc-keypad-right'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important;`;
                            if (c.rotation) r += ` transform: rotate(${c.rotation}deg); transform-origin: center center;`;
                            r += ` }\n`;
                            return r;
                        }
                    }
                ];

                categories.forEach(cat => {
                    css += `/* --- ${cat.title} --- */\n`;
                    cat.ids.forEach(id => {
                        const c = currentCoords[id];
                        if (c) {
                            css += cat.format(id, c);
                        }
                    });
                    css += '\n';
                });

                if (navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(css).then(() => {
                        const origText = copyBtn.textContent;
                        copyBtn.textContent = '✓ CSS Copied!';
                        setTimeout(() => { copyBtn.textContent = origText; }, 2000);
                    }).catch(() => {
                        console.log(css);
                        const origText = copyBtn.textContent;
                        copyBtn.textContent = '✓ Logged to Console';
                        setTimeout(() => { copyBtn.textContent = origText; }, 2000);
                    });
                } else {
                    console.log(css);
                    alert('CSS rules printed to browser console!');
                }
            });
        }

        // Reset positions
        if (resetBtn) {
            resetBtn.addEventListener('click', () => {
                if (confirm('Reset all PCB component positions to Page 98 pad grid defaults?')) {
                    localStorage.removeItem('abc80_calibration_coords');
                    localStorage.removeItem('abc80_ic_coords');
                    currentCoords = Object.assign({}, DEFAULT_CALIBRATION_COORDS);
                    applyCoordinates();
                    updateHUD();
                }
            });
        }
    }

    if (document.readyState === 'loading') {
        window.addEventListener('DOMContentLoaded', initAll);
    } else {
        initAll();
    }
})();
