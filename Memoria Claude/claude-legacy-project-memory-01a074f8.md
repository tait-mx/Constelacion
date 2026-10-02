**Purpose & context**

Ernesto is building "Constelación," a wireless LED installation driven by ESP32 microcontrollers communicating over TCP/IP. The system uses one ESP32 classic as master and multiple ESP32-C3 SuperMini boards as battery-powered clients, each mapped to a musical note (do–re–mi–fa–sol–la–si–do across one octave). The master orchestrates LED activation sequences that represent musical melodies — currently "Noche de Paz" (Silent Night) — with timing derived from BPM and note fractions. The project blends embedded systems, networking, and musical structure into a single cohesive installation.

The project evolved from: a single-client blink prototype → multi-client broadcast → router-based infrastructure → per-client ID assignment → musical sequence engine → expansion to 8 clients with hardware stability work.

Ernesto has a prior related project ("La Constelación") on Arduino Mega using TLC5940NT LED sequencers, DFPlayer Mini audio, and button/LCD control — also musically structured around villancicos. The ESP32 TCP system appears to be a more advanced wireless successor.

**Current state**

- System has 8 ESP32-C3 clients (indices 0–7), each hardcoded with `MI_ID` and mapped to a musical note
- Master runs on fixed power; clients run on 18650 batteries with TP4056 charger modules
- Musical sequence engine is operational: BPM-based timing, note fractions stored as `{client, numerator, denominator}` structs, non-blocking `millis()`-based state machine
- `SILENCE` sentinel (value 255) implemented for rests — skips ON/OFF commands and LED activity while preserving tempo
- Non-legato gap implemented as a fixed 1/32-note duration clipped from note endings (`NON_LEGATO_GAP_MS`, `notaSeparada`) — Ernesto corrected an initial mislabeling as "staccato" and prefers code naming to reflect precise musical terminology
- Master LED latency indicator on pin 2: lights on ON command, turns off after half the client duration
- `WiFi.setSleep(false)` and `setNoDelay(true)` applied to both master and clients for timing improvement
- LCD1602 with I2C PCF8574 backpack integrated on master ESP32 (address `0x27`, SDA→GPIO21, SCL→GPIO22) for status display
- Active hardware issue: antenna interference from protoboards and voltage brownout during WiFi radio peaks causing client boot loops and master freezes on failed TCP writes

**On the horizon**

- Adding buck-boost regulator (TPS63020 or XL6302x module) between battery output and ESP32-C3 5V pin to stabilize voltage through current draw peaks (AMS1117-3.3 was evaluated and rejected as incompatible with LiPo discharge range)
- Patching master's `enviarComando()` to detect failed TCP writes and release client slots immediately rather than blocking
- Inverting TCP connection model (master connects to clients at fixed IPs) — confirmed viable architecturally, deferred pending hardware stability resolution
- Clients 4–7 currently hang freely for antenna clearance; permanent physical mounting solution implied

**Key learnings & principles**

- ESP32-C3 SuperMini antenna is disrupted by protoboard metal strips — boards must be kept off protoboards for reliable WiFi
- Brownout during WiFi radio transmission peaks is a root cause of boot loops; linear regulators (AMS1117) are incompatible with LiPo voltage range for this purpose
- Master must handle TCP write failures gracefully; blocking on a disconnected socket freezes the entire sequence
- `1/4` in C++ integer context evaluates to 0 — fraction durations must use float literals or separate numerator/denominator fields
- "Legato," "staccato," and "non-legato/détaché" have precise musical meanings; Ernesto expects code identifiers and comments to reflect correct terminology
- `WiFi.setSleep(false)` + `setNoDelay(true)` meaningfully reduce timing jitter for LED synchronization
- Clients must re-send `"ID:n"` on every reconnection (send is inside `conectarTCP()`); master uses a pending-zone with timeout before placing clients in indexed slots

**Approach & patterns**

- Iterative, hardware-first debugging: swap tests, isolation tests, and single-variable elimination
- Prefers complete, copy-paste-ready sketches after each round of changes
- Guided, step-by-step pace; shares screenshots when navigating unfamiliar UI
- Strong attention to naming precision — pushes back on imprecise terminology in code and corrects it explicitly
- Musical structure is central to the project's logic; sequence design mirrors real music notation concepts (BPM, note fractions, rests, articulation)

**Tools & resources**

- Hardware: ESP32 DevKit Classic (master), ESP32-C3 SuperMini ×8 (clients), 18650 batteries + TP4056 modules, TP-Link Archer AX23 router (SSID: `constelacion`, no WAN), LCD1602 + PCF8574 I2C backpack
- Master static IP: `192.168.0.150`; master MAC: `20:50:0D:E3:34:48`
- Development: Arduino IDE on macOS 10.13.6 (High Sierra) — requires esp32 core 1.0.6 for classic ESP32, with CP210x VCP driver v6.0.3
- Key files: `maestro_constelacion.ino`, `cliente_constelacion.ino`
- Version control: Git/GitHub repository "Constelacion"