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
        paused: false,
        aiLocked: false,
        disasm: {
            lines: [],
            minAddr: null,
            maxAddr: null,
            pendingAddr: null,
            lastPc: 0,
            lastRelocateTime: 0
        },
        mem: {
            baseAddr: 0x0000,
            count: 128,
            bytes: [],
            pendingAddr: null,
            lastFetchTime: 0
        },
        stack: {
            anchorSp: 0x17BF,
            lastSp: 0x17BF,
            stackBytes: [],
            baseAddr: 0x17BF,
            pendingAddr: null
        },
        registers: {
            pc: 0, sp: 0, af: 0, bc: 0, de: 0, hl: 0, ix: null, iy: null,
            flags: { s: 0, z: 0, h: 0, pv: 0, n: 0, c: 0 }
        },
        cycles: 0,
        tapeState: 'STOPPED',
        tape: {
            state: 'stopped',
            fileId: '0001',
            annotation: 'BCD Up-Counter (1000H)',
            counter: 0,
            progress: 0.0,
            toneHz: 0,
            library: [],
            listRequested: false
        },
        qb: {
            visible: false,
            focused: false,
            powerOn: true,
            connected: false,
            mode: 'disconnected',
            currentAddress: 0x1000,
            highlightedKey: 0,
            promptBuffer: '',
            statusMessage: '',
            rows: [],
            attrs: [],
            programs: [],
            statusRequested: false
        }
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

    function defaultInstance() {
        try {
            const params = new URLSearchParams(window.location.search);
            const urlInst = params.get('instance');
            if (urlInst === 'resident' || urlInst === 'unique') {
                return urlInst;
            }
            return (window.self !== window.top) ? 'resident' : 'unique';
        } catch (e) {
            return 'resident';
        }
    }

    function selectedInstance() {
        const store = storage();
        const saved = store ? store.getItem('abc80_instance') : null;
        if (saved === 'resident' || saved === 'unique') return saved;
        const select = document.getElementById('instance-select');
        if (select && select.dataset.initialized === 'true' && select.value) return select.value;
        return defaultInstance();
    }

    // A random token kept in sessionStorage for unique instance, or "resident" for resident instance.
    function sessionToken() {
        if (selectedInstance() === 'resident') {
            return 'resident';
        }
        const store = storage();
        let token = store ? store.getItem('abc80_token') : null;
        if (!token || token === 'resident') {
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

    function updateHostStatus() {
        const inst = selectedInstance().toUpperCase();
        if (state.wsConnected) {
            setHostStatus(true, 'HOST: ONLINE (' + inst + ')');
        } else {
            setHostStatus(false, 'HOST: OFFLINE');
        }
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
            updateHostStatus();
            scheduleReconnect();
            return;
        }
        ws.onopen = function () {
            state.wsConnected = true;
            state.tape.listRequested = false;
            state.qb.statusRequested = false;
            reconnectDelayMs = 1000;
            updateHostStatus();
            updateExecutionControls();
            ws.send(JSON.stringify({ type: 'hello', token: sessionToken(), rom: selectedRom() }));
            fetchMemory(state.mem.baseAddr);
        };
        ws.onmessage = function (event) {
            let msg;
            try { msg = JSON.parse(event.data); } catch (e) { return; }
            if (msg.type === 'telemetry') applyTelemetry(msg);
            else if (msg.type === 'disasm') handleDisasmResponse(msg);
            else if (msg.type === 'memData') handleMemDataResponse(msg);
            else if (msg.type === 'tape_list') handleTapeListResponse(msg);
            else if (msg.type === 'qb_state') handleQbStateResponse(msg);
            else if (msg.type === 'aiPunch') handleAiPunch(msg);
            else if (msg.type === 'error') console.warn('[ABC-80 host] ' + msg.reason);
        };
        ws.onclose = function () {
            state.wsConnected = false;
            setOperatorLockout(false);
            updateHostStatus();
            updateExecutionControls();
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

    function sendRun() {
        if (state.aiLocked) return;
        sendHostCommand({ type: 'run' });
    }

    function sendPause() {
        if (state.aiLocked) return;
        sendHostCommand({ type: 'pause' });
    }

    function sendStep() {
        if (state.aiLocked) return;
        sendHostCommand({ type: 'step' });
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
        if (msg.paused !== undefined) {
            state.paused = !!msg.paused;
        }
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
        if (msg.tape) {
            const prevTapeState = state.tape.state;
            state.tape.state = msg.tape.state || 'stopped';
            state.tape.fileId = msg.tape.fileId || '0001';
            state.tape.annotation = msg.tape.annotation !== undefined ? msg.tape.annotation : state.tape.annotation;
            state.tape.counter = msg.tape.counter || 0;
            state.tape.progress = msg.tape.progress || 0;
            state.tape.toneHz = msg.tape.toneHz || 0;
            state.tapeState = state.tape.state.toUpperCase();
            updateCassetteDeckUI();
            if (prevTapeState === 'recording' && state.tape.state === 'stopped') {
                sendHostCommand({ type: 'tape_list' });
            }
        }
        if (!state.tape.listRequested && state.mem.pendingAddr === null) {
            state.tape.listRequested = true;
            sendHostCommand({ type: 'tape_list' });
        }
        updateInspector();
        updateExecutionControls();
        updateDisassembly(state.registers.pc, state.paused);
        updateStackView(state.registers.sp, msg.stackBytes, msg.stackBaseAddr, state.paused);

        if (state.mem.bytes.length === 0 && state.mem.pendingAddr === null) {
            fetchMemory(state.mem.baseAddr);
        }

        const memTab = document.getElementById('tab-memory');
        if (memTab && memTab.classList.contains('active') && state.paused) {
            const now = Date.now();
            if ((now - (state.mem.lastFetchTime || 0) > 500) && state.mem.pendingAddr === null) {
                state.mem.lastFetchTime = now;
                fetchMemory(state.mem.baseAddr);
            }
        }

        const select = document.getElementById('rom-select');
        if (select && msg.rom && select.value !== msg.rom) {
            select.value = msg.rom;
            const store = storage();
            if (store) store.setItem('abc80_rom', msg.rom);
        }
        if (msg.rom === '1993' && !state.qb.statusRequested) {
            state.qb.statusRequested = true;
            setQbCrtVisible(true, false);
            sendHostCommand({ type: 'qb_status' });
        } else if (msg.rom === 'monitor' && state.qb.statusRequested) {
            state.qb.statusRequested = false;
            state.qb.connected = false;
            state.qb.mode = 'disconnected';
            setQbCrtVisible(false, false);
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
        if (state.aiLocked) return;
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
        if (state.aiLocked) return;
        if (key === 'RST') return;
        sendHostCommand({ type: 'key', key: KEY_NAME_FOR_BUTTON[key] || key, action: 'up' });
    }

    // The reset switch is not part of the key matrix: it resets the CPU and the PPI.
    function handleHardwareReset() {
        if (state.aiLocked) return;
        initAudio();
        sendHostCommand({ type: 'reset' });
        if (selectedRom() === '1993' || state.qb.visible) {
            setTimeout(() => sendHostCommand({ type: 'qb_status' }), 40);
        }
        const btn = document.getElementById('btn-hw-rst');
        if (btn) {
            btn.classList.add('key-pressed');
            setTimeout(() => btn.classList.remove('key-pressed'), 150);
        }
    }

    // ---- AI Keypad Punching, Operator Lockout & Hand Animation ----

    let aiHandFadeTimer = null;

    function setOperatorLockout(locked) {
        state.aiLocked = !!locked;
        const grids = document.querySelectorAll('.keypad-grid-3x4, .keypad-bezel-module, #disc-hw-rst');
        grids.forEach(el => el.classList.toggle('ai-locked', !!locked));
        const rstBtn = document.getElementById('btn-hw-rst');
        if (rstBtn) {
            rstBtn.disabled = !!locked;
        }
        const romSelect = document.getElementById('rom-select');
        if (romSelect) {
            romSelect.disabled = !!locked;
        }
        const cancelBtn = document.getElementById('cancel-ai-btn');
        if (cancelBtn) {
            cancelBtn.disabled = !locked;
        }
        updateExecutionControls();
    }

    function findKeyButton(name) {
        if (!name) return null;
        let norm = String(name).toUpperCase().trim();
        if (norm === 'RST') {
            return document.getElementById('btn-hw-rst') || document.querySelector('.tactile-switch[data-key="RST"]');
        }
        if (norm === 'GO' || norm === 'RUN') {
            return document.querySelector('.key-btn[data-key="GO EXEC"], .key-btn[data-key="GO"]');
        }
        if (norm === 'ADDR') norm = 'ADRS';
        if (norm === 'STEP') norm = 'TO TAPE';
        if (norm === 'BP') norm = 'FROM TAPE';
        return document.querySelector('.key-btn[data-key="' + norm + '"]');
    }

    function handleAiPunch(msg) {
        const cursor = document.getElementById('ai-hand-cursor');
        if (!cursor) return;

        if (aiHandFadeTimer) {
            clearTimeout(aiHandFadeTimer);
            aiHandFadeTimer = null;
        }

        const isPunchActive = (msg.remaining !== undefined && msg.remaining > 0) || !!msg.down;
        setOperatorLockout(isPunchActive);

        const btn = findKeyButton(msg.key);
        if (btn) {
            const rect = btn.getBoundingClientRect();
            // Position cursor pointer fingertip on button center
            const x = rect.left + rect.width / 2 - 28;
            const y = rect.top + rect.height / 2 - 8;
            cursor.style.left = Math.round(x) + 'px';
            cursor.style.top = Math.round(y) + 'px';
            cursor.classList.remove('ai-hand-hidden');

            if (msg.down) {
                cursor.classList.add('ai-punch-down');
                btn.classList.add('ai-punched');
                initAudio();
            } else {
                cursor.classList.remove('ai-punch-down');
                btn.classList.remove('ai-punched');
            }
        }

        if (msg.remaining === 0 && !msg.down) {
            setOperatorLockout(false);
            aiHandFadeTimer = setTimeout(function () {
                cursor.classList.add('ai-hand-hidden');
                document.querySelectorAll('.key-btn.ai-punched, #btn-hw-rst.ai-punched').forEach(function (b) {
                    b.classList.remove('ai-punched');
                });
            }, 800);
        }
    }

    function initInstanceSelect() {
        const select = document.getElementById('instance-select');
        if (!select) return;
        const store = storage();
        const saved = store ? store.getItem('abc80_instance') : null;
        const initial = (saved === 'resident' || saved === 'unique') ? saved : defaultInstance();
        select.value = initial;
        select.dataset.initialized = 'true';
        select.addEventListener('change', function () {
            const s = storage();
            if (s) s.setItem('abc80_instance', select.value);
            updateHostStatus();
            if (ws) {
                ws.close();
            }
        });
    }

    function initAiControls() {
        const cancelBtn = document.getElementById('cancel-ai-btn');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', function () {
                sendHostCommand({ type: 'cancelAiPunch' });
                setOperatorLockout(false);
                const cursor = document.getElementById('ai-hand-cursor');
                if (cursor) cursor.classList.add('ai-hand-hidden');
                document.querySelectorAll('.key-btn.ai-punched, #btn-hw-rst.ai-punched').forEach(function (b) {
                    b.classList.remove('ai-punched');
                });
            });
        }
    }

    function changeRom(name) {
        if (state.aiLocked) return;
        const store = storage();
        if (store) store.setItem('abc80_rom', name);
        sendHostCommand({ type: 'setRom', rom: name });
        if (name === '1993') {
            state.qb.statusRequested = true;
            setQbCrtVisible(true, false);
            setTimeout(() => sendHostCommand({ type: 'qb_status' }), 40);
        } else {
            state.qb.statusRequested = false;
            state.qb.connected = false;
            state.qb.mode = 'disconnected';
            setQbCrtVisible(false, false);
        }
    }

    // For tests and the console.
    window.__abc80 = {
        initAudio: initAudio,
        get audioCtx() { return audioCtx; },
        get lastTelemetry() { return state.lastTelemetry; },
        get isPaused() { return state.paused; },
        get disasmState() { return state.disasm; },
        get ws() { return ws; },
        sendHostCommand: sendHostCommand,
        applyTelemetry: applyTelemetry,   // feed a telemetry frame to the renderer (test hook)
        updateDisassembly: updateDisassembly,
        get memState() { return state.mem; },
        get stackState() { return state.stack; },
        get qbState() { return state.qb; },
        setQbFocus: setQbFocus,
        setQbCrtVisible: setQbCrtVisible,
        fetchMemory: fetchMemory,
        fetchStack: fetchStack,
        updateStackView: updateStackView,
        sendRun: sendRun,
        sendPause: sendPause,
        sendStep: sendStep,
        disconnect: function () { reconnectDelayMs = 3600000; if (ws) { ws.onmessage = null; ws.close(); } },  // stop live frames (test hook)
        get token() { return sessionToken(); },
        get selectedInstance() { return selectedInstance(); },
        setInstance: function (inst) {
            const select = document.getElementById('instance-select');
            if (select) select.value = inst;
            const store = storage();
            if (store) store.setItem('abc80_instance', inst);
            updateHostStatus();
            if (ws) ws.close();
        },
        get isAiLocked() {
            return !!state.aiLocked;
        },
        get isAiHandVisible() {
            const el = document.getElementById('ai-hand-cursor');
            return el ? !el.classList.contains('ai-hand-hidden') : false;
        },
        handleAiPunch: handleAiPunch
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

        const isPlaying = state.tape.state === 'playing';
        const isRecording = state.tape.state === 'recording';
        const toneHz = state.tape.toneHz || (isPlaying ? 1000 : 0);
        scopeCtx.strokeStyle = isPlaying ? '#10b981' : (isRecording ? '#ef4444' : '#1e293b');
        scopeCtx.lineWidth = 1.6;
        scopeCtx.beginPath();

        if (!isPlaying && !isRecording) {
            for (let x = 0; x < w; x++) {
                const y = h / 2 + Math.sin(x * 0.03 + scopePhase) * 1.5;
                if (x === 0) scopeCtx.moveTo(x, y);
                else scopeCtx.lineTo(x, y);
            }
            scopePhase += 0.02;
        } else {
            const halfWavePx = (toneHz >= 1800) ? 11 : 22;
            const amp = (h / 2 - 10);
            for (let x = 0; x < w; x++) {
                const sq = Math.sin((x / halfWavePx) * Math.PI + scopePhase) >= 0 ? -amp : amp;
                const y = h / 2 + sq;
                if (x === 0) scopeCtx.moveTo(x, y);
                else scopeCtx.lineTo(x, y);
            }
            scopePhase += (toneHz >= 1800) ? 0.42 : 0.21;
        }
        scopeCtx.stroke();
        requestAnimationFrame(drawScope);
    }

    function normalizeHexFileId(raw) {
        if (typeof raw !== 'string') return null;
        let s = raw.trim().toUpperCase();
        if (s.startsWith('0X')) s = s.substring(2);
        if (s.endsWith('H')) s = s.substring(0, s.length - 1);
        if (!/^[0-9A-F]{1,4}$/.test(s)) return null;
        return s.padStart(4, '0');
    }

    function updateCassetteDeckUI() {
        const deck = document.getElementById('cassette-deck');
        const statusBadge = document.getElementById('tape-status-badge');
        const toneLed = document.getElementById('tape-tone-led');
        const toneLabel = document.getElementById('tape-tone-label');
        const counterEl = document.getElementById('tape-counter');
        const labelIdEl = document.getElementById('cassette-label-id');
        const annInput = document.getElementById('tape-annotation-input');
        const idInput = document.getElementById('tape-file-id-input');
        const progFill = document.getElementById('tape-progress-fill');
        const packLeft = document.getElementById('tape-pack-left');
        const packRight = document.getElementById('tape-pack-right');
        const btnPlay = document.getElementById('btn-tape-play');
        const btnRec = document.getElementById('btn-tape-rec');

        const st = state.tape.state || 'stopped';
        if (deck) {
            deck.classList.toggle('deck-playing', st === 'playing');
            deck.classList.toggle('deck-recording', st === 'recording');
        }
        if (statusBadge) {
            statusBadge.textContent = st.toUpperCase();
            statusBadge.className = 'tape-status-badge ' + st;
        }
        if (btnPlay) btnPlay.classList.toggle('active', st === 'playing');
        if (btnRec) btnRec.classList.toggle('active', st === 'recording');

        const hz = state.tape.toneHz || 0;
        if (toneLed) {
            toneLed.classList.toggle('tone-1khz', hz > 0 && hz < 1500);
            toneLed.classList.toggle('tone-2khz', hz >= 1500);
        }
        if (toneLabel) {
            toneLabel.textContent = hz > 0 ? (hz + ' Hz') : (st === 'stopped' ? 'IDLE' : 'WAIT');
        }
        if (counterEl) {
            counterEl.textContent = String((state.tape.counter || 0) % 1000).padStart(3, '0');
        }
        if (labelIdEl) {
            labelIdEl.textContent = 'FILE ' + (state.tape.fileId || '0001');
        }
        if (idInput && document.activeElement !== idInput) {
            idInput.value = state.tape.fileId || '0001';
        }
        if (annInput && document.activeElement !== annInput && state.tape.annotation !== undefined) {
            annInput.value = state.tape.annotation;
        }
        const p = Math.max(0, Math.min(1, Number(state.tape.progress) || 0));
        if (progFill) {
            progFill.style.width = Math.round(p * 100) + '%';
        }
        if (packLeft) {
            packLeft.style.setProperty('--pack-scale', (0.92 - 0.44 * p).toFixed(3));
        }
        if (packRight) {
            packRight.style.setProperty('--pack-scale', (0.48 + 0.44 * p).toFixed(3));
        }
        document.querySelectorAll('#tape-library-list .tape-lib-row').forEach(row => {
            row.classList.toggle('selected', row.getAttribute('data-file-id') === state.tape.fileId);
        });
    }

    function handleTapeListResponse(msg) {
        if (!msg || !Array.isArray(msg.tapes)) return;
        state.tape.library = msg.tapes;
        renderTapeLibrary();
    }

    function renderTapeLibrary() {
        const listEl = document.getElementById('tape-library-list');
        if (!listEl) return;
        if (!state.tape.library || state.tape.library.length === 0) {
            listEl.innerHTML = '<div class="disasm-placeholder">No saved .tape files in ~/.config/abc-80/tapes/</div>';
            return;
        }
        listEl.innerHTML = state.tape.library.map(item => {
            const fid = item.fileId || '0000';
            const ann = item.annotation || '(Untitled Tape)';
            const isSel = (fid === state.tape.fileId);
            let meta = 'EMPTY';
            if (item.valid && item.bytes > 0) {
                const sHex = (item.startAddr || 0).toString(16).toUpperCase().padStart(4, '0');
                const eHex = (item.endAddr || 0).toString(16).toUpperCase().padStart(4, '0');
                meta = `${sHex}H–${eHex}H (${item.bytes}B)`;
            }
            return `<div class="tape-lib-row${isSel ? ' selected' : ''}" data-file-id="${fid}">` +
                   `<span class="tape-lib-id">${fid}</span>` +
                   `<span class="tape-lib-ann">${ann}</span>` +
                   `<span class="tape-lib-meta">${meta}</span>` +
                   `</div>`;
        }).join('');

        listEl.querySelectorAll('.tape-lib-row').forEach(row => {
            row.addEventListener('click', () => {
                const fid = row.getAttribute('data-file-id');
                if (!fid) return;
                const idInput = document.getElementById('tape-file-id-input');
                if (idInput) idInput.value = fid;
                sendHostCommand({ type: 'tape_select', fileId: fid });
            });
        });
    }

    function initCassetteDeck() {
        const btnPlay = document.getElementById('btn-tape-play');
        const btnRec = document.getElementById('btn-tape-rec');
        const btnRew = document.getElementById('btn-tape-rew');
        const btnStop = document.getElementById('btn-tape-stop');
        const btnSelect = document.getElementById('btn-tape-select');
        const idInput = document.getElementById('tape-file-id-input');
        const btnSaveLabel = document.getElementById('btn-tape-save-label');
        const annInput = document.getElementById('tape-annotation-input');

        function currentSlotHex() {
            const fromInput = idInput ? normalizeHexFileId(idInput.value) : null;
            return fromInput || state.tape.fileId || '0001';
        }

        function doSelectSlot() {
            const hex4 = currentSlotHex();
            if (idInput) idInput.value = hex4;
            state.tape.fileId = hex4;
            sendHostCommand({ type: 'tape_select', fileId: hex4 });
        }

        function doSaveLabel() {
            const hex4 = currentSlotHex();
            const ann = annInput ? annInput.value.slice(0, 32) : '';
            state.tape.fileId = hex4;
            state.tape.annotation = ann;
            sendHostCommand({ type: 'tape_annotate', fileId: hex4, annotation: ann });
            sendHostCommand({ type: 'tape_select', fileId: hex4 });
        }

        if (btnPlay) {
            btnPlay.disabled = false;
            btnPlay.addEventListener('click', () => {
                initAudio();
                const hex4 = currentSlotHex();
                sendHostCommand({ type: 'tape_transport', action: 'play', fileId: hex4 });
            });
        }
        if (btnRec) {
            btnRec.disabled = false;
            btnRec.addEventListener('click', () => {
                initAudio();
                const ann = annInput ? annInput.value.slice(0, 32) : '';
                sendHostCommand({ type: 'tape_transport', action: 'record', annotation: ann });
            });
        }
        if (btnRew) {
            btnRew.disabled = false;
            btnRew.addEventListener('click', () => {
                sendHostCommand({ type: 'tape_transport', action: 'rewind' });
            });
        }
        if (btnStop) {
            btnStop.disabled = false;
            btnStop.addEventListener('click', () => {
                sendHostCommand({ type: 'tape_transport', action: 'stop' });
                setTimeout(() => sendHostCommand({ type: 'tape_list' }), 40);
            });
        }
        if (btnSelect) btnSelect.addEventListener('click', doSelectSlot);
        if (idInput) {
            idInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    doSelectSlot();
                }
            });
        }
        if (btnSaveLabel) btnSaveLabel.addEventListener('click', doSaveLabel);
        if (annInput) {
            annInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    doSaveLabel();
                }
            });
        }
        updateCassetteDeckUI();
    }

    // ==========================================================================
    // Early-'90s 14" Color VGA CRT Monitor (QuickBASIC 4.0 PC/AT Host)
    // ==========================================================================

    function escapeHtml(ch) {
        if (ch === '&') return '&amp;';
        if (ch === '<') return '&lt;';
        if (ch === '>') return '&gt;';
        return ch;
    }

    // Translate internal ASCII CP437 placeholders back to authentic IBM VGA CP437 Unicode glyphs
    function translateCp437Glyph(ch, row1, col1, fg) {
        if (row1 >= 1 && row1 <= 5) {
            if (ch === '#') return '\u2588'; // █ (219)
            if (ch === '.') return '\u2584'; // ▄ (220)
            if (ch === '^') return '\u2580'; // ▀ (223)
            if (ch === '[') return '\u2590'; // ▐ (222)
            if (ch === ']') return '\u258C'; // ▌ (221)
        }
        if (row1 === 8 && fg === 9 && ch === '#') {
            return '\u2588'; // ███ mode cursor
        }
        // Highlighted key blocks in ColourKeyPressed (fg 10 green or fg 12 red on key rows)
        if ((row1 === 12 || row1 === 14 || row1 === 17 || row1 === 22) && (fg === 10 || fg === 12) && ch === '#') {
            return '\u2588';
        }
        // Memory table frame (Rows 7..17, Cols 13..53): single vertical/double horizontal CP437 box
        if (row1 >= 7 && row1 <= 17 && col1 >= 13 && col1 <= 53) {
            if (row1 === 7) {
                if (col1 === 13) return '\u2552'; // ╒
                if (col1 === 33) return '\u2564'; // ╤
                if (col1 === 53) return '\u2555'; // ╕
                if (ch === '-') return '\u2550';  // ═
            } else if (row1 === 9) {
                if (col1 === 13) return '\u255E'; // ╞
                if (col1 === 33) return '\u256A'; // ╪
                if (col1 === 53) return '\u2561'; // ╡
                if (ch === '-') return '\u2550';  // ═
            } else if (row1 === 13) {
                if (col1 === 13) return '\u251C'; // ├
                if (col1 === 33) return '\u253C'; // ┼
                if (col1 === 53) return '\u2524'; // ┤
                if (ch === '-') return '\u2500';  // ─
                if (ch === '*') return '\u25C4';  // ◄/► indicator
            } else if (row1 === 17) {
                if (col1 === 13) return '\u2558'; // ╘
                if (col1 === 33) return '\u2567'; // ╧
                if (col1 === 53) return '\u255B'; // ╛
                if (ch === '-') return '\u2550';  // ═
            } else if (ch === '|') {
                return '\u2502'; // │
            }
        }
        // Right-side function key boxes (Rows 11..18, Cols 57..72): double-line CP437 box
        if (row1 >= 11 && row1 <= 18 && col1 >= 57 && col1 <= 72) {
            if (ch === '|') return '\u2551'; // ║
            if (ch === '-' && !(row1 === 12 && col1 === 69)) return '\u2550'; // ═ (preserve '-' label at row 12 col 69)
            if (row1 === 12 && col1 === 69 && ch === '-') return '\u2500'; // ─
            if (ch === '+') {
                if (row1 === 11) return col1 === 57 ? '\u2554' : (col1 === 72 ? '\u2557' : '\u2566');
                if (row1 === 13) return col1 === 57 ? '\u2560' : (col1 === 72 ? '\u2563' : '\u256C');
                if (row1 === 15) return col1 === 57 ? '\u255A' : (col1 === 72 ? '\u255D' : '\u2569');
                if (row1 === 16) return col1 === 57 ? '\u2554' : (col1 === 72 ? '\u2557' : '\u2566');
                if (row1 === 18) return col1 === 57 ? '\u255A' : (col1 === 72 ? '\u255D' : '\u2569');
            }
        }
        // Bottom hex key bar (Rows 21..23, Cols 8..72): double-line CP437 box
        if (row1 >= 21 && row1 <= 23 && col1 >= 8 && col1 <= 72) {
            if (ch === '|') return '\u2551'; // ║
            if (ch === '-') return '\u2550'; // ═
            if (ch === '+') {
                if (row1 === 21) return col1 === 8 ? '\u2554' : (col1 === 72 ? '\u2557' : '\u2566');
                if (row1 === 23) return col1 === 8 ? '\u255A' : (col1 === 72 ? '\u255D' : '\u2569');
            }
        }
        return ch;
    }

    // Return virtual QuickBASIC key command if (row1, col1) lies inside an on-screen CP437 key cell
    function qbKeyActionAtCell(row1, col1) {
        if (row1 === 22 && col1 >= 9 && col1 <= 71) {
            const rel = col1 - 9;
            if (rel % 4 !== 3) {
                const idx = Math.floor(rel / 4);
                if (idx >= 0 && idx < 16) {
                    return '0123456789ABCDEF'[idx];
                }
            }
        }
        if (row1 === 12) {
            if (col1 >= 58 && col1 <= 61) return 'INSERT';
            if (col1 >= 63 && col1 <= 66) return 'HOME';
            if (col1 >= 68 && col1 <= 71) return 'PAGEUP';
        }
        if (row1 === 14) {
            if (col1 >= 58 && col1 <= 61) return 'DELETE';
            if (col1 >= 63 && col1 <= 66) return 'END';
            if (col1 >= 68 && col1 <= 71) return 'PAGEDOWN';
        }
        if (row1 === 17) {
            if (col1 >= 66 && col1 <= 71) return 'SHIFT+F5';
        }
        return null;
    }

    function renderQbCrtScreen() {
        const screenEl = document.getElementById('qb-crt-screen');
        if (!screenEl) return;
        const rows = state.qb.rows || [];
        const attrs = state.qb.attrs || [];
        if (rows.length === 0) return;

        let html = '';
        for (let r = 0; r < 25; ++r) {
            const rowStr = (rows[r] || '').padEnd(80, ' ');
            const attrHex = (attrs[r] || '').padEnd(160, '07');
            let rowHtml = '<div class="qb-crt-row" data-row="' + (r + 1) + '">';
            let spanClass = '';
            let spanAction = '';
            let spanText = '';

            for (let c = 0; c < 80; ++c) {
                const byteHex = attrHex.substr(c * 2, 2);
                const attrVal = parseInt(byteHex, 16);
                const fg = isNaN(attrVal) ? 7 : (attrVal & 0x0F);
                const bg = isNaN(attrVal) ? 0 : ((attrVal >> 4) & 0x07);
                const action = qbKeyActionAtCell(r + 1, c + 1) || '';
                const cls = 'vga-fg-' + fg + ' vga-bg-' + bg + (action ? ' qb-cell-clickable' : '');
                const glyph = translateCp437Glyph(rowStr[c], r + 1, c + 1, fg);

                if (cls !== spanClass || action !== spanAction) {
                    if (spanText.length > 0) {
                        const dataAttr = spanAction ? ' data-qb-key="' + spanAction + '"' : '';
                        rowHtml += '<span class="' + spanClass + '"' + dataAttr + '>' + spanText + '</span>';
                        spanText = '';
                    }
                    spanClass = cls;
                    spanAction = action;
                }
                spanText += escapeHtml(glyph);
            }
            if (spanText.length > 0) {
                const dataAttr = spanAction ? ' data-qb-key="' + spanAction + '"' : '';
                rowHtml += '<span class="' + spanClass + '"' + dataAttr + '>' + spanText + '</span>';
            }
            rowHtml += '</div>';
            html += rowHtml;
        }
        screenEl.innerHTML = html;
    }

    function updateQbCrtBadges() {
        const win = document.getElementById('qb-crt-window');
        const toggleBtn = document.getElementById('btn-toggle-qb-crt');
        const linkBadge = document.getElementById('qb-link-badge');
        const focusBadge = document.getElementById('qb-focus-badge');
        const progSelect = document.getElementById('qb-program-select');

        if (win) {
            win.classList.toggle('hidden', !state.qb.visible);
            win.classList.toggle('crt-focused', !!(state.qb.visible && state.qb.focused));
        }
        if (toggleBtn) {
            toggleBtn.classList.toggle('header-btn-active', !!state.qb.visible);
        }
        if (linkBadge) {
            if (state.qb.connected) {
                linkBadge.textContent = 'LINK: 8255 ACTIVE (' + state.qb.mode.toUpperCase() + ')';
                linkBadge.className = 'qb-link-badge connected';
            } else {
                linkBadge.textContent = 'LINK: STANDBY';
                linkBadge.className = 'qb-link-badge standby';
            }
        }
        if (focusBadge) {
            if (state.qb.visible && state.qb.focused) {
                focusBadge.textContent = 'KBD FOCUS: VGA CRT';
                focusBadge.className = 'qb-focus-badge crt-focus';
            } else {
                focusBadge.textContent = 'KBD FOCUS: ABC-80 PCB';
                focusBadge.className = 'qb-focus-badge pcb-focus';
            }
        }
        if (progSelect && Array.isArray(state.qb.programs)) {
            const currentVal = progSelect.value;
            let opts = '<option value="">-- Library (' + state.qb.programs.length + ') --</option>';
            state.qb.programs.forEach(p => {
                opts += '<option value="' + escapeHtml(p) + '">' + escapeHtml(p) + '</option>';
            });
            progSelect.innerHTML = opts;
            if (currentVal && state.qb.programs.includes(currentVal)) {
                progSelect.value = currentVal;
            }
        }
    }

    function setQbFocus(focused) {
        state.qb.focused = !!(focused && state.qb.visible);
        updateQbCrtBadges();
    }

    function setQbCrtVisible(visible, focusOnOpen) {
        state.qb.visible = !!visible;
        if (!visible) {
            state.qb.focused = false;
        } else if (focusOnOpen) {
            state.qb.focused = true;
        }
        updateQbCrtBadges();
    }

    // Play QuickBASIC SOUND and PLAY MML events through WebAudio
    function playQbAudioEvents(events) {
        if (!Array.isArray(events) || events.length === 0) return;
        if (!state.soundEnabled) return;
        initAudio();
        if (!audioCtx || audioCtx.state !== 'running') return;

        let cursorTime = audioCtx.currentTime + 0.01;
        function scheduleTone(freqHz, durationSec) {
            if (durationSec <= 0) return;
            if (freqHz > 20) {
                const osc = audioCtx.createOscillator();
                const gain = audioCtx.createGain();
                osc.type = 'square';
                osc.frequency.setValueAtTime(freqHz, cursorTime);
                gain.gain.setValueAtTime(0.045, cursorTime);
                osc.connect(gain);
                gain.connect(audioCtx.destination);
                osc.start(cursorTime);
                osc.stop(cursorTime + durationSec);
            }
            cursorTime += durationSec;
        }

        function scheduleMml(mml) {
            if (typeof mml !== 'string') return;
            let octave = 4;
            let defaultLen = 4;
            let tempo = 120;
            let i = 0;
            while (i < mml.length) {
                const ch = mml[i++].toUpperCase();
                if (ch === ' ' || ch === '\t') continue;
                if (ch === 'O' && i < mml.length && /[0-7]/.test(mml[i])) {
                    octave = parseInt(mml[i++], 10);
                } else if (ch === '>') {
                    if (octave < 7) octave++;
                } else if (ch === '<') {
                    if (octave > 1) octave--;
                } else if (ch === 'L') {
                    let num = '';
                    while (i < mml.length && /[0-9]/.test(mml[i])) num += mml[i++];
                    if (num) defaultLen = Math.max(1, parseInt(num, 10));
                } else if (ch === 'T') {
                    let num = '';
                    while (i < mml.length && /[0-9]/.test(mml[i])) num += mml[i++];
                    if (num) tempo = Math.max(32, parseInt(num, 10));
                } else if (ch === 'M' && i < mml.length) {
                    i++;
                } else if (ch >= 'A' && ch <= 'G') {
                    const semiMap = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
                    let semi = semiMap[ch];
                    if (i < mml.length && (mml[i] === '#' || mml[i] === '+')) { semi++; i++; }
                    else if (i < mml.length && mml[i] === '-') { semi--; i++; }
                    let len = defaultLen;
                    let num = '';
                    while (i < mml.length && /[0-9]/.test(mml[i])) num += mml[i++];
                    if (num) len = Math.max(1, parseInt(num, 10));
                    const midi = (octave + 1) * 12 + semi;
                    const freq = 440.0 * Math.pow(2.0, (midi - 69) / 12.0);
                    const dur = (240.0 / tempo) / len;
                    scheduleTone(freq, dur);
                }
            }
        }

        events.forEach(ev => {
            if (!ev) return;
            if (ev.kind === 'sound') {
                scheduleTone(Number(ev.freqHz) || 1000, Number(ev.durationSec) || 0.05);
            } else if (ev.kind === 'play') {
                scheduleMml(ev.mml || '');
            }
        });
    }

    function handleQbStateResponse(msg) {
        if (!msg) return;
        state.qb.connected = !!msg.connected;
        state.qb.mode = msg.mode || (msg.connected ? 'neutral' : 'disconnected');
        if (typeof msg.currentAddress === 'number') state.qb.currentAddress = msg.currentAddress;
        if (typeof msg.highlightedKey === 'number') state.qb.highlightedKey = msg.highlightedKey;
        if (typeof msg.promptBuffer === 'string') state.qb.promptBuffer = msg.promptBuffer;
        if (typeof msg.statusMessage === 'string') state.qb.statusMessage = msg.statusMessage;
        if (Array.isArray(msg.rows)) state.qb.rows = msg.rows;
        if (Array.isArray(msg.attrs)) state.qb.attrs = msg.attrs;
        if (Array.isArray(msg.programs)) state.qb.programs = msg.programs;

        renderQbCrtScreen();
        updateQbCrtBadges();
        if (Array.isArray(msg.audioEvents) && msg.audioEvents.length > 0) {
            playQbAudioEvents(msg.audioEvents);
        }
    }

    function sendQbKey(keyName) {
        if (!keyName) return;
        initAudio();
        setQbFocus(true);
        sendHostCommand({ type: 'qb_key', key: keyName });
    }

    function connectQbHost() {
        initAudio();
        if (selectedRom() !== '1993') {
            const romSelect = document.getElementById('rom-select');
            if (romSelect) romSelect.value = '1993';
            changeRom('1993');
            setQbCrtVisible(true, true);
            setTimeout(() => sendHostCommand({ type: 'qb_connect' }), 60);
            return;
        }
        setQbCrtVisible(true, true);
        sendHostCommand({ type: 'qb_connect' });
    }

    function initQbCrtWindow() {
        const win = document.getElementById('qb-crt-window');
        const toggleBtn = document.getElementById('btn-toggle-qb-crt');
        const closeBtn = document.getElementById('btn-qb-close');
        const dragHandle = document.getElementById('qb-crt-drag-handle');
        const screenEl = document.getElementById('qb-crt-screen');
        const tubeEl = document.getElementById('qb-crt-tube');
        const boardContainer = document.getElementById('board-container');

        if (toggleBtn) {
            toggleBtn.addEventListener('click', () => {
                const nextVis = !state.qb.visible;
                setQbCrtVisible(nextVis, nextVis);
                if (nextVis) {
                    sendHostCommand({ type: 'qb_status' });
                }
            });
        }
        if (closeBtn) {
            closeBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                setQbCrtVisible(false, false);
            });
        }

        // Click inside CRT window focuses QuickBASIC keyboard routing; click on PCB restores ABC-80 keypad focus
        if (win) {
            win.addEventListener('pointerdown', () => {
                if (state.qb.visible) setQbFocus(true);
            });
        }
        if (boardContainer) {
            boardContainer.addEventListener('pointerdown', () => {
                if (state.qb.focused) setQbFocus(false);
            });
        }

        // Clickable on-screen CP437 virtual buttons inside the 80x25 screen
        if (screenEl) {
            screenEl.addEventListener('click', (e) => {
                const cell = e.target.closest('[data-qb-key]');
                if (cell) {
                    const keyCmd = cell.getAttribute('data-qb-key');
                    if (keyCmd) sendQbKey(keyCmd);
                }
            });
        }

        // Draggable monitor bezel
        if (dragHandle && win) {
            dragHandle.addEventListener('pointerdown', (e) => {
                if (e.target.closest('button')) return;
                e.preventDefault();
                const rect = win.getBoundingClientRect();
                const startX = e.clientX;
                const startY = e.clientY;
                const origLeft = rect.left;
                const origTop = rect.top;

                function onMove(ev) {
                    const nx = Math.max(8, Math.min(window.innerWidth - 120, origLeft + (ev.clientX - startX)));
                    const ny = Math.max(48, Math.min(window.innerHeight - 80, origTop + (ev.clientY - startY)));
                    win.style.right = 'auto';
                    win.style.left = Math.round(nx) + 'px';
                    win.style.top = Math.round(ny) + 'px';
                }
                function onUp() {
                    window.removeEventListener('pointermove', onMove);
                    window.removeEventListener('pointerup', onUp);
                }
                window.addEventListener('pointermove', onMove);
                window.addEventListener('pointerup', onUp);
            });
        }

        // Lower Apron tactile controls
        const btnConnect = document.getElementById('btn-qb-connect');
        const btnAdds = document.getElementById('btn-qb-adds');
        const btnData = document.getElementById('btn-qb-data');
        const btnPrev = document.getElementById('btn-qb-prev');
        const btnNext = document.getElementById('btn-qb-next');
        const btnIns = document.getElementById('btn-qb-ins');
        const btnDel = document.getElementById('btn-qb-del');
        const btnRun = document.getElementById('btn-qb-run');
        const btnMon009b = document.getElementById('btn-qb-mon-009b');
        const fileInput = document.getElementById('qb-filename-input');
        const progSelect = document.getElementById('qb-program-select');
        const btnLoad = document.getElementById('btn-qb-load');
        const btnSave = document.getElementById('btn-qb-save');
        const brightnessWheel = document.getElementById('qb-crt-brightness');
        const btnPower = document.getElementById('btn-qb-power');

        if (btnConnect) btnConnect.addEventListener('click', connectQbHost);
        if (btnAdds) btnAdds.addEventListener('click', () => sendQbKey('HOME'));
        if (btnData) btnData.addEventListener('click', () => sendQbKey('END'));
        if (btnPrev) btnPrev.addEventListener('click', () => sendQbKey('PAGEUP'));
        if (btnNext) btnNext.addEventListener('click', () => sendQbKey('PAGEDOWN'));
        if (btnIns) btnIns.addEventListener('click', () => sendQbKey('INSERT'));
        if (btnDel) btnDel.addEventListener('click', () => sendQbKey('DELETE'));
        if (btnRun) btnRun.addEventListener('click', () => sendQbKey('SHIFT+F5'));

        if (btnMon009b) {
            btnMon009b.addEventListener('click', () => {
                initAudio();
                setQbFocus(true);
                ['HOME', '0', '0', '9', 'B', 'SHIFT+F5'].forEach(k => {
                    sendHostCommand({ type: 'qb_key', key: k });
                });
                // Transfer keyboard focus back to the ABC-80 PCB so the operator can use the 009BH keypad monitor
                setQbFocus(false);
            });
        }

        if (progSelect && fileInput) {
            progSelect.addEventListener('change', () => {
                if (progSelect.value) {
                    fileInput.value = progSelect.value;
                }
            });
        }

        if (btnLoad) {
            btnLoad.addEventListener('click', () => {
                const fname = (fileInput && fileInput.value.trim()) ? fileInput.value.trim() : 'TEST.ABC';
                setQbFocus(true);
                sendHostCommand({ type: 'qb_load', filename: fname });
            });
        }

        if (btnSave) {
            btnSave.addEventListener('click', () => {
                const fname = (fileInput && fileInput.value.trim()) ? fileInput.value.trim() : 'TEST.ABC';
                const startAddr = state.qb.currentAddress & 0xFFFF;
                const finishAddr = Math.min(0xFFFF, startAddr + 0x000F);
                setQbFocus(true);
                sendHostCommand({ type: 'qb_save', filename: fname, startAddr: startAddr, endAddr: finishAddr });
            });
        }

        if (brightnessWheel && tubeEl) {
            brightnessWheel.addEventListener('input', (e) => {
                tubeEl.style.setProperty('--crt-brightness', e.target.value);
            });
        }

        if (btnPower && tubeEl) {
            btnPower.addEventListener('click', () => {
                state.qb.powerOn = !state.qb.powerOn;
                btnPower.classList.toggle('power-on', state.qb.powerOn);
                tubeEl.classList.toggle('power-off', !state.qb.powerOn);
            });
        }
    }

    function initInspector() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
                btn.classList.add('active');
                const targetId = btn.getAttribute('data-tab');
                const pane = document.getElementById(targetId);
                if (pane) pane.classList.add('active');
                if (targetId === 'tab-cpu') {
                    updateDisassembly(state.registers.pc, state.paused);
                } else if (targetId === 'tab-memory') {
                    fetchMemory(state.mem.baseAddr);
                    if (state.registers.sp !== 0) {
                        fetchStack(state.registers.sp);
                    }
                    updateStackView(state.registers.sp, state.stack.stackBytes, state.stack.baseAddr, state.paused);
                } else if (targetId === 'tab-tape') {
                    sendHostCommand({ type: 'tape_list' });
                    updateCassetteDeckUI();
                }
            });
        });

        const btnRun = document.getElementById('btn-cpu-run');
        if (btnRun) btnRun.addEventListener('click', sendRun);
        const btnPause = document.getElementById('btn-cpu-pause');
        if (btnPause) btnPause.addEventListener('click', sendPause);
        const btnStep = document.getElementById('btn-cpu-step');
        if (btnStep) btnStep.addEventListener('click', sendStep);

        // Memory Viewer controls
        document.querySelectorAll('.mem-jump-bar .chip-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const addr = parseHexInput(btn.getAttribute('data-addr'));
                if (!isNaN(addr)) fetchMemory(addr);
            });
        });

        const btnJump = document.getElementById('btn-mem-jump');
        const memOffsetInput = document.getElementById('mem-offset-input');
        function doJump() {
            if (memOffsetInput) {
                const addr = parseHexInput(memOffsetInput.value);
                if (!isNaN(addr)) fetchMemory(addr);
            }
        }
        if (btnJump) btnJump.addEventListener('click', doJump);
        if (memOffsetInput) {
            memOffsetInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    doJump();
                }
            });
        }

        const MEM_PAGE_SIZE = 128;
        const btnPrev = document.getElementById('btn-mem-prev');
        if (btnPrev) btnPrev.addEventListener('click', () => {
            fetchMemory((state.mem.baseAddr - MEM_PAGE_SIZE) & 0xFFFF);
        });
        const btnNext = document.getElementById('btn-mem-next');
        if (btnNext) btnNext.addEventListener('click', () => {
            fetchMemory((state.mem.baseAddr + MEM_PAGE_SIZE) & 0xFFFF);
        });

        // Stack Anchor controls
        const btnAnchor = document.getElementById('btn-stack-anchor');
        const anchorInput = document.getElementById('stack-anchor-input');
        function doAnchor() {
            if (anchorInput) {
                const a = parseHexInput(anchorInput.value);
                if (!isNaN(a)) {
                    state.stack.anchorSp = a;
                } else {
                    state.stack.anchorSp = state.registers.sp;
                    anchorInput.value = state.stack.anchorSp.toString(16).toUpperCase().padStart(4, '0');
                }
                updateStackView(state.registers.sp, state.stack.stackBytes, state.stack.baseAddr, state.paused);
            }
        }
        if (btnAnchor) btnAnchor.addEventListener('click', doAnchor);
        if (anchorInput) {
            anchorInput.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    doAnchor();
                }
            });
        }

        window.addEventListener('resize', () => {
            const activeTab = document.querySelector('.tab-btn.active');
            if (!activeTab) return;
            const targetId = activeTab.getAttribute('data-tab');
            if (targetId === 'tab-cpu') {
                if (state.paused) {
                    sendHostCommand({ type: 'readDisasm', addr: state.disasm.lastPc || state.registers.pc, count: getDisasmLineCount() });
                } else {
                    renderDisassembly();
                }
            } else if (targetId === 'tab-memory') {
                updateStackView(state.registers.sp, state.stack.stackBytes, state.stack.baseAddr, state.paused);
            }
        });

        const disasmContainer = document.getElementById('disasm-container');
        if (disasmContainer) {
            disasmContainer.innerHTML = '<div class="disasm-placeholder">Loading disassembly stream...</div>';
        }

        fetchMemory(state.mem.baseAddr);
        updateStackView(state.registers.sp, [], state.registers.sp, false);
        updateInspector();
        updateExecutionControls();
    }

    function updateExecutionControls() {
        const btnRun = document.getElementById('btn-cpu-run');
        const btnPause = document.getElementById('btn-cpu-pause');
        const btnStep = document.getElementById('btn-cpu-step');
        const badge = document.getElementById('cpu-status-badge');

        if (!state.wsConnected) {
            if (btnRun) btnRun.disabled = true;
            if (btnPause) btnPause.disabled = true;
            if (btnStep) btnStep.disabled = true;
            if (badge) {
                badge.textContent = 'OFFLINE';
                badge.className = 'cpu-status-badge offline';
            }
        } else if (state.aiLocked) {
            if (btnRun) btnRun.disabled = true;
            if (btnPause) btnPause.disabled = true;
            if (btnStep) btnStep.disabled = true;
            if (badge) {
                badge.textContent = state.paused ? 'PAUSED' : 'RUNNING';
                badge.className = 'cpu-status-badge ' + (state.paused ? 'paused' : 'running');
            }
        } else if (state.paused) {
            if (btnRun) btnRun.disabled = false;
            if (btnPause) btnPause.disabled = true;
            if (btnStep) btnStep.disabled = false;
            if (badge) {
                badge.textContent = 'PAUSED';
                badge.className = 'cpu-status-badge paused';
            }
        } else {
            if (btnRun) btnRun.disabled = true;
            if (btnPause) btnPause.disabled = false;
            if (btnStep) btnStep.disabled = true;
            if (badge) {
                badge.textContent = 'RUNNING';
                badge.className = 'cpu-status-badge running';
            }
        }
    }

    function handleDisasmResponse(msg) {
        if (!msg || typeof msg.addr !== 'number') return;
        if (msg.addr !== state.disasm.pendingAddr) {
            return;
        }
        state.disasm.pendingAddr = null;
        state.disasm.lines = Array.isArray(msg.lines) ? msg.lines : [];
        if (state.disasm.lines.length > 0) {
            state.disasm.minAddr = state.disasm.lines[0].addr;
            state.disasm.maxAddr = state.disasm.lines[state.disasm.lines.length - 1].addr;
        } else {
            state.disasm.minAddr = null;
            state.disasm.maxAddr = null;
        }
        renderDisassembly();
    }

    function parseHexInput(str) {
        if (typeof str !== 'string') return NaN;
        let s = str.trim();
        if (s.startsWith('$')) s = s.substring(1);
        else if (s.startsWith('0x') || s.startsWith('0X')) s = s.substring(2);
        if (s.endsWith('h') || s.endsWith('H')) s = s.substring(0, s.length - 1);
        if (!/^[0-9a-fA-F]+$/.test(s)) return NaN;
        return parseInt(s, 16);
    }

    function fetchMemory(addr, count = 128) {
        addr = ((addr % 0x10000) + 0x10000) % 0x10000;
        state.mem.pendingAddr = addr;
        sendHostCommand({ type: 'readMem', addr: addr, count: count });
    }

    function fetchStack(sp) {
        if (state.stack.pendingAddr !== null) return;
        sp = ((sp % 0x10000) + 0x10000) % 0x10000;
        state.stack.pendingAddr = sp;
        sendHostCommand({ type: 'readMem', addr: sp, count: 32 });
    }

    function handleMemDataResponse(msg) {
        if (!msg || typeof msg.addr !== 'number' || !Array.isArray(msg.data)) return;
        let handled = false;
        if (state.mem.pendingAddr !== null && msg.addr === state.mem.pendingAddr) {
            state.mem.pendingAddr = null;
            state.mem.baseAddr = msg.addr;
            state.mem.bytes = msg.data;
            renderMemoryTable();
            handled = true;
        }
        if (state.stack.pendingAddr !== null && msg.addr === state.stack.pendingAddr) {
            state.stack.pendingAddr = null;
            state.stack.baseAddr = msg.addr;
            state.stack.stackBytes = msg.data;
            updateStackView(state.registers.sp, msg.data, msg.addr, state.paused);
            handled = true;
        }
        if (!handled) {
            if (msg.addr === state.mem.baseAddr) {
                state.mem.bytes = msg.data;
                renderMemoryTable();
            } else if (msg.addr === state.registers.sp && msg.data.length === 32) {
                state.stack.baseAddr = msg.addr;
                state.stack.stackBytes = msg.data;
                updateStackView(state.registers.sp, msg.data, msg.addr, state.paused);
            }
        }
    }

    function renderMemoryTable() {
        const table = document.getElementById('hex-editor-table');
        if (!table) return;
        if (state.mem.bytes.length === 0) {
            table.innerHTML = '<div class="disasm-placeholder">No memory data available.</div>';
            return;
        }
        let html = '';
        for (let i = 0; i < state.mem.bytes.length; i += 16) {
            const rowAddr = (state.mem.baseAddr + i) & 0xFFFF;
            const addrStr = rowAddr.toString(16).toUpperCase().padStart(4, '0');
            const slice = state.mem.bytes.slice(i, Math.min(i + 16, state.mem.bytes.length));
            const bytesHtml = slice.map(b => `<span class="hex-byte">${b.toString(16).toUpperCase().padStart(2, '0')}</span>`).join(' ');
            const asciiStr = slice.map(b => (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.').join('');
            html += `<div class="hex-row">` +
                    `<span class="hex-addr">${addrStr}:</span>` +
                    `<span class="hex-bytes">${bytesHtml}</span>` +
                    `<span class="hex-ascii">${asciiStr}</span>` +
                    `</div>`;
        }
        table.innerHTML = html;

        const offsetInput = document.getElementById('mem-offset-input');
        if (offsetInput && document.activeElement !== offsetInput) {
            offsetInput.value = state.mem.baseAddr.toString(16).toUpperCase().padStart(4, '0');
        }

        document.querySelectorAll('.mem-jump-bar .chip-btn').forEach(btn => {
            const chipAddr = parseHexInput(btn.getAttribute('data-addr'));
            btn.classList.toggle('active', chipAddr === state.mem.baseAddr);
        });
    }

    function updateStackView(currentSp, stackBytes, baseAddr, isPaused) {
        if (isPaused) {
            const delta = Math.abs(currentSp - state.stack.lastSp);
            if (delta > 64 || currentSp > state.stack.anchorSp) {
                state.stack.anchorSp = currentSp;
                const anchorInput = document.getElementById('stack-anchor-input');
                if (anchorInput && document.activeElement !== anchorInput) {
                    anchorInput.value = currentSp.toString(16).toUpperCase().padStart(4, '0');
                }
            }
        }
        state.stack.lastSp = currentSp;
        if (Array.isArray(stackBytes) && stackBytes.length > 0) {
            state.stack.stackBytes = stackBytes;
            state.stack.baseAddr = typeof baseAddr === 'number' ? baseAddr : currentSp;
        }

        const spVal = document.getElementById('stack-sp-val');
        if (spVal) {
            spVal.textContent = '0x' + currentSp.toString(16).toUpperCase().padStart(4, '0');
        }

        const depthWords = Math.max(0, Math.floor((state.stack.anchorSp - currentSp) / 2));
        const depthVal = document.getElementById('stack-depth-val');
        if (depthVal) {
            depthVal.textContent = depthWords.toString();
        }

        const table = document.getElementById('stack-editor-table');
        if (table) {
            if (!isPaused) {
                table.classList.add('dimmed');
            } else {
                table.classList.remove('dimmed');
            }
        }

        renderStackTable(currentSp, state.stack.anchorSp, state.stack.stackBytes, depthWords);
    }

    function getStackLineCount() {
        const table = document.getElementById('stack-editor-table');
        if (table && table.clientHeight > 0) {
            const fit = Math.floor(table.clientHeight / 20);
            return Math.max(4, Math.min(32, fit));
        }
        return 8;
    }

    function renderStackTable(currentSp, anchorSp, stackBytes, depthWords) {
        const table = document.getElementById('stack-editor-table');
        if (!table) return;
        if (!stackBytes || stackBytes.length === 0) {
            table.innerHTML = '<div class="disasm-placeholder">Loading stack data...</div>';
            return;
        }
        let html = '';
        const maxWords = getStackLineCount();
        const numWords = Math.min(maxWords, Math.floor(stackBytes.length / 2));
        for (let w = 0; w < numWords; w++) {
            const wordAddr = (currentSp + w * 2) & 0xFFFF;
            const isTos = (w === 0);
            const marker = isTos ? '▶ TOS' : '&nbsp;';
            const offset = w * 2;
            const relOffset = `(SP+${offset.toString(16).toUpperCase().padStart(2, '0')})`;
            const bLow = stackBytes[w * 2];
            const bHigh = stackBytes[w * 2 + 1];
            const wordVal = (bHigh << 8) | bLow;
            const addrStr = wordAddr.toString(16).toUpperCase().padStart(4, '0');
            const wordStr = wordVal.toString(16).toUpperCase().padStart(4, '0');
            const bytesStr = `${bLow.toString(16).toUpperCase().padStart(2, '0')} ${bHigh.toString(16).toUpperCase().padStart(2, '0')}`;
            html += `<div class="stack-row${isTos ? ' tos' : ''}" data-addr="${wordAddr}">` +
                    `<span class="stack-marker">${marker}</span>` +
                    `<span class="stack-addr">${addrStr}:</span>` +
                    `<span class="stack-rel">${relOffset}</span>` +
                    `<span class="stack-word">[ ${wordStr} ]</span>` +
                    `<span class="stack-bytes">${bytesStr}</span>` +
                    `</div>`;
        }
        table.innerHTML = html;
    }

    function renderDisassembly() {
        const container = document.getElementById('disasm-container');
        if (!container) return;
        if (state.disasm.lines.length === 0) {
            container.innerHTML = '<div class="disasm-placeholder">No instructions decoded.</div>';
            return;
        }
        const pc = state.disasm.lastPc;
        const html = state.disasm.lines.map(line => {
            const isCurrent = (line.addr === pc);
            const addrStr = line.addr.toString(16).toUpperCase().padStart(4, '0');
            const bytesStr = (line.bytes || []).map(b => b.toString(16).toUpperCase().padStart(2, '0')).join(' ');
            const m = line.mnemonic || '';
            const spaceIdx = m.indexOf(' ');
            const op = spaceIdx >= 0 ? m.substring(0, spaceIdx) : m;
            const args = spaceIdx >= 0 ? m.substring(spaceIdx + 1) : '';
            return `<div class="disasm-line${isCurrent ? ' current' : ''}" data-addr="${line.addr}">` +
                   `<span class="dis-cursor">${isCurrent ? '▶' : '&nbsp;'}</span>` +
                   `<span class="dis-addr">${addrStr}:</span>` +
                   `<span class="dis-bytes">${bytesStr}</span>` +
                   `<span class="dis-op">${op}</span>` +
                   `<span class="dis-args">${args}</span>` +
                   `</div>`;
        }).join('');
        container.innerHTML = html;
    }

    function getDisasmLineCount() {
        const container = document.getElementById('disasm-container');
        if (container && container.clientHeight > 0) {
            const fit = Math.floor(container.clientHeight / 20);
            return Math.max(8, Math.min(64, fit));
        }
        return 16;
    }

    function updateDisassembly(pc, isPaused) {
        state.disasm.lastPc = pc;
        const container = document.getElementById('disasm-container');
        if (!container) return;

        if (!isPaused) {
            container.classList.add('dimmed');
            // Running: no relocation traffic. The only request allowed is the single
            // cold-start fetch while no window exists yet, so the panel is never blank.
            if (state.disasm.minAddr === null || state.disasm.maxAddr === null) {
                const now = Date.now();
                if (state.disasm.pendingAddr !== null && now - (state.disasm.lastRelocateTime || 0) > 1000) {
                    state.disasm.pendingAddr = null;
                }
                if (state.disasm.pendingAddr === null) {
                    state.disasm.lastRelocateTime = now;
                    state.disasm.pendingAddr = pc;
                    sendHostCommand({ type: 'readDisasm', addr: pc, count: getDisasmLineCount() });
                }
            }
            return;
        }

        container.classList.remove('dimmed');

        let isOnRow = false;
        if (state.disasm.lines.length > 0 && state.disasm.minAddr !== null && state.disasm.maxAddr !== null) {
            if (pc >= state.disasm.minAddr && pc <= state.disasm.maxAddr) {
                isOnRow = state.disasm.lines.some(l => l.addr === pc);
            }
        }

        if (!isOnRow) {
            const now = Date.now();
            if (state.disasm.pendingAddr !== null && now - (state.disasm.lastRelocateTime || 0) > 1000) {
                state.disasm.pendingAddr = null;
            }
            if (state.disasm.pendingAddr === null) {
                state.disasm.lastRelocateTime = now;
                state.disasm.pendingAddr = pc;
                sendHostCommand({ type: 'readDisasm', addr: pc, count: getDisasmLineCount() });
            }
        } else {
            renderDisassembly();
        }
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
        document.getElementById('reg-ix').textContent = state.registers.ix === null ? '----' : '0x' + state.registers.ix.toString(16).toUpperCase().padStart(4, '0');
        document.getElementById('reg-iy').textContent = state.registers.iy === null ? '----' : '0x' + state.registers.iy.toString(16).toUpperCase().padStart(4, '0');
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
        if (e.key === '-' || e.key === 'F10') return '-';
        if (e.key === 'Enter') return 'GO EXEC';
        const k = e.key.toLowerCase();
        if (k === 'g') return 'GO EXEC';
        if (k === 'm') return 'ADRS';   // A and D are hex digits, so ADRS and DATA use M and N
        if (k === 'n') return 'DATA';
        if (k === 'w') return 'TO TAPE';
        if (k === 'l') return 'FROM TAPE';
        return null;
    }

    function qbKeyNameForEvent(e) {
        if (e.key === 'F1') return 'F1';
        if (e.shiftKey) {
            if (e.key === 'F5') return 'SHIFT+F5';
            if (e.key === 'F9') return 'SHIFT+F9';
            if (e.key === 'F10') return 'SHIFT+F10';
            if (e.key === 'Escape') return 'SHIFT+ESC';
        }
        if (e.key === 'Home') return 'HOME';
        if (e.key === 'End') return 'END';
        if (e.key === 'PageUp') return 'PAGEUP';
        if (e.key === 'PageDown') return 'PAGEDOWN';
        if (e.key === 'Insert') return 'INSERT';
        if (e.key === 'Delete') return 'DELETE';
        if (e.key === 'Enter') return 'ENTER';
        if (e.key === 'Backspace') return 'BACKSPACE';
        if (e.key === 'Escape') return 'ESCAPE';

        const promptModes = ['save_file', 'load_file', 'save_done'];
        if (promptModes.includes(state.qb.mode)) {
            if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
                return e.key;
            }
        }
        if (/^[0-9a-fA-F]$/.test(e.key) && !e.ctrlKey && !e.altKey && !e.metaKey) {
            return e.key.toUpperCase();
        }
        return null;
    }

    function setupKeyboardListeners() {
        const heldByCode = {};  // physical key -> board key, so a release always matches its press

        window.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA') return;
            if (e.key === 'F12') {
                e.preventDefault();
                document.getElementById('btn-toggle-inspector').click();
                return;
            }
            if (state.aiLocked) {
                return;
            }
            // When the PC/AT Color VGA CRT Monitor is visible & focused, route keys to QuickBASIC
            if (state.qb.visible && state.qb.focused) {
                const qbKey = qbKeyNameForEvent(e);
                if (qbKey !== null) {
                    e.preventDefault();
                    if (qbKey === 'F1') {
                        connectQbHost();
                    } else if (qbKey === 'SHIFT+ESC') {
                        setQbFocus(false);
                        sendHostCommand({ type: 'qb_key', key: 'SHIFT+ESC' });
                    } else {
                        sendQbKey(qbKey);
                    }
                }
                return;
            }
            if (e.key === 'Escape') {
                e.preventDefault();
                if (!e.repeat) handleHardwareReset();
                return;
            }
            if (e.key === 'F5') {
                e.preventDefault();
                if (!e.repeat) sendRun();
                return;
            }
            if (e.key === 'F6') {
                e.preventDefault();
                if (!e.repeat) sendPause();
                return;
            }
            if (e.key === 'F7' || e.key === ' ') {
                e.preventDefault();
                if (!e.repeat) sendStep();
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
            if (!state.aiLocked) {
                handleKeypadRelease(name);
            }
        });

        window.addEventListener('blur', () => {
            Object.keys(heldByCode).forEach(code => {
                if (!state.aiLocked) {
                    handleKeypadRelease(heldByCode[code]);
                }
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
        'ic-u1': { refdes: 'U1', name: 'Z-80 CPU', pkg: '40-pin, socketed (p.104)', specs: 'Z-80 CPU (parts list p.96). One T-state is 0.56 us, about 1.79 MHz (monitor listing p.192)', role: 'Central processing unit' },
        'ic-u2': { refdes: 'U2', name: '2732 EPROM', pkg: '24-pin, socketed (p.104)', specs: '2732, 4K x 8 (parts list p.96); the one 2732 supplied', role: 'Holds the monitor program (listing pp.166-204)' },
        'ic-u3': { refdes: 'U3', name: '2732 expansion ROM socket', pkg: '24-pin socket (p.104)', specs: '4K expansion ROM socket at U3, empty as supplied; a user program burned into a 2732 can be plugged in and run here (p.90)', role: 'Expansion ROM (p.90)' },
        'ic-u4': { refdes: 'U4', name: '6116 RAM', pkg: '24-pin, socketed (p.104)', specs: '6116, 2K x 8 (p.90, parts list p.96)', role: 'Work RAM (p.90)' },
        'ic-u5': { refdes: 'U5', name: '8255 PPI (main)', pkg: '40-pin, socketed (p.104)', specs: '8255, one of two (parts list p.96)', role: 'Keypad input, 7-segment and digit outputs, speaker output (monitor listing pp.192-195)' },
        'ic-u6': { refdes: 'U6', name: '8255 PPI (EPROM programmer)', pkg: '40-pin, socketed (p.104)', specs: '8255, one of two (parts list p.96). The EPROM programmer uses an 8255, a 74LS04 and a 741 (p.105); the monitor drives it in BURN and DATAIN (pp.178-179)', role: 'EPROM programmer interface' },
        'ic-u7': { refdes: 'U7', name: 'EPROM programmer socket', pkg: '24-pin socket (p.104)', specs: 'Socket for burning a 2732, or a 2716 with modification; when not burning it can be used as an I/O port (p.90)', role: 'EPROM burner (p.90)' },
        'ic-u8': { refdes: 'U8', name: '74LS139', pkg: '16-pin, soldered directly (p.104)', specs: '74LS139 (parts list p.96)', role: 'Function not stated in the book.' },
        'ic-u9': { refdes: 'U9', name: '74LS14', pkg: '14-pin, soldered directly (p.104)', specs: '74LS14 (parts list p.96)', role: 'Function not stated in the book.' },
        'ic-u10': { refdes: 'U10', name: '74LS04', pkg: '14-pin, soldered directly (p.104)', specs: '74LS04 (p.105); listed as 7404 in the parts list (p.96)', role: 'Part of the EPROM programmer (p.105)' },
        'ic-u11': { refdes: 'U11', name: '741', pkg: '8-pin, soldered directly (p.104)', specs: '741 (parts list p.96, p.105)', role: 'Part of the EPROM programmer (p.105)' },
        'silk-abc80-eprom': { refdes: 'SILK', name: 'Board legend ABC-80 EPROM', pkg: 'Printed legend', specs: 'Legend printed on the board (p.95, p.98)', role: 'Marks the EPROM programmer area (p.95)' },

        // --- TO-92 Transistors ---
        'disc-q1': { refdes: 'Q1', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q2': { refdes: 'Q2', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q3': { refdes: 'Q3', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q4': { refdes: 'Q4', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q5': { refdes: 'Q5', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q6': { refdes: 'Q6', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q7': { refdes: 'Q7', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q8': { refdes: 'Q8', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Around the display (p.103). Function not stated in the book.' },
        'disc-q9': { refdes: 'Q9', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q10': { refdes: 'Q10', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q11': { refdes: 'Q11', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q12': { refdes: 'Q12', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q13': { refdes: 'Q13', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q14': { refdes: 'Q14', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q15': { refdes: 'Q15', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q16': { refdes: 'Q16', name: 'PNP transistor', pkg: '3 leads E C B (p.103)', specs: 'One of 16 PNP transistors (parts list p.96); 8 are placed around the display and 8 on the left of the board (p.103)', role: 'Left side of the board (p.103). Function not stated in the book.' },
        'disc-q17': { refdes: 'Q17', name: '9013 NPN transistor', pkg: '3 leads C B E (p.103)', specs: 'The only NPN transistor (parts list p.96), placed in the middle of the board (p.103)', role: 'Controls the 25 V supply of the EPROM programmer (p.103)' },

        // --- Bus Pull-Up Resistors (18x 10k) ---
        'disc-r-pu1': { refdes: 'RP1.1', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu2': { refdes: 'RP1.2', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu3': { refdes: 'RP1.3', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu4': { refdes: 'RP1.4', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu5': { refdes: 'RP1.5', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu6': { refdes: 'RP1.6', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu7': { refdes: 'RP1.7', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu8': { refdes: 'RP1.8', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu9': { refdes: 'RP1.9', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu10': { refdes: 'RP1.10', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu11': { refdes: 'RP1.11', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu12': { refdes: 'RP1.12', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu13': { refdes: 'RP1.13', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu14': { refdes: 'RP1.14', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu15': { refdes: 'RP1.15', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu16': { refdes: 'RP1.16', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu17': { refdes: 'RP1.17', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-pu18': { refdes: 'RP1.18', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); one of the 18 marked 10K x18 (p.95, p.100)', role: 'Function not stated in the book.' },

        // --- Keypad Column Pull-Up Resistors (8x 3.3k) ---
        'disc-r-q9': { refdes: 'R(Q9)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q10': { refdes: 'R(Q10)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q11': { refdes: 'R(Q11)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q12': { refdes: 'R(Q12)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q13': { refdes: 'R(Q13)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q14': { refdes: 'R(Q14)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q15': { refdes: 'R(Q15)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-q16': { refdes: 'R(Q16)', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); one of the 8 marked 3.3k x8 (p.95, p.100)', role: 'Function not stated in the book.' },

        // --- Programmer & Clock Bias Resistors ---
        'disc-res-28v': { refdes: 'R-28V', name: '33 ohm resistor', pkg: '1/16 W', specs: '33 ohm, 1/16 W (parts list p.96); marked 33 beside the 28 V node (p.95)', role: 'Beside the 28 V node (p.95). Function not stated in the book.' },
        'disc-led-ep': { refdes: 'LED-EP', name: 'Red LED', pkg: 'LED (p.102)', specs: 'Red LED, one of two red and one green (parts list p.96); marked EP on the board (p.95)', role: 'Marked EP, beside the 28 V node (p.95)' },
        'disc-r-ep-1k8': { refdes: 'R-EP', name: '1.8K ohm resistor', pkg: '1/16 W', specs: '1.8K ohm, 1/16 W (parts list p.96); marked 1.8k beside the EP LED (p.95)', role: 'Beside the EP LED (p.95). Function not stated in the book.' },
        'disc-r-prog1': { refdes: 'R-PROG1', name: '1.8K ohm resistor', pkg: '1/16 W', specs: '1.8K ohm, 1/16 W (parts list p.96); drawn near the EPROM programmer ICs (p.95)', role: 'Near the EPROM programmer ICs (p.95). Function not stated in the book.' },
        'disc-r-prog2': { refdes: 'R-PROG2', name: '3.3K ohm resistor', pkg: '1/16 W', specs: '3.3K ohm, 1/16 W (parts list p.96); drawn near the EPROM programmer ICs (p.95)', role: 'Near the EPROM programmer ICs (p.95). Function not stated in the book.' },
        'disc-r-prog3': { refdes: 'R-PROG3', name: '560 ohm resistor', pkg: '1/16 W', specs: '560 ohm, 1/16 W (parts list p.96); drawn near the EPROM programmer ICs (p.95)', role: 'Near the EPROM programmer ICs (p.95). Function not stated in the book.' },
        'disc-r-prog-10k': { refdes: 'R-PROG-10K', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96); drawn near the EPROM programmer ICs (p.95)', role: 'Near the EPROM programmer ICs (p.95). Function not stated in the book.' },
        'disc-r-prog4': { refdes: 'R-PROG4', name: '47K ohm resistor', pkg: '1/16 W', specs: '47K ohm, 1/16 W (parts list p.96); drawn near the EPROM programmer ICs (p.95)', role: 'Near the EPROM programmer ICs (p.95). Function not stated in the book.' },
        'disc-r-clock': { refdes: 'R-CLK', name: '330 ohm resistor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has three 330 ohm resistors and the drawing already shows all three elsewhere; the book has no crystal circuit', role: 'Not in the book' },

        // --- Reset & Clock Subsystem Passives ---
        'disc-cap-50p': { refdes: 'C-50P', name: '50 pF capacitor', pkg: 'Capacitor', specs: '50 pF (parts list p.96); marked 50 (p.95)', role: 'Function not stated in the book.' },
        'disc-cap-101a': { refdes: 'C-101A', name: '100 pF capacitor', pkg: 'Capacitor', specs: '100 pF (parts list p.96); one of two', role: 'Function not stated in the book.' },
        'disc-cap-101b': { refdes: 'C-101B', name: '100 pF capacitor', pkg: 'Capacitor', specs: '100 pF (parts list p.96); one of two', role: 'Function not stated in the book.' },
        'disc-cap-10u': { refdes: 'C-10U', name: '10 uF/16 V electrolytic capacitor', pkg: 'Capacitor', specs: '10 uF, 16 V (parts list p.96, p.101); polarity matters (p.101)', role: 'Function not stated in the book.' },
        'disc-r-rst-10k': { refdes: 'R-RST-10K', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96)', role: 'Function not stated in the book.' },
        'disc-d-rst': { refdes: 'D-RST', name: 'IN914 diode', pkg: 'Glass diode (p.102)', specs: 'IN914 detector diode, glass (parts list p.96, p.102); one of two', role: 'Function not stated in the book.' },

        // --- Power & Cassette Header Subsystem Passives ---
        'disc-r-ear-330': { refdes: 'R-EAR-330', name: '330 ohm resistor', pkg: '1/16 W', specs: '330 ohm, 1/16 W (parts list p.96)', role: 'Function not stated in the book.' },
        'disc-d-ear': { refdes: 'D-EAR', name: 'IN914 diode', pkg: 'Glass diode (p.102)', specs: 'IN914 detector diode, glass (parts list p.96, p.102); one of two', role: 'Function not stated in the book.' },
        'disc-cap-ear-203': { refdes: 'C-EAR-203', name: '0.02 uF capacitor (marked 203)', pkg: 'Capacitor', specs: '0.02 uF, marked 203 = 20 x 10^3 pF (parts list p.96, p.101); one of four', role: 'Function not stated in the book.' },
        'disc-cap-mic-203': { refdes: 'C-MIC-203', name: '0.02 uF capacitor (marked 203)', pkg: 'Capacitor', specs: '0.02 uF, marked 203 = 20 x 10^3 pF (parts list p.96, p.101); one of four', role: 'Function not stated in the book.' },
        'disc-r-mic-10k1': { refdes: 'R-MIC1', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96)', role: 'Function not stated in the book.' },
        'disc-r-mic-10k2': { refdes: 'R-MIC2', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96)', role: 'Function not stated in the book.' },
        'disc-r-mic-10k3': { refdes: 'R-MIC3', name: '10K ohm resistor', pkg: '1/16 W', specs: '10K ohm, 1/16 W (parts list p.96)', role: 'Function not stated in the book.' },
        'disc-cap-dc-203': { refdes: 'C-DC-203', name: '0.02 uF capacitor (marked 203)', pkg: 'Capacitor', specs: '0.02 uF, marked 203 = 20 x 10^3 pF (parts list p.96, p.101); one of four', role: 'Function not stated in the book.' },
        'disc-cap-dc-100u': { refdes: 'C-DC-100U', name: '100 uF/16 V electrolytic capacitor', pkg: 'Capacitor', specs: '100 uF, 16 V (parts list p.96, p.101); one of two; polarity matters', role: 'Function not stated in the book.' },
        'disc-cap-7805-203': { refdes: 'C-7805-203', name: '0.02 uF capacitor (marked 203)', pkg: 'Capacitor', specs: '0.02 uF, marked 203 = 20 x 10^3 pF (parts list p.96, p.101); one of four', role: 'Function not stated in the book.' },

        // --- Display Segment Resistors (8x 120R) ---
        'disc-r-seg1': { refdes: 'R-SEG1', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg2': { refdes: 'R-SEG2', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg3': { refdes: 'R-SEG3', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg4': { refdes: 'R-SEG4', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg5': { refdes: 'R-SEG5', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg6': { refdes: 'R-SEG6', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg7': { refdes: 'R-SEG7', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },
        'disc-r-seg8': { refdes: 'R-SEG8', name: '120 ohm resistor', pkg: '1/16 W', specs: '120 ohm, 1/16 W (parts list p.96); one of the 8 marked 120 x8 (p.95, p.100)', role: 'Function not stated in the book.' },

        // --- Capacitors ---
        'disc-cap-c2': { refdes: 'C2', name: '100 uF/16 V electrolytic capacitor', pkg: 'Capacitor', specs: '100 uF, 16 V (parts list p.96, p.101); one of two; polarity matters', role: 'Function not stated in the book.' },
        'disc-cap-c1': { refdes: 'C1', name: 'Capacitor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has 10 capacitors (4 x 0.02 uF, 2 x 100 pF, 1 x 50 pF, 2 x 100 uF, 1 x 10 uF) and the drawing already shows all ten', role: 'Not in the book' },
        'disc-tantalum-cp': { refdes: 'CP', name: 'Capacitor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has 10 capacitors (4 x 0.02 uF, 2 x 100 pF, 1 x 50 pF, 2 x 100 uF, 1 x 10 uF) and the drawing already shows all ten', role: 'Not in the book' },
        'disc-cap-vcc1': { refdes: 'C-VCC1', name: 'Capacitor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has 10 capacitors (4 x 0.02 uF, 2 x 100 pF, 1 x 50 pF, 2 x 100 uF, 1 x 10 uF) and the drawing already shows all ten', role: 'Not in the book' },
        'disc-cap-vcc2': { refdes: 'C-VCC2', name: 'Capacitor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has 10 capacitors (4 x 0.02 uF, 2 x 100 pF, 1 x 50 pF, 2 x 100 uF, 1 x 10 uF) and the drawing already shows all ten', role: 'Not in the book' },
        'disc-cap-vcc3': { refdes: 'C-VCC3', name: 'Capacitor (not in the book)', pkg: '—', specs: 'NOT on the original board: the parts list (p.96) has 10 capacitors (4 x 0.02 uF, 2 x 100 pF, 1 x 50 pF, 2 x 100 uF, 1 x 10 uF) and the drawing already shows all ten', role: 'Not in the book' },

        // --- Power, Jacks, Switches & Ports ---
        'disc-lm7805': { refdes: 'IC-7805', name: '7805 voltage regulator', pkg: 'On a heat sink (p.104)', specs: '7805 (parts list p.96), mounted with a screw on a heat sink at the top right of the board (p.103, p.104)', role: 'Voltage regulator' },
        'disc-jack-dc': { refdes: 'J-DC', name: 'DC jack (white)', pkg: 'Jack', specs: 'White DC jack (p.106); marked DC (p.95)', role: 'DC power input' },
        'disc-jack-ear': { refdes: 'J-EAR', name: 'EAR jack (black)', pkg: 'Jack', specs: 'Black EAR jack (p.106); one of three jacks (parts list p.96)', role: 'EAR jack (p.106)' },
        'disc-jack-mic': { refdes: 'J-MIC', name: 'MIC jack (black)', pkg: 'Jack', specs: 'Black MIC jack (p.106); one of three jacks (parts list p.96)', role: 'MIC jack (p.106)' },
        'disc-hw-rst': { refdes: 'SW-RST', name: 'Reset switch (not in the book)', pkg: '—', specs: 'NOT in the parts list (p.96); RST is one of the 24 keys of the keypad (monitor listing p.192, p.200)', role: 'Not in the book' },
        'disc-int-header': { refdes: 'J-INT', name: 'INT (marked on the board)', pkg: '—', specs: 'Marked INT on the board (p.95); no matching part in the parts list (p.96)', role: 'Function not stated in the book.' },
        'disc-clock-section': { refdes: 'XTAL-1', name: 'Clock oscillator (the book has no crystal)', pkg: '—', specs: 'The book lists an oscillator coil (red, medium-period inductor, p.96, p.107) with the 50 pF and 100 pF capacitors, and no crystal. The monitor T-state is 0.56 us (p.192). This drawn crystal is NOT on the original board.', role: 'Clock source' },

        // --- 7-Segment LED Displays (6x) ---
        'disc-disp-a3': { refdes: 'DISP-A3', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Address Display Digit 3 (Bits A15-A12)' },
        'disc-disp-a2': { refdes: 'DISP-A2', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Address Display Digit 2 (Bits A11-A8)' },
        'disc-disp-a1': { refdes: 'DISP-A1', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Address Display Digit 1 (Bits A7-A4)' },
        'disc-disp-a0': { refdes: 'DISP-A0', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Address Display Digit 0 (Bits A3-A0)' },
        'disc-disp-d1': { refdes: 'DISP-D1', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Data Display High Digit (Bits D7-D4)' },
        'disc-disp-d0': { refdes: 'DISP-D0', name: '7-segment display (common cathode)', pkg: 'Display', specs: 'One of 6 common-cathode 7-segment displays (parts list p.96, p.106)', role: 'Data Display Low Digit (Bits D3-D0)' },

        // --- Audio / Speaker ---
        'disc-speaker': { refdes: 'SP', name: 'Ferro-magnetic dynamic speaker', pkg: 'Round cone speaker with a magnet (pp.97, 106)', specs: 'One speaker (parts list p.96), mounted from the back of the board with its magnet protruding at the front (p.106); wired through a 33 ohm resistor marked SP (p.95)', role: 'Sound output' },
        'disc-r-sp33': { refdes: 'R-SP33', name: '33 ohm resistor', pkg: '1/16 W', specs: '33 ohm, 1/16 W (parts list p.96); marked 33 and SP beside the speaker (p.95)', role: 'In the speaker line, marked SP (p.95)' },
        'disc-r-sp330a': { refdes: 'R-SP330A', name: '330 ohm resistor', pkg: '1/16 W', specs: '330 ohm, 1/16 W (parts list p.96); beside the speaker (p.95)', role: 'Beside the speaker (p.95). Function not stated in the book.' },
        'disc-r-sp330b': { refdes: 'R-SP330B', name: '330 ohm resistor', pkg: '1/16 W', specs: '330 ohm, 1/16 W (parts list p.96); beside the speaker (p.95)', role: 'Beside the speaker (p.95). Function not stated in the book.' },
        'disc-led-audio-grn': { refdes: 'LED-AUD-GRN', name: 'Green LED', pkg: 'LED (p.102)', specs: 'The only green LED (parts list p.96); drawn beside the speaker (p.95)', role: 'Beside the speaker (p.95). Function not stated in the book.' },
        'disc-led-halt': { refdes: 'LED-HALT', name: 'Red LED', pkg: 'LED (p.102)', specs: 'Red LED, one of two (parts list p.96); drawn beside the speaker (p.95)', role: 'Beside the speaker (p.95). Function not stated in the book.' },

        // --- Dual 3x4 Keypad Modules ---
        'disc-keypad-left': { refdes: 'KEY-L', name: 'Keypad module (12 keys)', pkg: 'Keypad', specs: 'One of two 12-key keypad modules, 24 keys in all (parts list p.96; pp.106-107)', role: 'Keypad (key code table, pp.202-203)' },
        'disc-keypad-right': { refdes: 'KEY-R', name: 'Keypad module (12 keys)', pkg: 'Keypad', specs: 'One of two 12-key keypad modules, 24 keys in all (parts list p.96; pp.106-107)', role: 'Keypad (key code table, pp.202-203)' }
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
        initInstanceSelect();
        initAiControls();
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

        initCassetteDeck();
        initQbCrtWindow();

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
        'disc-speaker': { left: 339.2, top: 376.0, width: 50.0, height: 50.0 },
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
                        ids: ['disc-speaker', 'disc-r-sp33', 'disc-r-sp330a', 'disc-r-sp330b', 'disc-led-audio-grn', 'disc-led-halt'],
                        format: (id, c) => {
                            let r = `#${id} { left: ${c.left}px; top: ${c.top}px;`;
                            if (id !== 'disc-speaker' && c.width !== undefined && c.height !== undefined) {
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
