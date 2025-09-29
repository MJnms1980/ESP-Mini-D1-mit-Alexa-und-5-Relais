#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

// Relay configuration -------------------------------------------------------
static const uint8_t RELAY_COUNT = 8;
static const uint8_t RELAY_PINS[RELAY_COUNT] = {5, 4, 0, 2, 14, 12, 13, 15}; // D1, D2, D3, D4, D5, D6, D7, D8
static const bool RELAY_ACTIVE_STATE = LOW;     // Adjust to HIGH if your relay board is active-high
static const bool RELAY_INACTIVE_STATE = !RELAY_ACTIVE_STATE;

// Sonoff (eWeLink DIY) configuration ---------------------------------------
static const char *DEVICE_ID = "ESP8266R8RL";  // Must be unique per device
static const char *DEVICE_NAME = "ESP8266 8-Relais";
static const char *DEVICE_APKEY = "12345678";  // Replace with a random 8+ character string

ESP8266WebServer uiServer(80);
ESP8266WebServer ewelinkServer(8081);
WiFiManager wifiManager;

bool relayStates[RELAY_COUNT] = {false};
bool configPortalRunning = false;
bool servicesStarted = false;
uint32_t seqCounter = 1;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;

// Utility functions ---------------------------------------------------------
String stateToString(bool state) {
  return state ? "on" : "off";
}

void applyRelayState(uint8_t index, bool state) {
  if (index >= RELAY_COUNT) {
    return;
  }
  relayStates[index] = state;
  digitalWrite(RELAY_PINS[index], state ? RELAY_ACTIVE_STATE : RELAY_INACTIVE_STATE);
}

void handleRelayUpdate(uint8_t index, bool state) {
  applyRelayState(index, state);
}

void broadcastRelayState() {
  // Placeholder for MQTT or WebSocket integrations if required later.
}

String buildHtmlPage() {
  String html = F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>ESP8266 8 Relais</title>");
  html += F("<style>body{font-family:Arial;margin:0;background:#f7f7f7;color:#333;}header{background:#1f4b99;color:white;padding:1rem;text-align:center;}main{padding:1rem;}ul{list-style:none;padding:0;}li{background:white;margin:.5rem 0;padding:1rem;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}button{float:right;padding:.5rem 1rem;font-size:1rem;border:none;border-radius:4px;cursor:pointer;}button.on{background:#27ae60;color:white;}button.off{background:#c0392b;color:white;}footer{padding:1rem;font-size:.8rem;text-align:center;color:#777;}</style>");
  html += F("</head><body><header><h1>ESP8266 8 Relais</h1></header><main><ul id=\"relays\"></ul></main><footer>OTA aktiviert &middot; Sonoff DIY API &middot; WiFi: ");
  html += WiFi.SSID();
  html += F("</footer><script>const formatter=(state)=>state?\"EIN\":\"AUS\";function render(relays){const list=document.getElementById('relays');list.innerHTML='';relays.forEach((relay,index)=>{const li=document.createElement('li');li.innerHTML=`<span>Relais ${index+1}: <strong>${formatter(relay.state)}</strong></span>`;const btn=document.createElement('button');btn.textContent=relay.state?'AUS':'EIN';btn.className=relay.state?'off':'on';btn.addEventListener('click',()=>toggleRelay(index,!relay.state));li.appendChild(btn);list.appendChild(li);});}
async function loadRelays(){const res=await fetch('/api/relays');if(res.ok){const data=await res.json();render(data.relays);}else{console.error('Fehler beim Laden der Relaisdaten');}}
async function toggleRelay(index,state){const res=await fetch('/api/relays',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index,state})});if(res.ok){loadRelays();}else{alert('Befehl konnte nicht ausgeführt werden.');}}
document.addEventListener('DOMContentLoaded',()=>{loadRelays();setInterval(loadRelays,3000);});</script></body></html>");
  return html;
}

// HTTP UI handlers ----------------------------------------------------------
void handleRoot() {
  uiServer.send(200, "text/html", buildHtmlPage());
}

void handleApiRelaysGet() {
  DynamicJsonDocument doc(512);
  JsonArray arr = doc.createNestedArray("relays");
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["index"] = i;
    obj["state"] = relayStates[i];
  }
  String payload;
  serializeJson(doc, payload);
  uiServer.send(200, "application/json", payload);
}

void handleApiRelaysPost() {
  if (!uiServer.hasArg("plain")) {
    uiServer.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, uiServer.arg("plain"));
  if (err) {
    uiServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  uint8_t index = doc["index"] | 255;
  bool state = doc["state"] | false;
  if (index >= RELAY_COUNT) {
    uiServer.send(422, "application/json", "{\"error\":\"Index außerhalb des Bereichs\"}");
    return;
  }

  handleRelayUpdate(index, state);
  broadcastRelayState();

  DynamicJsonDocument response(128);
  response["index"] = index;
  response["state"] = relayStates[index];
  String payload;
  serializeJson(response, payload);
  uiServer.send(200, "application/json", payload);
}

void handleNotFound() {
  uiServer.send(404, "application/json", "{\"error\":\"Nicht gefunden\"}");
}

// Sonoff DIY (eWeLink) handlers --------------------------------------------
void handleEwelinkInfo() {
  DynamicJsonDocument doc(768);
  doc["seq"] = seqCounter++;
  doc["error"] = 0;
  JsonObject data = doc.createNestedObject("data");
  data["deviceid"] = DEVICE_ID;
  data["name"] = DEVICE_NAME;
  data["apikey"] = DEVICE_APKEY;
  data["swVersion"] = "1.0.0";
  data["model"] = "ESP8266-8CH";
  data["staMac"] = WiFi.macAddress();
  JsonArray switches = data.createNestedArray("switches");
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    JsonObject sw = switches.createNestedObject();
    sw["switch"] = stateToString(relayStates[i]);
    sw["outlet"] = i;
  }

  String payload;
  serializeJson(doc, payload);
  ewelinkServer.send(200, "application/json", payload);
}

