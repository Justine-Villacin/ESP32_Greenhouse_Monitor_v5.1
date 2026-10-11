// config.h — SmartGrow ESP32 firmware configuration (v5.2).
// Edit the values below, then flash SmartGrow_ESP32.ino.
// (In the Arduino IDE this appears as a second tab next to the .ino file —
// you don't need to "include" it yourself, just fill it in.)

#ifndef SMARTGROW_CONFIG_H
#define SMARTGROW_CONFIG_H

// ============================================================
// WiFi  (ESP32 supports 2.4 GHz networks ONLY — not 5 GHz)
// ============================================================
#define WIFI_SSID        "Delinia Fam 2.4G"
#define WIFI_PASSWORD    "03121980"

// ============================================================
// Server — pick ONE target
// ============================================================
// --- Local desktop mode (python desktop.py on your PC) ---
//     #define SERVER_USE_HTTPS  false
//     #define SERVER_HOST       "192.168.1.10"
//     #define SERVER_PORT       8000
//
// --- Hosted on Vercel (active) ---
// SERVER_HOST is the BARE DOMAIN ONLY — the firmware builds the full URL.
//   RIGHT: "smartgrowv20.vercel.app"
//   WRONG: "https://smartgrowv20.vercel.app/"
#define SERVER_USE_HTTPS   false
#define SERVER_HOST        "192.168.100.217"
#define SERVER_PORT        8000   // unused while SERVER_USE_HTTPS is true, but must stay defined
#define UPDATE_PATH        "/update"

// Must exactly match SMARTGROW_API_KEY on the server. Leave "" only while
// SMARTGROW_API_KEY is unset server-side.
#define API_KEY             ""

// ============================================================
// Pins (ESP32 DevKit v1 / similar 30-38 pin boards)
// ============================================================
// SENSORS
#define DHT_PIN              14     // DHT11 data pin
#define DHT_TYPE              DHT11
#define SOIL_PIN              34    // soil moisture analog out (ADC1, input-only pin)
#define LDR_PIN               35    // light sensor analog out  (ADC1, input-only pin)

// 4-CHANNEL RELAY MODULE  (IN1..IN4)
#define RELAY_PUMP_PIN        25    // IN1 -> 12V water pump
#define RELAY_LIGHT_PIN       26    // IN2 -> warm light
#define RELAY_FAN_PIN         35   // IN3 -> 12V fan
#define RELAY_SPARE_PIN       33    // IN4 -> spare channel (kept OFF; free for future use)

#define STATUS_LED_PIN         2    // onboard LED on most ESP32 DevKit boards

// Most 4-channel relay modules switch ON when the IN pin is driven LOW
// ("active-low"). If yours turns ON when driven HIGH, set this to false.
// SmartGrow_Diagnostics.ino lets you check this safely before anything is wired to the relays.
#define RELAY_ACTIVE_LOW     true

// ============================================================
// Calibration — REQUIRED before trusting the readings
// ============================================================
// Run SmartGrow_Diagnostics.ino first and read the raw ADC numbers (0-4095).
//   Soil sensor: raw value in dry air -> SOIL_RAW_DRY, fully in water -> SOIL_RAW_WET.
#define SOIL_RAW_DRY          3000   // reported as 0%
#define SOIL_RAW_WET          1200   // reported as 100%

//   Light sensor: raw value covered/dark -> LDR_RAW_DARK, bright light -> LDR_RAW_BRIGHT.
//   (DARK may be higher OR lower than BRIGHT depending on how it is wired —
//   both are handled. The firmware uses this to know which end of the scale
//   is "just dark" and which end means "sensor fault".)
#define LDR_RAW_DARK          4000   // reported as 0
#define LDR_RAW_BRIGHT         200   // reported as 1000

// ============================================================
// Automation thresholds
// ============================================================
// Keep in sync with the dashboard (public/js/ui.js, templates/index.html).
#define TEMP_FAN_ON_C          30.0   // fan ON at/above this
#define TEMP_FAN_OFF_C         28.0   // fan OFF at/below this

#define SOIL_PUMP_ON_PCT        40    // pump ON at/below this
#define SOIL_PUMP_OFF_PCT       45    // pump OFF at/above this

#define LIGHT_GROWLIGHT_ON      300   // warm light ON below this
#define LIGHT_GROWLIGHT_OFF     320   // warm light OFF at/above this
// NOTE: the warm light itself lights up the sensor. Set the sensor so it
// does NOT point straight at the lamp, or the light will cycle on/off.

// ============================================================
// Relay protection
// ============================================================
// After any relay changes state it will not be switched again by a
// threshold for this long. Stops rapid clicking that wears relays and
// resets/brownouts the ESP32. (Fail-safe OFF is always immediate.)
#define MIN_RELAY_HOLD_MS     15000

// Pump safety: a pump never runs longer than PUMP_MAX_RUN_MS in one go,
// then it rests for PUMP_COOLDOWN_MS so water can soak in to the soil
// sensor. Protects against a stuck/dry sensor flooding the plant or
// running the pump dry. Raise PUMP_MAX_RUN_MS if your pot needs more water.
#define PUMP_MAX_RUN_MS       30000   // 30 s
#define PUMP_COOLDOWN_MS     120000   // 2 min

// ============================================================
// Timing (milliseconds)
// ============================================================
#define SENSOR_READ_INTERVAL_MS     2500   // DHT11 needs >= 2 s between reads
#define SERVER_POST_INTERVAL_MS     5000   // how often a reading is sent to the dashboard
#define WIFI_RECONNECT_INTERVAL_MS 10000   // retry WiFi this often while disconnected
#define WIFI_REBOOT_AFTER_MS      600000   // reboot if WiFi has been down this long (0 = never)
#define HTTP_CONNECT_TIMEOUT_MS     5000
#define HTTP_RESPONSE_TIMEOUT_MS    6000
#define MIN_HEAP_FOR_TLS           40000   // skip a post if the largest free block is smaller

// ============================================================
// Sensor fault detection
// ============================================================
#define DHT_FAIL_THRESHOLD           3   // consecutive DHT failures before "disconnected"
#define RAIL_COUNT_THRESHOLD         4   // consecutive rail-pinned analog reads before "disconnected"
#define ADC_SAMPLES                 16   // analog reads averaged per measurement (noise filter)

#endif // SMARTGROW_CONFIG_H
