#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <fauxmoESP.h>

static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

static const uint8_t NUM_RELAYS = 8;
static const uint8_t RELAY_PINS[NUM_RELAYS] = {5, 4, 0, 2, 14, 12, 13, 15};
static const char *RELAY_DEVICE_NAMES[NUM_RELAYS] = {
    "Relay 1", "Relay 2", "Relay 3", "Relay 4",
    "Relay 5", "Relay 6", "Relay 7", "Relay 8"};

static bool relayStates[NUM_RELAYS] = {false};

static const uint8_t MOMENTARY_RELAY = 0;
static const unsigned long MOMENTARY_PULSE_MS = 250;

static bool momentaryPending = false;
static unsigned long momentaryReleaseAt = 0;

static fauxmoESP fauxmo;
static uint8_t relayDeviceIds[NUM_RELAYS] = {0};
static bool alexaReady = false;

static void connectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Verbinde mit WLAN ");
  Serial.print(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();
  Serial.print("WLAN verbunden, IP-Adresse: ");
  Serial.println(WiFi.localIP());
}

static int8_t relayIndexForDevice(unsigned char deviceId, const char *deviceName) {
  for (uint8_t i = 0; i < NUM_RELAYS; ++i) {
    if (relayDeviceIds[i] == deviceId) {
      return i;
    }

    if (deviceName != nullptr && RELAY_DEVICE_NAMES[i] != nullptr) {
      String requested(deviceName);
      if (requested.equalsIgnoreCase(RELAY_DEVICE_NAMES[i])) {
        return i;
      }
    }
  }

  return -1;
}

static void configureAlexaIntegration() {
  fauxmo.createServer(true);
  fauxmo.setPort(80);

  fauxmo.enable(false);

  for (uint8_t i = 0; i < NUM_RELAYS; ++i) {
    relayDeviceIds[i] = fauxmo.addDevice(RELAY_DEVICE_NAMES[i]);
    fauxmo.setState(relayDeviceIds[i], relayStates[i], relayStates[i] ? 255 : 0);
  }

  fauxmo.onSetState([](unsigned char deviceId, const char *deviceName, bool state,
                       unsigned char value) {
    (void)value;
    int8_t relay = relayIndexForDevice(deviceId, deviceName);
    if (relay >= 0) {
      applyRelayState(static_cast<uint8_t>(relay), state);
    }
  });

  alexaReady = true;
  fauxmo.enable(true);
}

void applyRelayState(uint8_t relay, bool on) {
  if (relay >= NUM_RELAYS) {
    return;
  }

  relayStates[relay] = on;
  digitalWrite(RELAY_PINS[relay], on ? HIGH : LOW);

  if (alexaReady) {
    fauxmo.setState(relayDeviceIds[relay], relayStates[relay],
                    relayStates[relay] ? 255 : 0);
  }

  if (relay == MOMENTARY_RELAY) {
    if (on) {
      momentaryPending = true;
      momentaryReleaseAt = millis() + MOMENTARY_PULSE_MS;
    } else {
      momentaryPending = false;
    }
  }
}

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_RELAYS; ++i) {
    pinMode(RELAY_PINS[i], OUTPUT);
    applyRelayState(i, false);
  }

  connectToWifi();
  configureAlexaIntegration();
}

void loop() {
  // Hier würden normalerweise Server-Handler oder andere Aufgaben laufen.

  fauxmo.handle();

  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  if (momentaryPending && millis() >= momentaryReleaseAt) {
    applyRelayState(MOMENTARY_RELAY, false);
  }
}
