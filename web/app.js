/*
 * app.js - ABC-80 Single-Board Trainer Web Controller & Host WebSocket Client
 *
 * Copyright (C) 2026, Charles Chiou
 */

(function () {
    'use strict';

    // 7-Segment SVG Segment Bitmask Map (PB0..PB7: a,b,c,d,e,f,g,dp)
    const SEG_MAP = {
        '0': 0b00111111,
        '1': 0b00000110,
        '2': 0b01011011,
        '3': 0b01001111,
        '4': 0b01100110,
        '5': 0b01101101,
        '6': 0b01111101,
        '7': 0b00000111,
        '8': 0b01111111,
        '9': 0b01101111,
        'A': 0b01110111,
        'B': 0b01111100,
        'C': 0b00111001,
        'D': 0b01011110,
        'E': 0b01111001,
        'F': 0b01110001,
        '-': 0b01000000,
        ' ': 0b00000000
    };

    const SVG_7SEG = `
        <svg class="seg-svg" viewBox="0 0 32 52">
            <polygon class="seg-path seg-a" points="6,4  24,4  20,8  10,8" />
            <polygon class="seg-path seg-b" points="25,5  28,8  25,23 21,20" />
            <polygon class="seg-path seg-c" points="24,27 27,30 24,45 20,42" />
            <polygon class="seg-path seg-d" points="6,46 24,46 20,42 10,42" />
            <polygon class="seg-path seg-e" points="5,27 9,25  9,42  5,45" />
            <polygon class="seg-path seg-f" points="5,8  9,10  9,23  5,20" />
            <polygon class="seg-path seg-g" points="8,24 22,24 24,26 22,28 8,28 6,26" />
            <circle class="seg-path seg-dp" cx="29" cy="46" r="2.2" />
        </svg>
    `;

    // Local & Host-Synced State
    const state = {
        soundEnabled: true,
        wsConnected: false,
        inputMode: 'ADRS',
        address: 0x1000,
        data: 0x3E,
        displayDigits: ['1', '0', '0', '0', '3', 'E'],
        registers: {
            pc: 0x0000,
            sp: 0x17FE,
            af: 0x0040,
            bc: 0x0006,
            de: 0x1000,
            hl: 0x2000,
            ix: 0x0000,
            iy: 0x0000,
            flags: { s: 0, z: 1, h: 0, pv: 0, n: 0, c: 0 }
        },
        cycles: 1200,
        tapeState: 'STOPPED'
    };

    let isCalibrating = false;

    // WebSocket Host Gateway Connection
    let ws = null;
    function connectHostWebSocket() {
        const wsUrl = (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + (window.location.host || 'localhost:8080') + '/ws';
        const wsDot = document.getElementById('host-ws-dot');
        const wsText = document.getElementById('host-ws-text');

        try {
            ws = new WebSocket(wsUrl);
            ws.onopen = function () {
                state.wsConnected = true;
                if (wsDot) { wsDot.className = 'status-dot dot-connected'; }
                if (wsText) { wsText.textContent = 'HOST: ONLINE'; }
                console.log('[ABC-80 Web] Connected to x86_64 emulator backend.');
            };

            ws.onmessage = function (event) {
                try {
                    const msg = JSON.parse(event.data);
                    if (msg.type === 'telemetry') {
                        if (msg.displayDigits) {
                            state.displayDigits = msg.displayDigits;
                            updateDisplayDigits();
                        }
                        if (msg.registers) {
                            state.registers = msg.registers;
                            updateInspector();
                        }
                        if (msg.cycles !== undefined) {
                            state.cycles = msg.cycles;
                            document.getElementById('cycle-counter').textContent = state.cycles.toLocaleString();
                        }
                    }
                } catch (e) {}
            };

            ws.onclose = function () {
                state.wsConnected = false;
                if (wsDot) { wsDot.className = 'status-dot dot-offline'; }
                if (wsText) { wsText.textContent = 'HOST: OFFLINE (SIM)'; }
                setTimeout(connectHostWebSocket, 3000);
            };

            ws.onerror = function () {
                ws.close();
            };
        } catch (e) {
            if (wsDot) { wsDot.className = 'status-dot dot-offline'; }
            if (wsText) { wsText.textContent = 'HOST: OFFLINE (SIM)'; }
            setTimeout(connectHostWebSocket, 3000);
        }
    }

    function sendHostCommand(cmd) {
        if (state.wsConnected && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(cmd));
        }
    }

    // Web Audio Synthesizer
    let audioCtx = null;
    function initAudio() {
        if (!audioCtx) {
            const AudioContextClass = window.AudioContext || window.webkitAudioContext;
            if (AudioContextClass) audioCtx = new AudioContextClass();
        }
    }

    function playKeyClickSound() {
        if (!state.soundEnabled || !audioCtx) return;
        try {
            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();
            osc.type = 'triangle';
            osc.frequency.setValueAtTime(650, audioCtx.currentTime);
            osc.frequency.exponentialRampToValueAtTime(80, audioCtx.currentTime + 0.02);
            gain.gain.setValueAtTime(0.18, audioCtx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.02);
            osc.connect(gain);
            gain.connect(audioCtx.destination);
            osc.start();
            osc.stop(audioCtx.currentTime + 0.025);
        } catch (e) {}
    }

    function playBeepSound(freq, duration) {
        if (!state.soundEnabled || !audioCtx) return;
        try {
            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();
            osc.type = 'square';
            osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
            gain.gain.setValueAtTime(0.08, audioCtx.currentTime);
            gain.gain.linearRampToValueAtTime(0.001, audioCtx.currentTime + duration);
            osc.connect(gain);
            gain.connect(audioCtx.destination);
            osc.start();
            osc.stop(audioCtx.currentTime + duration);
        } catch (e) {}
    }

    function initDisplayDigits() {
        const digitIds = ['digit-a3', 'digit-a2', 'digit-a1', 'digit-a0', 'digit-d1', 'digit-d0'];
        digitIds.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.innerHTML = SVG_7SEG;
        });
        updateDisplayDigits();
    }

    function setSegmentGlyph(digitElement, char, showDp = false) {
        if (!digitElement) return;
        const mask = SEG_MAP[char.toUpperCase()] || 0;
        const segClasses = ['seg-a', 'seg-b', 'seg-c', 'seg-d', 'seg-e', 'seg-f', 'seg-g'];
        segClasses.forEach((cls, idx) => {
            const path = digitElement.querySelector(`.${cls}`);
            if (path) {
                if ((mask & (1 << idx)) !== 0) path.classList.add('active');
                else path.classList.remove('active');
            }
        });

        const dpPath = digitElement.querySelector('.seg-dp');
        if (dpPath) {
            if (showDp) dpPath.classList.add('active');
            else dpPath.classList.remove('active');
        }
    }

    function updateDisplayDigits() {
        const digitIds = ['digit-a3', 'digit-a2', 'digit-a1', 'digit-a0', 'digit-d1', 'digit-d0'];
        digitIds.forEach((id, idx) => {
            const el = document.getElementById(id);
            const val = state.displayDigits[idx] || ' ';
            setSegmentGlyph(el, val, false);
        });
    }

    function setDisplayAddressData(addr, data) {
        state.address = addr & 0xFFFF;
        state.data = data & 0xFF;
        const hexAddr = state.address.toString(16).toUpperCase().padStart(4, '0');
        const hexData = state.data.toString(16).toUpperCase().padStart(2, '0');
        state.displayDigits = [hexAddr[0], hexAddr[1], hexAddr[2], hexAddr[3], hexData[0], hexData[1]];
        updateDisplayDigits();
    }

    function handleKeypadPress(key, code) {
        initAudio();
        playKeyClickSound();

        const btn = document.querySelector(`.tactile-switch[data-key="${key}"], .overlay-key[data-key="${key}"], .key-btn[data-key="${key}"]`);
        if (btn) {
            btn.classList.add('key-pressed');
            setTimeout(() => btn.classList.remove('key-pressed'), 120);
        }

        // Send to host backend
        sendHostCommand({ type: 'keypress', key: key, code: code });

        // Standalone simulation fallback
        if (!state.wsConnected) {
            if (/^[0-9A-F]$/i.test(key)) {
                const hexDigit = parseInt(key, 16);
                if (state.inputMode === 'ADRS') {
                    state.address = ((state.address << 4) | hexDigit) & 0xFFFF;
                } else {
                    state.data = ((state.data << 4) | hexDigit) & 0xFF;
                }
                setDisplayAddressData(state.address, state.data);
                updateInspector();
            } else if (key === 'ADRS') {
                state.inputMode = 'ADRS';
                playBeepSound(1000, 0.04);
            } else if (key === 'DATA') {
                state.inputMode = 'DATA';
                playBeepSound(1200, 0.04);
            } else if (key === '+') {
                state.address = (state.address + 1) & 0xFFFF;
                state.data = (state.address * 3 + 7) & 0xFF;
                setDisplayAddressData(state.address, state.data);
                updateInspector();
            } else if (key === '-') {
                state.cycles += 4;
                state.registers.pc = (state.registers.pc + 1) & 0xFFFF;
                state.address = state.registers.pc;
                state.data = 0x3E;
                setDisplayAddressData(state.address, state.data);
                playBeepSound(1400, 0.05);
                updateInspector();
            } else if (key === 'RST') {
                state.address = 0x0000;
                state.data = 0x31;
                state.registers.pc = 0x0000;
                state.registers.sp = 0x17FE;
                setDisplayAddressData(state.address, state.data);
                playBeepSound(440, 0.12);
                updateInspector();
            } else if (key === 'GO EXEC') {
                playBeepSound(2000, 0.1);
            }
        }
    }

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

        const disasmList = [
            { addr: '0000', bytes: '31 FE 17', mnem: 'LD', op: 'SP, 17FEH' },
            { addr: '0003', bytes: '3E 98', mnem: 'LD', op: 'A, 98H' },
            { addr: '0005', bytes: '32 03 20', mnem: 'LD', op: '(2003H), A' },
            { addr: '0008', bytes: 'CD 20 02', mnem: 'CALL', op: '0220H' },
            { addr: '000B', bytes: 'AF', mnem: 'XOR', op: 'A' },
            { addr: '000C', bytes: '32 00 10', mnem: 'LD', op: '(1000H), A' }
        ];

        const disasmContainer = document.getElementById('disasm-container');
        if (disasmContainer) {
            disasmContainer.innerHTML = disasmList.map((item, idx) => `
                <div class="disasm-line ${idx === 0 ? 'current-pc' : ''}">
                    <span class="disasm-addr">${item.addr}</span>
                    <span class="disasm-bytes">${item.bytes}</span>
                    <span class="disasm-mnemonic">${item.mnem}</span>
                    <span class="disasm-operands">${item.op}</span>
                </div>
            `).join('');
        }

        populateHexEditor(0x0000);
        updateInspector();
    }

    function populateHexEditor(baseAddr) {
        const hexContainer = document.getElementById('hex-editor-table');
        if (!hexContainer) return;
        const rows = [];
        const sampleRomBytes = [
            0x31, 0xFE, 0x17, 0x3E, 0x98, 0x32, 0x03, 0x20, 0xCD, 0x20, 0x02, 0xAF, 0x32, 0x00, 0x10, 0xCD,
            0x50, 0x01, 0xC3, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        ];

        for (let r = 0; r < 2; r++) {
            const addr = (baseAddr + r * 16).toString(16).toUpperCase().padStart(4, '0');
            const bytesSlice = sampleRomBytes.slice(r * 16, r * 16 + 16);
            const hexSpans = bytesSlice.map(b => `<span class="hex-byte">${b.toString(16).toUpperCase().padStart(2, '0')}</span>`).join(' ');
            const asciiChars = bytesSlice.map(b => (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.').join('');
            rows.push(`
                <div class="hex-row">
                    <span class="hex-addr">${addr}:</span>
                    <span class="hex-bytes">${hexSpans}</span>
                    <span class="hex-ascii">${asciiChars}</span>
                </div>
            `);
        }
        hexContainer.innerHTML = rows.join('');
    }

    function updateInspector() {
        document.getElementById('cycle-counter').textContent = state.cycles.toLocaleString();
        document.getElementById('reg-pc').textContent = '0x' + state.registers.pc.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-sp').textContent = '0x' + state.registers.sp.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-af').textContent = '0x' + state.registers.af.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-bc').textContent = '0x' + state.registers.bc.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-de').textContent = '0x' + state.registers.de.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-hl').textContent = '0x' + state.registers.hl.toString(16).toUpperCase().padStart(4, '0');
    }

    function setupKeyboardListeners() {
        window.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT') return;
            const key = e.key.toUpperCase();
            if (/^[0-9A-F]$/.test(key)) {
                handleKeypadPress(key, parseInt(key, 16));
            } else if (e.key === 'ArrowRight' || e.key === '+') {
                handleKeypadPress('+', 0x13);
            } else if (e.key === ' ' || e.key === '-' || e.key === 'F10') {
                e.preventDefault();
                handleKeypadPress('-', 0x10);
            } else if (e.key === 'Enter' || key === 'G') {
                e.preventDefault();
                handleKeypadPress('GO EXEC', 0x11);
            } else if (e.key === 'Escape' || e.key === 'F5') {
                handleKeypadPress('RST', 0x18);
            } else if (key === 'A' || key === 'M') {
                handleKeypadPress('ADRS', 0x14);
            } else if (key === 'D') {
                handleKeypadPress('DATA', 0x12);
            } else if (key === 'W') {
                handleKeypadPress('TO TAPE', 0x17);
            } else if (key === 'L') {
                handleKeypadPress('FROM TAPE', 0x16);
            } else if (e.key === 'F12') {
                e.preventDefault();
                document.getElementById('btn-toggle-inspector').click();
            }
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

    function initAll() {
        initDisplayDigits();
        initInspector();
        initScope();
        initViewportNavigation();
        setupKeyboardListeners();
        connectHostWebSocket();

        document.querySelectorAll('.tactile-switch, .overlay-key, .key-btn, .hw-reset-tactile-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const key = btn.getAttribute('data-key');
                const code = parseInt(btn.getAttribute('data-code'), 16);
                if (key) handleKeypadPress(key, code);
            });
        });

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

        const btnPlay = document.getElementById('btn-tape-play');
        const btnRec = document.getElementById('btn-tape-rec');
        const btnStop = document.getElementById('btn-tape-stop');
        const btnRew = document.getElementById('btn-tape-rew');

        if (btnPlay) btnPlay.addEventListener('click', () => { initAudio(); state.tapeState = 'PLAYING'; sendHostCommand({ type: 'tape', action: 'play' }); });
        if (btnRec) btnRec.addEventListener('click', () => { initAudio(); state.tapeState = 'RECORDING'; sendHostCommand({ type: 'tape', action: 'record' }); });
        if (btnStop) btnStop.addEventListener('click', () => { initAudio(); state.tapeState = 'STOPPED'; sendHostCommand({ type: 'tape', action: 'stop' }); });
        if (btnRew) btnRew.addEventListener('click', () => { initAudio(); state.tapeState = 'STOPPED'; playKeyClickSound(); sendHostCommand({ type: 'tape', action: 'rewind' }); });

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
        'disc-q7':  { left: 309.6, top: 365.6, width: 18.0, height: 12.0 },
        'disc-q8':  { left: 313.6, top: 392.8, width: 18.0, height: 12.0 },
        'disc-q9':  { left: 21.1, top: 233.5, width: 18.0, height: 12.0 },
        'disc-q10': { left: 21.4, top: 247.2, width: 18.0, height: 12.0 },
        'disc-q11': { left: 20.6, top: 277.8, width: 18.0, height: 12.0 },
        'disc-q12': { left: 21.0, top: 263.0, width: 18.0, height: 12.0 },
        'disc-q13': { left: 21.8, top: 292.0, width: 18.0, height: 12.0 },
        'disc-q14': { left: 21.4, top: 307.5, width: 18.0, height: 12.0 },
        'disc-q15': { left: 21.4, top: 324.1, width: 18.0, height: 12.0 },
        'disc-q16': { left: 21.8, top: 339.6, width: 18.0, height: 12.0 },
        'disc-q17': { left: 196.0, top: 218.0, width: 18.0, height: 15.0 },

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
        'disc-res-28v': { left: 383.7, top: 132.2, width: 24.0, height: 5.0 },
        'disc-r-prog1': { left: 208.0, top: 236.0, width: 22.0, height: 4.0 },
        'disc-r-prog2': { left: 208.0, top: 244.7, width: 22.0, height: 4.0 },
        'disc-r-prog3': { left: 208.0, top: 252.0, width: 22.0, height: 4.0 },
        'disc-r-prog4': { left: 208.0, top: 260.0, width: 22.0, height: 4.0 },
        'disc-r-clock': { left: 216.0, top: 83.0, width: 20.0, height: 4.0 },

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
        'disc-cap-c2': { left: 17.6, top: 5.2, width: 22.0, height: 26.0 },
        'disc-cap-c1': { left: 349.0, top: 38.0, width: 18.0, height: 24.0 },
        'disc-tantalum-cp': { left: 355.4, top: 128.3, width: 18.0, height: 24.0 },
        'disc-cap-vcc1': { left: 375.3, top: 95.3, width: 8.0, height: 8.0 },
        'disc-cap-vcc2': { left: 352.0, top: 75.3, width: 8.0, height: 8.0 },
        'disc-cap-vcc3': { left: 409.3, top: 78.0, width: 8.0, height: 8.0 },

        // --- Power, Jacks, Switches & Ports ---
        'disc-lm7805': { left: 388.0, top: 4.0, width: 48.0, height: 46.0 },
        'disc-jack-dc': { left: 350.0, top: 8.0, width: 20.0, height: 22.0 },
        'disc-jack-ear': { left: 288.0, top: 8.0, width: 18.0, height: 20.0 },
        'disc-jack-mic': { left: 320.0, top: 8.0, width: 18.0, height: 20.0 },
        'disc-hw-rst': { left: 247.0, top: 6.0, width: 20.0, height: 22.0 },
        'disc-int-header': { left: 34.2, top: 36.4, width: 14.0, height: 16.0 },
        'disc-conto-socket': { left: 30.0, top: 359.0, width: 26.0, height: 12.0 },
        'disc-clock-section': { left: 260.7, top: 53.3, width: 60.0, height: 22.0 },

        // --- 7-Segment LED Displays (6x DIP-10 Packages) ---
        'disc-disp-a3': { left: 60.3, top: 379.1, width: 38.0, height: 56.0 },
        'disc-disp-a2': { left: 99.7, top: 380.3, width: 38.0, height: 56.0 },
        'disc-disp-a1': { left: 138.3, top: 380.3, width: 38.0, height: 56.0 },
        'disc-disp-a0': { left: 177.7, top: 380.4, width: 38.0, height: 56.0 },
        'disc-disp-d1': { left: 219.5, top: 380.0, width: 38.0, height: 56.0 },
        'disc-disp-d0': { left: 259.1, top: 379.1, width: 38.0, height: 56.0 },

        // --- Audio / Speaker ---
        'disc-piezo-disk': { left: 339.2, top: 376.0, width: 50.0, height: 50.0 },

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

                // Also update child DIP body or socket frame if IC
                const dip = el.querySelector('.dip-ic, .empty-ladder-socket, .socket-u3-frame, .socket-u7-frame, .dip-socket-frame, .silk-abc80-eprom-text');
                if (dip && c.width !== undefined && c.height !== undefined) {
                    dip.style.width = c.width + 'px';
                    dip.style.height = c.height + 'px';
                }
            });
        }

        applyCoordinates();

        // Update HUD display for the selected component
        function updateHUD() {
            if (!compSelect) return;
            compSelect.value = selectedCompId;
            const c = currentCoords[selectedCompId] || { left: 0, top: 0, width: 0, height: 0 };
            if (inputX) inputX.value = Number(c.left.toFixed(1));
            if (inputY) inputY.value = Number(c.top.toFixed(1));
            if (inputW) inputW.value = Number(c.width.toFixed(1));
            if (inputH) inputH.value = Number(c.height.toFixed(1));

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

        // Keyboard arrow nudge
        window.addEventListener('keydown', (e) => {
            if (!isCalibrating) return;
            if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key)) {
                if (document.activeElement && document.activeElement.tagName === 'INPUT') return;
                e.preventDefault();

                let step = currentStep;
                if (e.shiftKey) step = 5.0;
                else if (e.altKey) step = 0.2;

                if (e.key === 'ArrowUp') nudge('y', -step);
                if (e.key === 'ArrowDown') nudge('y', step);
                if (e.key === 'ArrowLeft') nudge('x', -step);
                if (e.key === 'ArrowRight') nudge('x', step);
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
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important; }\n#${id} .dip-ic, #${id} .empty-ladder-socket, #${id} .socket-u3-frame, #${id} .socket-u7-frame { width: ${c.width}px !important; height: ${c.height}px !important; }\n\n`
                    },
                    {
                        title: 'Silkscreen Overlay',
                        ids: ['silk-abc80-eprom'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important; }\n#${id} .silk-abc80-eprom-text { width: ${c.width}px !important; height: ${c.height}px !important; }\n\n`
                    },
                    {
                        title: 'TO-92 Transistors (+50% Enlarged)',
                        ids: ['disc-q1', 'disc-q2', 'disc-q3', 'disc-q4', 'disc-q5', 'disc-q6', 'disc-q7', 'disc-q8', 'disc-q9', 'disc-q10', 'disc-q11', 'disc-q12', 'disc-q13', 'disc-q14', 'disc-q15', 'disc-q16', 'disc-q17'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Bus Pull-Up Resistors (18x 10k)',
                        ids: ['disc-r-pu1', 'disc-r-pu2', 'disc-r-pu3', 'disc-r-pu4', 'disc-r-pu5', 'disc-r-pu6', 'disc-r-pu7', 'disc-r-pu8', 'disc-r-pu9', 'disc-r-pu10', 'disc-r-pu11', 'disc-r-pu12', 'disc-r-pu13', 'disc-r-pu14', 'disc-r-pu15', 'disc-r-pu16', 'disc-r-pu17', 'disc-r-pu18'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Keypad Column Pull-Up Resistors (8x 3.3k)',
                        ids: ['disc-r-q9', 'disc-r-q10', 'disc-r-q11', 'disc-r-q12', 'disc-r-q13', 'disc-r-q14', 'disc-r-q15', 'disc-r-q16'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Programmer & Clock Bias Resistors',
                        ids: ['disc-res-28v', 'disc-r-prog1', 'disc-r-prog2', 'disc-r-prog3', 'disc-r-prog4', 'disc-r-clock'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Display Segment Resistors (8x 120R)',
                        ids: ['disc-r-seg1', 'disc-r-seg2', 'disc-r-seg3', 'disc-r-seg4', 'disc-r-seg5', 'disc-r-seg6', 'disc-r-seg7', 'disc-r-seg8'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Capacitors',
                        ids: ['disc-cap-c2', 'disc-cap-c1', 'disc-tantalum-cp', 'disc-cap-vcc1', 'disc-cap-vcc2', 'disc-cap-vcc3'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Power, Jacks & Hardware',
                        ids: ['disc-lm7805', 'disc-jack-dc', 'disc-jack-ear', 'disc-jack-mic', 'disc-hw-rst', 'disc-int-header', 'disc-conto-socket', 'disc-clock-section'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: '7-Segment LED Displays (6x)',
                        ids: ['disc-disp-a3', 'disc-disp-a2', 'disc-disp-a1', 'disc-disp-a0', 'disc-disp-d1', 'disc-disp-d0'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important; }\n`
                    },
                    {
                        title: 'Audio / Speaker',
                        ids: ['disc-piezo-disk'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; }\n`
                    },
                    {
                        title: 'Dual 3x4 Keypad Modules',
                        ids: ['disc-keypad-left', 'disc-keypad-right'],
                        format: (id, c) => `#${id} { left: ${c.left}px; top: ${c.top}px; width: ${c.width}px !important; height: ${c.height}px !important; }\n`
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
