# SmartGrow Firmware (v5.2)

| Sketch | Purpose |
|---|---|
| `SmartGrow_Diagnostics/SmartGrow_Diagnostics.ino` | Run **first**. No WiFi. Tests every sensor and relay channel, and gives the raw numbers for calibration. |
| `SmartGrow_ESP32/SmartGrow_ESP32.ino` (+ `config.h`) | The real firmware: sensors, local automation with fail-safes, WiFi, reporting to the dashboard. |

## Hardware

- ESP32 DevKit (30/38-pin)
- DHT11 temperature/humidity sensor
- Soil moisture sensor (analog output)
- Light sensor (LDR / LDR module with analog output)
- 4-channel relay module
- 12V fan, 12V water pump, warm light
- A 12V power supply (for fan + pump) and a **5V buck converter** (12V -> 5V) to power the ESP32 and relay module

## Wiring (matches `config.h`)

### Sensors

| Component | Connect to | ESP32 pin |
|---|---|---|
| DHT11 `VCC` / `GND` / `DATA` | 3.3V / GND / data | **GPIO 14** (needs a 10k pull-up to 3.3V if it is a bare 4-pin sensor; 3-pin modules already have one) |
| Soil sensor `VCC` / `GND` / `AO` | 3.3V / GND / analog out | **GPIO 34** |
| Light sensor `VCC` / `GND` / `AO` | 3.3V / GND / analog out | **GPIO 35** |

### 4-channel relay module

| Relay module | ESP32 | Controls |
|---|---|---|
| `VCC` | 5V (from the buck converter, **not** from the ESP32 USB pin if you can avoid it) | |
| `GND` | GND (shared with ESP32) | |
| `IN1` | GPIO 25 | 12V pump |
| `IN2` | GPIO 26 | warm light |
| `IN3` | GPIO 27 | 12V fan |
| `IN4` | GPIO 33 | spare (kept OFF) |

All grounds must be connected together: ESP32 GND, relay GND, buck converter GND, and 12V supply GND.

### Relay output side (12V devices)

Per channel: **12V+ -> COM**, **NO -> device (+)**, **device (-) -> 12V GND**. Use NO (normally open) so a device is OFF when the relay is unpowered.

- Put a **1N4007 diode across the pump and the fan** (stripe/band toward the **+** wire). This absorbs the voltage spike when a motor switches off. Without it, those spikes are a classic cause of ESP32 resets and WiFi dropouts.
- **Warm light:** if it is a 12V light, wire it exactly like the fan. If it is a **mains (110/220V)** bulb or heat lamp, the relay switches the live wire. Mains wiring can kill you. Use a relay module rated for the load, a closed enclosure, keep it away from the low-voltage side and from water, or have a qualified person do it.

## Why the ESP32 "sometimes disconnects"

Two causes were found in v5.1, both fixed in v5.2:

1. **Each post did three TLS handshakes** (a diagnostic one, a real one, plus waiting on slow ones) every 2 seconds. That could take 4-10 s per cycle while the dashboard declared "disconnected" after only 8 s. v5.2 does one handshake, every 5 s, with bounded timeouts, and the dashboard waits 20 s.
2. **WiFi power-save was on**, and the reconnect logic could cancel its own in-progress connection. v5.2 disables WiFi sleep, reconnects every 10 s without interrupting itself, logs the exact disconnect reason code, and reboots only if WiFi stays down for 10 minutes.

If it still drops, the cause is almost always **power or signal**:

- "Brownout detector was triggered" in the Serial Monitor = power problem. Power the ESP32 from a proper 5V/2A supply (buck converter), not a weak USB port. The relay coils and motors must not share a thin supply with the ESP32.
- Missing diodes across the motors (see above).
- Weak WiFi: the Serial Monitor prints RSSI. Worse than about -75 dBm is unreliable. Move the router/ESP32 closer or use a better antenna.
- The ESP32 only works on **2.4 GHz** WiFi.

## 1. Install libraries

Arduino IDE -> **Tools -> Manage Libraries**:

- **DHT sensor library** by Adafruit (accept **Adafruit Unified Sensor** too)

ArduinoJson is no longer needed. Install the **esp32** board package by Espressif (Boards Manager). Select board **ESP32 Dev Module**.

## 2. Run the diagnostics sketch

1. Upload `SmartGrow_Diagnostics.ino`, open Serial Monitor at **115200**, line ending **Newline**.
2. DHT11 must print temperature/humidity (not "READ FAILED").
3. Soil: note the raw number in dry air and in a cup of water. Light sensor: note covered/dark and bright.
4. Relay test with **nothing connected to the relay outputs** first: send `p1`, `l1`, `f1`, `s1` (and `0` versions). Each relay should click. If `1` switches it OFF, set `RELAY_ACTIVE_LOW` to `false` in both sketches.
5. Then connect the devices and repeat.

## 3. Configure and flash the real firmware

1. Put your calibration numbers into `config.h` (`SOIL_RAW_*`, `LDR_RAW_*`). **Do not skip this**, the percentages depend on it.
2. Check the WiFi and server settings, then upload and watch the Serial Monitor at 115200.

## Automation behaviour

| Device | Turns ON | Turns OFF | Safety |
|---|---|---|---|
| Fan | temp >= 30 C | temp <= 28 C | OFF if DHT11 fails 3 reads in a row |
| Pump | soil <= 40% | soil >= 45% | OFF if soil sensor fails; max 30 s per run, then 2 min rest |
| Warm light | light < 300 | light >= 320 | OFF if light sensor faults |

Every relay waits at least 15 s between switches (except fail-safe OFF, which is instant). Automation runs on the ESP32 itself, so it keeps working if WiFi or the website is down.

The dashboard displays what the firmware reports; it does not command the relays. If the website/Vercel setup changed, only `storage.py` changed in v5.2 (disconnect timeout), so redeploy once.