bool parseEwelinkPayload(JsonDocument &doc) {
  if (!ewelinkServer.hasArg("plain")) {
    ewelinkServer.send(400, "application/json", "{\"error\":400}");
    return false;
  }

  DeserializationError err = deserializeJson(doc, ewelinkServer.arg("plain"));
  if (err) {
    ewelinkServer.send(400, "application/json", "{\"error\":401}");
    return false;
  }

  const char *deviceId = doc["deviceid"] | "";
  if (strcmp(deviceId, DEVICE_ID) != 0) {
    ewelinkServer.send(401, "application/json", "{\"error\":406}");
    return false;
  }
  return true;
}

void respondEwelinkSuccess() {
  DynamicJsonDocument response(256);
  response["seq"] = seqCounter++;
  response["error"] = 0;
  response["data"] = JsonObject();
  String payload;
  serializeJson(response, payload);
  ewelinkServer.send(200, "application/json", payload);
}

void handleEwelinkSwitch() {
  DynamicJsonDocument doc(512);
  if (!parseEwelinkPayload(doc)) {
    return;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  const char *state = data["switch"] | "";
  uint8_t outlet = data["outlet"] | 255;

  if (outlet >= RELAY_COUNT) {
    ewelinkServer.send(422, "application/json", "{\"error\":422}");
    return;
  }

  bool turnOn = strcmp(state, "on") == 0;
  handleRelayUpdate(outlet, turnOn);
  broadcastRelayState();
  respondEwelinkSuccess();
}

void handleEwelinkSwitches() {
  DynamicJsonDocument doc(768);
  if (!parseEwelinkPayload(doc)) {
    return;
  }

  JsonArray arr = doc["data"]["switches"].as<JsonArray>();
  for (JsonObject sw : arr) {
    uint8_t outlet = sw["outlet"] | 255;
    const char *state = sw["switch"] | "";
    if (outlet < RELAY_COUNT) {
      bool turnOn = strcmp(state, "on") == 0;
      handleRelayUpdate(outlet, turnOn);
    }
  }
  broadcastRelayState();
  respondEwelinkSuccess();
}

// Wi-Fi & OTA helpers -------------------------------------------------------
void startServices() {
  if (servicesStarted) {
    return;
  }

  // Initialize relay pins
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_INACTIVE_STATE);
    relayStates[i] = false;
  }

  // OTA configuration
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.begin();

  // UI server routes
  uiServer.on("/", HTTP_GET, handleRoot);
  uiServer.on("/api/relays", HTTP_GET, handleApiRelaysGet);
  uiServer.on("/api/relays", HTTP_POST, handleApiRelaysPost);
  uiServer.onNotFound(handleNotFound);
  uiServer.begin();

  // eWeLink DIY mode endpoints
  ewelinkServer.on("/zeroconf/info", HTTP_POST, handleEwelinkInfo);
  ewelinkServer.on("/zeroconf/switch", HTTP_POST, handleEwelinkSwitch);
  ewelinkServer.on("/zeroconf/switches", HTTP_POST, handleEwelinkSwitches);
  ewelinkServer.onNotFound([]() {
    ewelinkServer.send(404, "application/json", "{\"error\":404}");
  });
  ewelinkServer.begin();

  servicesStarted = true;
}

void stopServices() {
  if (!servicesStarted) {
    return;
  }
  uiServer.stop();
  ewelinkServer.stop();
  servicesStarted = false;
}

// Arduino lifecycle ---------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("ESP8266 8-Relais Controller"));

  wifiManager.setClass("invert");
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(600);
  wifiManager.setWiFiAutoReconnect(true);

  bool connected = wifiManager.autoConnect("ESP8266-Relais-Setup");
  configPortalRunning = wifiManager.getConfigPortalActive();

  if (connected) {
    Serial.print(F("Verbunden mit: "));
    Serial.println(WiFi.SSID());
    startServices();
  } else {
    Serial.println(F("Starte Konfigurationsportal..."));
  }
}

void loop() {
  if (configPortalRunning) {
    wifiManager.process();
    if (!wifiManager.getConfigPortalActive() && WiFi.status() == WL_CONNECTED) {
      configPortalRunning = false;
      startServices();
    }
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      unsigned long now = millis();
      if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = now;
        Serial.println(F("WLAN getrennt, starte Konfigurationsportal"));
        configPortalRunning = true;
        wifiManager.startConfigPortal("ESP8266-Relais-Setup");
        return;
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    uiServer.handleClient();
    ewelinkServer.handleClient();
  }
}
