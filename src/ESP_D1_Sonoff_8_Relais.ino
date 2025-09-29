#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ctype.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Relay configuration
// ---------------------------------------------------------------------------
static const uint8_t RELAY_COUNT = 8;
static const uint8_t RELAY_PINS[RELAY_COUNT] = {5, 4, 0, 2, 14, 12, 13, 15}; // D1..D8
static const bool RELAY_ACTIVE_STATE = LOW;   // Change to HIGH if your board is active-high
static const bool RELAY_INACTIVE_STATE = !RELAY_ACTIVE_STATE;

// ---------------------------------------------------------------------------
// Wi-Fi configuration storage
// ---------------------------------------------------------------------------
struct WifiCredentials {
  uint32_t magic;
  char ssid[32];
  char password[64];
};

static const uint32_t WIFI_CONFIG_MAGIC = 0xC0DECAFE;
static WifiCredentials storedCredentials;

// ---------------------------------------------------------------------------
// Sonoff DIY mode configuration
// ---------------------------------------------------------------------------
static const char *DEVICE_ID = "ESP8266R8RL";     // Must be unique per device
static const char *DEVICE_NAME = "ESP8266 8-Relais";
static const char *SOFT_AP_SSID = "ESP8266-Relais-Setup";
static const char *SOFT_AP_PASSWORD = "12345678"; // Set to nullptr for open AP

ESP8266WebServer uiServer(80);
ESP8266WebServer diyServer(8081);

bool relayStates[RELAY_COUNT] = {false};
static const unsigned long MOMENTARY_PULSE_MS = 250;
bool momentaryPending[RELAY_COUNT] = {false};
unsigned long momentaryReleaseAt[RELAY_COUNT] = {0};
bool wifiConnected = false;
bool configMode = false;
uint32_t sonoffSequence = 1;
unsigned long lastConnectAttempt = 0;
const unsigned long CONNECT_RETRY_DELAY = 10000;
bool diyServerActive = false;
bool uiRoutesConfigured = false;
bool diyRoutesConfigured = false;

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------
inline bool isMomentaryRelay(uint8_t index) {
  return index < 6;
}

void applyRelayState(uint8_t index, bool state) {
  if (index >= RELAY_COUNT) {
    return;
  }
  relayStates[index] = state;
  digitalWrite(RELAY_PINS[index], state ? RELAY_ACTIVE_STATE : RELAY_INACTIVE_STATE);
  if (isMomentaryRelay(index)) {
    if (state) {
      momentaryPending[index] = true;
      momentaryReleaseAt[index] = millis() + MOMENTARY_PULSE_MS;
    } else {
      momentaryPending[index] = false;
      momentaryReleaseAt[index] = 0;
    }
  }
}

void serviceMomentaryRelays() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (!isMomentaryRelay(i)) {
      continue;
    }
    if (momentaryPending[i] && (long)(now - momentaryReleaseAt[i]) >= 0) {
      applyRelayState(i, false);
    }
  }
}

void turnAllRelaysOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    applyRelayState(i, false);
  }
}

void loadStoredCredentials() {
  EEPROM.begin(sizeof(WifiCredentials));
  EEPROM.get(0, storedCredentials);
  if (storedCredentials.magic != WIFI_CONFIG_MAGIC) {
    memset(&storedCredentials, 0, sizeof(storedCredentials));
  }
}

void saveCredentials(const String &ssid, const String &password) {
  memset(&storedCredentials, 0, sizeof(storedCredentials));
  storedCredentials.magic = WIFI_CONFIG_MAGIC;
  ssid.toCharArray(storedCredentials.ssid, sizeof(storedCredentials.ssid));
  password.toCharArray(storedCredentials.password, sizeof(storedCredentials.password));
  EEPROM.put(0, storedCredentials);
  EEPROM.commit();
}

