# ESP8266 D1 8-Relais Steuerung

Dieses Projekt stellt einen vollständigen Arduino-Sketch bereit, um ein ESP8266-12F (z. B. Wemos D1 mini oder NodeMCU) mit einem 8-Kanal-Relaisboard zu betreiben. Neben einer lokalen Weboberfläche unterstützt der Sketch:

- Automatisches WLAN-Provisioning per Captive Portal (WiFiManager), wenn keine Zugangsdaten vorhanden sind oder die Verbindung fehlschlägt
- Steuerung über die Sonoff-/eWeLink-App im lokalen DIY-Modus (HTTP-API auf Port 8081)
- OTA-Firmwareupdates direkt über das WLAN (ArduinoOTA)
- Eine einfache Weboberfläche zur manuellen Bedienung sämtlicher Relais und zur Statusanzeige

## Voraussetzungen

1. **Arduino IDE** in aktueller Version mit installiertem ESP8266 Core.
2. Die folgenden Bibliotheken müssen über den Bibliotheksverwalter installiert werden:
   - [WiFiManager](https://github.com/tzapu/WiFiManager)
   - [ArduinoJson](https://arduinojson.org/) (Version 6 oder neuer)
3. Optional: Ein sicheres OTA-Passwort kann in `ArduinoOTA.setPasswordHash()` gesetzt werden.

## Pinbelegung

Die Standardbelegung nutzt die folgenden GPIOs (entsprechen bei vielen Boards den Pins D1–D8):

| Relais | GPIO | Board-Pin |
| ------ | ---- | --------- |
| 1      | 5    | D1        |
| 2      | 4    | D2        |
| 3      | 0    | D3        |
| 4      | 2    | D4        |
| 5      | 14   | D5        |
| 6      | 12   | D6        |
| 7      | 13   | D7        |
| 8      | 15   | D8        |

Falls Ihr Relais-Board aktiv-high arbeitet, kann `RELAY_ACTIVE_STATE` im Sketch auf `HIGH` geändert werden.

## Verwendung

1. Öffne die Datei `src/ESP_D1_Sonoff_8_Relais.ino` in der Arduino IDE.
2. Wähle als Board „NodeMCU 1.0 (ESP-12E Module)“ oder ein passendes ESP8266-12F Profil.
3. Kompiliere und lade den Sketch.
4. Beim ersten Start (oder wenn das WLAN nicht erreichbar ist) öffnet der Controller einen Access Point `ESP8266-Relais-Setup`. Verbinde dich und trage deine WLAN-Zugangsdaten ein.
5. Nach erfolgreicher Verbindung stehen folgende Funktionen zur Verfügung:
   - **Weboberfläche:** `http://<IP-Adresse>` → Übersicht und Schalten aller Relais.
   - **Sonoff DIY API:** Die eWeLink-App findet das Gerät im lokalen Modus. Device-ID und API-Key können im Sketch angepasst werden.
   - **OTA Update:** In der Arduino IDE „Sketch → Upload via Netzwerk“ auswählen, sobald das Gerät im gleichen Netzwerk ist.

## Anpassungen

- `DEVICE_ID`, `DEVICE_NAME` und `DEVICE_APKEY` sollten pro Gerät individuell gesetzt werden.
- Die Anzahl der Relais lässt sich über `RELAY_COUNT` und das `RELAY_PINS`-Array erweitern oder verkleinern.
- Für zusätzliche Integrationen (MQTT, Home Assistant, …) kann die Funktion `broadcastRelayState()` erweitert werden.

## Sicherheitshinweise

- Verwende ein separates, sicheres Netz oder aktiviere ein OTA-Passwort, wenn das Gerät produktiv eingesetzt wird.
- Die Sonoff DIY API erlaubt lokale Steuerung ohne zusätzliche Authentifizierung. Setze ggf. Firewall-Regeln, um unbefugte Zugriffe zu verhindern.
