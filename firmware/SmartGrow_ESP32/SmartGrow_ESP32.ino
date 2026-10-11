/*
  SmartGrow_ESP32.ino  (v5.2)
  ESP32 greenhouse controller for the SmartGrow dashboard.

  HARDWARE
    DHT11 (temp/humidity), soil moisture sensor, light sensor (LDR, analog),
    4-channel relay: IN1 = 12V pump, IN2 = warm light, IN3 = 12V fan, IN4 = spare.
    Pins are all in config.h.

  WHAT IT DOES
    - Reads sensors, runs automation LOCALLY (works even with no WiFi).
    - FAIL-SAFE: if a sensor looks disconnected, the device that depends on
      it is forced OFF immediately.
    - Pump safety: max run time + cooldown, so it can never run away.
    - Reports to the dashboard with POST /update every few seconds.
    - WiFi self-heals; sensor/automation loop is never blocked by WiFi.

  JSON SENT TO POST /update (matches app.py — unchanged from v5.1)
    temperature, humidity, soil_moisture, light  (omitted when that sensor is down)
    dht_connected, soil_connected, light_connected
    fan, water_pump, grow_light   ("ON"/"OFF")  <- grow_light = the warm light relay

  LIBRARIES (Arduino IDE -> Tools -> Manage Libraries)
    - "DHT sensor library" by Adafruit (accept "Adafruit Unified Sensor" too)
    ArduinoJson is NOT needed any more.
  Board package: "esp32" by Espressif (Boards Manager).
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "config.h"

DHT dht(DHT_PIN, DHT_TYPE);

// ---- Live state ----
float temperatureC = 0.0f;
float humidityPct  = 0.0f;
int   soilPct      = 0;
int   lightLevel   = 0;

bool dhtConnected   = false;
bool soilConnected  = false;
bool lightConnected = false;

bool pumpOn      = false;
bool growLightOn = false;   // the warm light
bool fanOn       = false;

// valid = we have received at least one good reading since boot / recovery
bool soilValid  = false;
bool lightValid = false;

int dhtFailCount  = 0;
int soilRailCount = 0;
int ldrRailCount  = 0;

// Time each relay last changed. Initialised "long ago" so the first
// switch after boot is allowed immediately (unsigned wrap-around is intended).
unsigned long fanChangedAt   = (unsigned long)(0UL - (unsigned long)MIN_RELAY_HOLD_MS);
unsigned long pumpChangedAt  = (unsigned long)(0UL - (unsigned long)MIN_RELAY_HOLD_MS);
unsigned long lightChangedAt = (unsigned long)(0UL - (unsigned long)MIN_RELAY_HOLD_MS);

bool pumpInCooldown = false;
unsigned long pumpCooldownStart = 0;

unsigned long lastSensorRead  = 0;
unsigned long lastServerPost  = 0;
unsigned long lastWifiAttempt = 0;
unsigned long wifiDownSince   = 0;   // 0 = WiFi currently up
int lowHeapCount = 0;
int postFailCount = 0;

// ---------------------------------------------------------------------------
// Relay helper: handles active-LOW / active-HIGH boards
// ---------------------------------------------------------------------------
void setRelay(uint8_t pin, bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void writeAllRelays() {
  setRelay(RELAY_PUMP_PIN,  pumpOn);
  setRelay(RELAY_LIGHT_PIN, growLightOn);
  setRelay(RELAY_FAN_PIN,   fanOn);
  setRelay(RELAY_SPARE_PIN, false);   // spare channel always OFF
}

// ---------------------------------------------------------------------------
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      // Reason codes: 2=auth expired, 15/204=handshake timeout, 201=AP not found,
      // 200=beacon timeout (weak signal), 202=auth fail (wrong password)
      Serial.printf("[WiFi] Disconnected, reason code %d\n", info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] Connected. IP %s  RSSI %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    default:
      break;
  }
}

void startWiFi() {
  WiFi.persistent(false);          // don't wear out flash on every begin()
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);            // modem-sleep causes random dropouts/latency
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[SmartGrow] WiFi connecting in background...");
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[SmartGrow] Booting v5.2...");

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_LIGHT_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);
  pinMode(RELAY_SPARE_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  writeAllRelays();                // everything OFF until a real reading says otherwise

  pinMode(SOIL_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  dht.begin();

  startWiFi();                     // non-blocking: automation starts immediately
}

// ---------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  maintainWiFi(now);

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;
    readSensors();
    runAutomation(millis());
    printStatus();
  }

  if (now - lastServerPost >= SERVER_POST_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      postReading();
    } else {
      Serial.println("[SmartGrow] Skipping post, WiFi not connected.");
    }
    lastServerPost = millis();     // measured AFTER the post so a slow post never causes back-to-back posts
  }

  // Status LED: solid = WiFi up, blinking = WiFi down.
  digitalWrite(STATUS_LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : (((millis() / 300) % 2) ? HIGH : LOW));

  delay(20);                       // let WiFi/idle tasks run
}

// ---------------------------------------------------------------------------
// WiFi supervision (never blocks)
// ---------------------------------------------------------------------------
void maintainWiFi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    wifiDownSince = 0;
    return;
  }

  if (wifiDownSince == 0) wifiDownSince = (now == 0) ? 1 : now;

#if WIFI_REBOOT_AFTER_MS > 0
  if (now - wifiDownSince >= (unsigned long)WIFI_REBOOT_AFTER_MS) {
    Serial.println("[SmartGrow] WiFi down too long, rebooting to recover the radio.");
    delay(100);
    ESP.restart();
  }
#endif

  if (now - lastWifiAttempt >= (unsigned long)WIFI_RECONNECT_INTERVAL_MS) {
    lastWifiAttempt = now;
    Serial.println("[SmartGrow] WiFi down, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ---------------------------------------------------------------------------
// Sensors
// ---------------------------------------------------------------------------
int readAnalogAveraged(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return (int)(sum / ADC_SAMPLES);
}

void readSensors() {
  // --- DHT11 --- a single failed read is normal; only call it
  // disconnected after several in a row. Out-of-range values are rejected
  // because the server answers HTTP 400 to them (which would make the whole
  // dashboard look offline).
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  bool dhtOk = !(isnan(h) || isnan(t)) && t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;
  if (dhtOk) {
    dhtFailCount = 0;
    temperatureC = t;
    humidityPct  = h;
    dhtConnected = true;
  } else {
    if (dhtFailCount < 1000) dhtFailCount++;
    if (dhtFailCount >= DHT_FAIL_THRESHOLD) dhtConnected = false;
  }

  // --- Soil moisture --- a reading pinned at either end of the ADC
  // range for several reads in a row means unplugged/shorted.
  int soilRaw = readAnalogAveraged(SOIL_PIN);
  bool soilRail = (soilRaw <= 5 || soilRaw >= 4090);
  if (soilRail) {
    if (soilRailCount < 1000) soilRailCount++;
  } else {
    soilRailCount = 0;
    long pct = map(soilRaw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
    soilPct = (int)constrain(pct, 0L, 100L);
    soilValid = true;
  }
  soilConnected = soilValid && (soilRailCount < RAIL_COUNT_THRESHOLD);

  // --- Light sensor --- the dark end of the scale is a LEGITIMATE reading
  // (a dark greenhouse must turn the warm light ON), so only the opposite
  // end of the scale counts as a fault.
  int ldrRaw = readAnalogAveraged(LDR_PIN);
  bool darkIsHigh = (LDR_RAW_DARK > LDR_RAW_BRIGHT);
  bool ldrFault = darkIsHigh ? (ldrRaw <= 5) : (ldrRaw >= 4090);
  if (ldrFault) {
    if (ldrRailCount < 1000) ldrRailCount++;
  } else {
    ldrRailCount = 0;
    long level = map(ldrRaw, LDR_RAW_DARK, LDR_RAW_BRIGHT, 0, 1000);
    lightLevel = (int)constrain(level, 0L, 1000L);
    lightValid = true;
  }
  lightConnected = lightValid && (ldrRailCount < RAIL_COUNT_THRESHOLD);
}

// ---------------------------------------------------------------------------
// Automation
// ---------------------------------------------------------------------------
void changeState(bool &state, bool next, unsigned long &changedAt, unsigned long now) {
  if (state != next) {
    state = next;
    changedAt = now;
  }
}

void runAutomation(unsigned long now) {
  // ---- Fan <- temperature ----
  if (!dhtConnected) {
    changeState(fanOn, false, fanChangedAt, now);            // fail-safe, immediate
  } else {
    bool want = fanOn;
    if (temperatureC >= TEMP_FAN_ON_C)       want = true;
    else if (temperatureC <= TEMP_FAN_OFF_C) want = false;
    if (want != fanOn && (now - fanChangedAt) >= (unsigned long)MIN_RELAY_HOLD_MS) {
      changeState(fanOn, want, fanChangedAt, now);
    }
  }

  // ---- Warm light <- ambient light ----
  if (!lightConnected) {
    changeState(growLightOn, false, lightChangedAt, now);    // fail-safe, immediate
  } else {
    bool want = growLightOn;
    if (lightLevel < LIGHT_GROWLIGHT_ON)        want = true;
    else if (lightLevel >= LIGHT_GROWLIGHT_OFF) want = false;
    if (want != growLightOn && (now - lightChangedAt) >= (unsigned long)MIN_RELAY_HOLD_MS) {
      changeState(growLightOn, want, lightChangedAt, now);
    }
  }

  // ---- Pump <- soil moisture, with max-run + cooldown ----
  if (!soilConnected) {
    changeState(pumpOn, false, pumpChangedAt, now);          // fail-safe, immediate
    pumpInCooldown = false;
  } else if (pumpOn) {
    if (soilPct >= SOIL_PUMP_OFF_PCT) {
      changeState(pumpOn, false, pumpChangedAt, now);        // target reached, stop at once
    } else if ((now - pumpChangedAt) >= (unsigned long)PUMP_MAX_RUN_MS) {
      changeState(pumpOn, false, pumpChangedAt, now);        // ran long enough, let water soak in
      pumpInCooldown = true;
      pumpCooldownStart = now;
      Serial.println("[SmartGrow] Pump max run reached, resting.");
    }
  } else {
    if (pumpInCooldown && (now - pumpCooldownStart) >= (unsigned long)PUMP_COOLDOWN_MS) {
      pumpInCooldown = false;
    }
    if (!pumpInCooldown && soilPct <= SOIL_PUMP_ON_PCT &&
        (now - pumpChangedAt) >= (unsigned long)MIN_RELAY_HOLD_MS) {
      changeState(pumpOn, true, pumpChangedAt, now);
    }
  }

  writeAllRelays();
}

// ---------------------------------------------------------------------------
// Reporting to the dashboard
// ---------------------------------------------------------------------------
bool postReading() {
  if (SERVER_USE_HTTPS && ESP.getMaxAllocHeap() < (size_t)MIN_HEAP_FOR_TLS) {
    Serial.printf("[SmartGrow] Low memory (%u), skipping post.\n", (unsigned)ESP.getMaxAllocHeap());
    if (++lowHeapCount >= 10) {
      Serial.println("[SmartGrow] Memory stayed low, rebooting.");
      delay(100);
      ESP.restart();
    }
    return false;
  }
  lowHeapCount = 0;

  String payload;
  payload.reserve(300);
  payload = "{";
  if (dhtConnected) {
    payload += "\"temperature\":" + String(temperatureC, 1) + ",";
    payload += "\"humidity\":" + String(humidityPct, 1) + ",";
  }
  if (soilConnected)  payload += "\"soil_moisture\":" + String(soilPct) + ",";
  if (lightConnected) payload += "\"light\":" + String(lightLevel) + ",";
  payload += String("\"dht_connected\":") + (dhtConnected ? "true" : "false") + ",";
  payload += String("\"soil_connected\":") + (soilConnected ? "true" : "false") + ",";
  payload += String("\"light_connected\":") + (lightConnected ? "true" : "false") + ",";
  payload += String("\"fan\":\"") + (fanOn ? "ON" : "OFF") + "\",";
  payload += String("\"water_pump\":\"") + (pumpOn ? "ON" : "OFF") + "\",";
  payload += String("\"grow_light\":\"") + (growLightOn ? "ON" : "OFF") + "\"}";

  HTTPClient http;
  WiFiClientSecure secureClient;   // fresh client every post (reusing one goes stale on ESP32)
  WiFiClient plainClient;
  bool began;

  if (SERVER_USE_HTTPS) {
    secureClient.setInsecure();    // no certificate validation: simplest for a hobby device
    began = http.begin(secureClient, String("https://") + SERVER_HOST + UPDATE_PATH);
  } else {
    began = http.begin(plainClient, String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + UPDATE_PATH);
  }

  if (!began) {
    Serial.println("[SmartGrow] http.begin() failed, check SERVER_HOST/PORT in config.h.");
    return false;
  }

  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  if (strlen(API_KEY) > 0) {
    http.addHeader("X-API-Key", API_KEY);
  }

  int status = http.POST(payload);
  bool ok = (status == 200);

  if (ok) {
    postFailCount = 0;
    Serial.println("[SmartGrow] POST OK (200)");
  } else if (status > 0) {
    postFailCount++;
    Serial.printf("[SmartGrow] POST -> HTTP %d: %s\n", status, http.getString().c_str());
    if (status == 401) Serial.println("[SmartGrow] 401: API_KEY in config.h does not match SMARTGROW_API_KEY on the server.");
  } else {
    postFailCount++;
    Serial.printf("[SmartGrow] POST failed: %s  (RSSI %d dBm, free heap %u)\n",
                  http.errorToString(status).c_str(), WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
  }

  http.end();
  secureClient.stop();
  plainClient.stop();
  return ok;
}

// ---------------------------------------------------------------------------
void printStatus() {
  Serial.println("-------------------------------------------");
  if (dhtConnected) Serial.printf("Temp: %.1fC  Hum: %.0f%%\n", temperatureC, humidityPct);
  else              Serial.println("Temp: OFFLINE  Hum: OFFLINE");

  if (soilConnected)  Serial.printf("Soil: %d%%", soilPct);  else Serial.print("Soil: OFFLINE");
  if (lightConnected) Serial.printf("  Light: %d\n", lightLevel); else Serial.println("  Light: OFFLINE");

  Serial.printf("Pump: %s  WarmLight: %s  Fan: %s%s\n",
                pumpOn ? "ON" : "off", growLightOn ? "ON" : "off", fanOn ? "ON" : "off",
                pumpInCooldown ? "  (pump resting)" : "");
  if (WiFi.status() == WL_CONNECTED) Serial.printf("WiFi: %s  RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else                               Serial.println("WiFi: disconnected");
}