bool haveStoredCredentials() {
  return storedCredentials.magic == WIFI_CONFIG_MAGIC && storedCredentials.ssid[0] != '\0';
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

String buildStatusJson() {
  String json = "{\"relays\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += "{\"index\":";
    json += i;
    json += ",\"state\":";
    json += relayStates[i] ? "true" : "false";
    json += '}';
  }
  json += "],\"wifi\":{\"connected\":";
  json += wifiConnected ? "true" : "false";
  json += ",\"ssid\":\"";
  json += wifiConnected ? jsonEscape(WiFi.SSID()) : "";
  json += "\"}}";
  return json;
}

String buildConfigPage() {
  String html = F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>ESP8266 Relais Setup</title>");
  html += F("<style>body{font-family:Arial;margin:0;background:#f0f2f5;}header{background:#003366;color:#fff;padding:1.5rem;text-align:center;}main{padding:1.5rem;}form{max-width:420px;margin:0 auto;background:#fff;padding:1.5rem;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.15);}label{display:block;margin-bottom:.5rem;font-weight:600;}input{width:100%;padding:.75rem;margin-bottom:1rem;border:1px solid #ccc;border-radius:4px;font-size:1rem;}button{width:100%;padding:.85rem;font-size:1.1rem;background:#0066cc;color:#fff;border:none;border-radius:4px;cursor:pointer;}button:hover{background:#004c99;}p{color:#555;text-align:center;}</style>");
  html += F("</head><body><header><h1>WLAN Einrichtung</h1></header><main><p>Verbinde dich mit dem Netzwerk <strong>");
  html += SOFT_AP_SSID;
  html += F("</strong> und gib die Zugangsdaten deines WLANs ein.</p><form id=\"cfg\"><label>SSID<input name=\"ssid\" maxlength=\"31\" required></label><label>Passwort<input name=\"password\" type=\"password\" maxlength=\"63\"></label><button type=\"submit\">Speichern &amp; Neustarten</button></form><p id=\"msg\"></p></main><script>const form=document.getElementById('cfg');const msg=document.getElementById('msg');form.addEventListener('submit',async(e)=>{e.preventDefault();msg.textContent='Speichert...';const data=new URLSearchParams(new FormData(form));const res=await fetch('/api/config',{method:'POST',body:data});if(res.ok){msg.textContent='Neustart in wenigen Sekunden...';}else{msg.textContent='Speichern fehlgeschlagen';}});</script></body></html>");
  return html;
}

String buildControlPage() {
  String html = F("<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>ESP8266 8 Relais</title>");
  html += F("<style>body{font-family:Arial;margin:0;background:#f7f7f7;color:#333;}header{background:#1f4b99;color:#fff;padding:1.5rem;text-align:center;}main{padding:1.5rem;}ul{list-style:none;padding:0;margin:0;}li{display:flex;justify-content:space-between;align-items:center;background:#fff;margin-bottom:1rem;padding:1rem 1.25rem;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.12);}button{min-width:90px;padding:.6rem 1rem;font-size:1rem;border:none;border-radius:6px;cursor:pointer;transition:all .2s;}button.on{background:#27ae60;color:#fff;}button.off{background:#c0392b;color:#fff;}footer{padding:1rem;text-align:center;font-size:.85rem;color:#666;}</style>");
  html += F("</head><body><header><h1>ESP8266 Relaissteuerung</h1></header><main><ul id=\"relays\"></ul></main><footer>Verbunden mit: ");
  html += jsonEscape(WiFi.SSID());
  html += F("</footer><script>const relaysEl=document.getElementById('relays');const render=(list)=>{relaysEl.innerHTML='';list.forEach((item)=>{const li=document.createElement('li');li.innerHTML=`<span>Relais ${item.index+1}: <strong>${item.state?'EIN':'AUS'}</strong></span>`;const btn=document.createElement('button');btn.textContent=item.state?'AUS':'EIN';btn.className=item.state?'off':'on';btn.addEventListener('click',()=>toggle(item.index,!item.state));li.appendChild(btn);relaysEl.appendChild(li);});};const load=async()=>{const res=await fetch('/api/relays');if(res.ok){const data=await res.json();render(data.relays);}};const toggle=async(index,state)=>{const body=new URLSearchParams({index:String(index),state:state?'1':'0'});const res=await fetch('/api/relays',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(res.ok){load();}else{alert('Schalten fehlgeschlagen');}};load();setInterval(load,4000);</script></body></html>");
  return html;
}

void sendJsonResponse(ESP8266WebServer &server, const String &json, int status = 200) {
  server.send(status, "application/json", json);
}

void handleRoot() {
  if (configMode) {
    uiServer.send(200, "text/html", buildConfigPage());
  } else {
    uiServer.send(200, "text/html", buildControlPage());
  }
}

void handleApiRelaysGet() {
  if (configMode) {
    sendJsonResponse(uiServer, "{\"error\":\"config_mode\"}", 403);
    return;
  }
  sendJsonResponse(uiServer, buildStatusJson());
}

void handleApiRelaysPost() {
  if (configMode) {
    sendJsonResponse(uiServer, "{\"error\":\"config_mode\"}", 403);
    return;
  }
  if (!uiServer.hasArg("index") || !uiServer.hasArg("state")) {
    sendJsonResponse(uiServer, "{\"error\":\"missing_parameters\"}", 400);
    return;
  }
  uint8_t index = uiServer.arg("index").toInt();
  bool state = uiServer.arg("state") == "1" || uiServer.arg("state").equalsIgnoreCase("true");
  if (index >= RELAY_COUNT) {
    sendJsonResponse(uiServer, "{\"error\":\"invalid_index\"}", 422);
    return;
  }
  applyRelayState(index, state);
  sendJsonResponse(uiServer, buildStatusJson());
}

void handleApiConfigPost() {
  if (!configMode) {
    sendJsonResponse(uiServer, "{\"error\":\"not_in_config_mode\"}", 403);
    return;
  }
  if (!uiServer.hasArg("ssid")) {
    sendJsonResponse(uiServer, "{\"error\":\"missing_ssid\"}", 400);
    return;
  }
  String ssid = uiServer.arg("ssid");
  String password = uiServer.arg("password");
  ssid.trim();
  password.trim();
  if (ssid.length() == 0) {
    sendJsonResponse(uiServer, "{\"error\":\"empty_ssid\"}", 400);
    return;
  }
  saveCredentials(ssid, password);
  sendJsonResponse(uiServer, "{\"status\":\"saved\"}");
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if (configMode) {
    uiServer.send(404, "text/plain", "Nicht gefunden");
  } else {
    uiServer.send(404, "application/json", "{\"error\":\"not_found\"}");
  }
}

// ---------------------------------------------------------------------------
// Minimal JSON parsing helpers for Sonoff DIY API
// ---------------------------------------------------------------------------
String getJsonValue(const String &body, const char *key) {
  String needle = String('\"') + key + '\"';
  int keyPos = body.indexOf(needle);
  if (keyPos < 0) {
    return String();
  }
  int colon = body.indexOf(':', keyPos + needle.length());
  if (colon < 0) {
    return String();
  }
  int start = colon + 1;
  while (start < (int)body.length() && isspace(body[start])) {
    start++;
  }
  if (start >= (int)body.length()) {
    return String();
  }
  if (body[start] == '\"') {
    int end = body.indexOf('"', start + 1);
    if (end < 0) {
      return String();
    }
    return body.substring(start + 1, end);
  }
  int end = start;
  while (end < (int)body.length() && body[end] != ',' && body[end] != '}' && body[end] != ']') {
    end++;
  }
  String value = body.substring(start, end);
  value.trim();
  return value;
}

int getJsonInt(const String &body, const char *key, int defaultValue = -1) {
  String value = getJsonValue(body, key);
  if (value.length() == 0) {
    return defaultValue;
  }
  if (value[0] == '\"') {
    value.remove(0, 1);
  }
  return value.toInt();
}

bool parseSonoffDevice(const String &body) {
  String deviceId = getJsonValue(body, "deviceid");
  return deviceId == DEVICE_ID;
}

String sonoffStateString(bool state) {
  return state ? "on" : "off";
}

String buildSonoffInfo() {
  String json = "{\"seq\":";
  json += sonoffSequence++;
  json += ",\"error\":0,\"data\":{\"deviceid\":\"";
  json += DEVICE_ID;
  json += "\",\"name\":\"";
  json += DEVICE_NAME;
  json += "\",\"switches\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += "{\"switch\":\"";
    json += sonoffStateString(relayStates[i]);
    json += "\",\"outlet\":";
    json += i;
    json += '}';
  }
  json += "],\"swVersion\":\"1.0.0\",\"devicekey\":\"";
  json += DEVICE_ID;
  json += "\"}}";
  return json;
}

void sendSonoffOk() {
  String json = "{\"seq\":";
  json += sonoffSequence++;
  json += ",\"error\":0}";
  sendJsonResponse(diyServer, json);
}

void handleSonoffInfo() {
  if (!parseSonoffDevice(diyServer.arg("plain"))) {
    sendJsonResponse(diyServer, "{\"error\":406}", 401);
    return;
  }
  sendJsonResponse(diyServer, buildSonoffInfo());
}

void handleSonoffSwitch() {
  String body = diyServer.arg("plain");
  if (!parseSonoffDevice(body)) {
    sendJsonResponse(diyServer, "{\"error\":406}", 401);
    return;
  }
  String state = getJsonValue(body, "switch");
  int outlet = getJsonInt(body, "outlet", -1);
  if (outlet < 0 || outlet >= RELAY_COUNT) {
    sendJsonResponse(diyServer, "{\"error\":422}", 422);
    return;
  }
  bool turnOn = state == "on";
  applyRelayState(outlet, turnOn);
  sendSonoffOk();
}

void handleSonoffSwitches() {
  String body = diyServer.arg("plain");
  if (!parseSonoffDevice(body)) {
    sendJsonResponse(diyServer, "{\"error\":406}", 401);
    return;
  }
  int arrayPos = body.indexOf("\"switches\"");
  int arrayEnd = arrayPos >= 0 ? body.indexOf(']', arrayPos) : -1;
  if (arrayPos >= 0 && arrayEnd < 0) {
    arrayEnd = body.length();
  }
  int objPos = arrayPos >= 0 ? body.indexOf('{', arrayPos) : -1;
  while (objPos >= 0 && (arrayEnd < 0 || objPos < arrayEnd)) {
    int objEnd = body.indexOf('}', objPos);
    if (objEnd < 0) {
      break;
    }
    String item = body.substring(objPos, objEnd + 1);
    int outlet = getJsonInt(item, "outlet", -1);
    String state = getJsonValue(item, "switch");
    if (outlet >= 0 && outlet < RELAY_COUNT) {
      bool turnOn = state == "on";
      applyRelayState(outlet, turnOn);
    }
    objPos = body.indexOf('{', objEnd + 1);
  }
  sendSonoffOk();
}

// ---------------------------------------------------------------------------
// Wi-Fi and server management
// ---------------------------------------------------------------------------
void configureUiRoutes() {
  uiServer.stop();
  if (!uiRoutesConfigured) {
    uiServer.on("/", HTTP_GET, handleRoot);
    uiServer.on("/api/relays", HTTP_GET, handleApiRelaysGet);
    uiServer.on("/api/relays", HTTP_POST, handleApiRelaysPost);
    uiServer.on("/api/config", HTTP_POST, handleApiConfigPost);
    uiServer.onNotFound(handleNotFound);
    uiRoutesConfigured = true;
  }
  uiServer.begin();
}

void configureSonoffRoutes() {
  diyServer.stop();
  if (!diyRoutesConfigured) {
    diyServer.on("/zeroconf/info", HTTP_POST, handleSonoffInfo);
    diyServer.on("/zeroconf/switch", HTTP_POST, handleSonoffSwitch);
    diyServer.on("/zeroconf/switches", HTTP_POST, handleSonoffSwitches);
    diyServer.onNotFound([]() {
      sendJsonResponse(diyServer, "{\"error\":404}", 404);
    });
    diyRoutesConfigured = true;
  }
  diyServer.begin();
  diyServerActive = true;
}

void startConfigPortal() {
  configMode = true;
  wifiConnected = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  if (SOFT_AP_PASSWORD && strlen(SOFT_AP_PASSWORD) >= 8) {
    WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD);
  } else {
    WiFi.softAP(SOFT_AP_SSID);
  }
  if (diyServerActive) {
    diyServer.stop();
    diyServerActive = false;
  }
  MDNS.end();
  configureUiRoutes();
  lastConnectAttempt = millis();
}

