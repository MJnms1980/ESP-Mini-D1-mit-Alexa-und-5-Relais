#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <fauxmoESP.h>

// ----------- Konfiguration ------------
constexpr char OTA_HOSTNAME[] = "jalousie-controller";
constexpr char CONFIG_AP_NAME[] = "Jalousie-Konfigurator";

constexpr uint8_t RELAY_PINS[] = {D1, D2, D3, D4, D5};
constexpr size_t RELAY_COUNT = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);

// Die Relais vieler Boards sind aktiv LOW. Falls Ihre Relais aktiv HIGH sind,
// bitte RELAY_ACTIVE_STATE auf HIGH ändern.
constexpr uint8_t RELAY_ACTIVE_STATE = LOW;
constexpr uint8_t RELAY_INACTIVE_STATE =
    (RELAY_ACTIVE_STATE == LOW) ? HIGH : LOW;

constexpr uint32_t RELAY_PULSE_DURATION_MS = 1000UL;  // 1 Sekunde

const char *const ALEXA_DEVICE_NAMES[RELAY_COUNT] = {
    "Jalousie eins runter",
    "Jalousie eins hoch",
    "Jalousie zwei runter",
    "Jalousie zwei hoch",
    "Jalousie stopp"};

struct RelayTask {
  bool active = false;
  unsigned long startMillis = 0;
};

RelayTask relayTasks[RELAY_COUNT];

fauxmoESP fauxmo;
bool servicesInitialised = false;

// Forward-Deklarationen
void triggerRelay(size_t index);
void handleRelayTasks();
void configureRelays();
void setupAlexaIntegration();
void setupOTA();
void beginNetworkServices();

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("ESP Mini D1 Jalousie-Steuerung startet"));

  configureRelays();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  WiFiManager wifiManager;
  wifiManager.setClass("invert");  // Dunkles Design für das Portal
  wifiManager.setConfigPortalTimeout(0);  // kein automatischer Timeout
  wifiManager.setWiFiAutoReconnect(true);

  if (wifiManager.autoConnect(CONFIG_AP_NAME)) {
    Serial.print(F("Mit WLAN verbunden: "));
    Serial.println(WiFi.SSID());
  } else {
    Serial.println(F("Kein WLAN verbunden. Konfigurationsportal bleibt aktiv."));
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!servicesInitialised) {
      beginNetworkServices();
    }
    ArduinoOTA.handle();
    fauxmo.handle();
  }

  handleRelayTasks();
}

void configureRelays() {
  for (size_t i = 0; i < RELAY_COUNT; ++i) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_INACTIVE_STATE);
    relayTasks[i].active = false;
  }
}

void beginNetworkServices() {
  setupOTA();
  setupAlexaIntegration();
  servicesInitialised = true;
}

void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA Update startet"));
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("OTA Update abgeschlossen"));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print(F("OTA Fehler: "));
    Serial.println(error);
  });

  ArduinoOTA.begin();
  Serial.println(F("OTA bereit"));
}

void setupAlexaIntegration() {
  fauxmo.createServer(true);
  fauxmo.setPort(80);  // Standard-Port für Alexa/Wemo
  fauxmo.enable(true);

  fauxmo.onSetState([](unsigned char device_id, const char *device_name,
                       bool state, unsigned char value) {
    Serial.print(F("Alexa-Befehl: "));
    Serial.print(device_name);
    Serial.print(F(" -> "));
    Serial.println(state ? F("EIN") : F("AUS"));

    if (state && device_id < RELAY_COUNT) {
      triggerRelay(device_id);
    }
  });

  for (size_t i = 0; i < RELAY_COUNT; ++i) {
    uint8_t id = fauxmo.addDevice(ALEXA_DEVICE_NAMES[i]);
    Serial.print(F("Alexa-Gerät registriert: "));
    Serial.print(ALEXA_DEVICE_NAMES[i]);
    Serial.print(F(" (ID: "));
    Serial.print(id);
    Serial.println(F(")"));
  }

  Serial.println(F("Alexa-Integration bereit"));
}

void triggerRelay(size_t index) {
  if (index >= RELAY_COUNT) {
    return;
  }

  RelayTask &task = relayTasks[index];
  task.active = true;
  task.startMillis = millis();
  digitalWrite(RELAY_PINS[index], RELAY_ACTIVE_STATE);

  Serial.print(F("Relais "));
  Serial.print(index + 1);
  Serial.println(F(" aktiviert"));
}

void handleRelayTasks() {
  const unsigned long now = millis();

  for (size_t i = 0; i < RELAY_COUNT; ++i) {
    RelayTask &task = relayTasks[i];
    if (!task.active) {
      continue;
    }

    if (now - task.startMillis >= RELAY_PULSE_DURATION_MS) {
      digitalWrite(RELAY_PINS[i], RELAY_INACTIVE_STATE);
      task.active = false;
      Serial.print(F("Relais "));
      Serial.print(i + 1);
      Serial.println(F(" deaktiviert"));
    }
  }
}