bool attemptWifiConnection() {
  if (!haveStoredCredentials()) {
    return false;
  }
  wifiConnected = false;
  WiFi.mode(configMode ? WIFI_AP_STA : WIFI_STA);
  if (configMode) {
    if (SOFT_AP_PASSWORD && strlen(SOFT_AP_PASSWORD) >= 8) {
      WiFi.softAP(SOFT_AP_SSID, SOFT_AP_PASSWORD);
    } else {
      WiFi.softAP(SOFT_AP_SSID);
    }
  }
  WiFi.begin(storedCredentials.ssid, storedCredentials.password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configMode = false;
    return true;
  }
  return false;
}

void beginNormalMode() {
  configMode = false;
  wifiConnected = true;
  configureUiRoutes();
  configureSonoffRoutes();
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.begin();
  if (MDNS.begin(DEVICE_NAME)) {
    MDNS.addService("http", "tcp", 80);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("ESP8266 8-Relais Controller"));

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_INACTIVE_STATE);
  }
  turnAllRelaysOff();

  loadStoredCredentials();
  configureUiRoutes();

  if (attemptWifiConnection()) {
    Serial.print(F("Verbunden mit: "));
    Serial.println(WiFi.SSID());
    beginNormalMode();
  } else {
    Serial.println(F("Starte Konfigurationsportal..."));
    startConfigPortal();
  }
}

void loop() {
  serviceMomentaryRelays();
  if (configMode) {
    uiServer.handleClient();
    if (haveStoredCredentials()) {
      unsigned long now = millis();
      if (now - lastConnectAttempt >= CONNECT_RETRY_DELAY) {
        lastConnectAttempt = now;
        if (attemptWifiConnection()) {
          Serial.print(F("Verbunden mit: "));
          Serial.println(WiFi.SSID());
          beginNormalMode();
        }
      }
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println(F("Verbindung verloren. Wechsel in Konfigurationsmodus."));
      startConfigPortal();
      return;
    }
    unsigned long now = millis();
    if (now - lastConnectAttempt >= CONNECT_RETRY_DELAY) {
      lastConnectAttempt = now;
      if (attemptWifiConnection()) {
        Serial.print(F("Verbunden mit: "));
        Serial.println(WiFi.SSID());
        beginNormalMode();
      }
    }
  } else {
    ArduinoOTA.handle();
    uiServer.handleClient();
    diyServer.handleClient();
  }
}
