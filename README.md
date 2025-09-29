# ESP8266 D1 8-Relais Steuerung

Dieser Sketch richtet ein ESP8266-12F (z. B. Wemos D1 mini oder NodeMCU) für den Betrieb mit einem 8-Kanal-Relaisboard ein. Das Gerät bietet:

- Eine lokale Weboberfläche zum Schalten aller acht Relais (Relais 1–6 als Taster, 7–8 als Schalter)
- Einen integrierten Einrichtungsmodus mit eigenem Access Point, falls kein WLAN gespeichert ist oder die Verbindung scheitert
- Kompatibilität zur Sonoff-/eWeLink-App über die lokale DIY-HTTP-API (Port 8081)
- OTA-Firmwareupdates über das WLAN (ArduinoOTA)

## Voraussetzungen

1. **Arduino IDE** in aktueller Version mit installiertem ESP8266 Core (Boardverwalter).
2. Keine zusätzlichen Bibliotheken notwendig – alle verwendeten Header stammen aus dem ESP8266 Core.
3. Optional: OTA-Uploads können mit einem Passwort versehen werden (siehe Hinweis im Sketch).

## Pinbelegung

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

Falls das verwendete Relaisboard aktiv-high arbeitet, setze `RELAY_ACTIVE_STATE` im Sketch auf `HIGH`.

## Verwendung

1. Öffne `src/ESP_D1_Sonoff_8_Relais.ino` in der Arduino IDE und wähle ein passendes ESP8266-Boardprofil (z. B. „NodeMCU 1.0“).
2. Kompiliere und lade den Sketch auf das Board.
3. Beim ersten Start oder wenn das konfigurierte WLAN nicht erreichbar ist, startet der Controller einen Access Point `ESP8266-Relais-Setup` (Standardpasswort `12345678`).
4. Verbinde dich mit dem Access Point und rufe `http://192.168.4.1` auf. Trage dort SSID und Passwort deines WLANs ein. Das Gerät startet danach neu und versucht sich zu verbinden.
5. Im normalen Betrieb stehen folgende Funktionen bereit:
   - **Weboberfläche:** `http://<IP-Adresse>` → Status und manuelle Steuerung der Relais.
   - **Sonoff DIY API:** Die eWeLink-App kann das Gerät im lokalen Modus ansprechen. Passe bei Bedarf `DEVICE_ID` und `DEVICE_NAME` an.
   - **OTA Update:** In der Arduino IDE „Sketch → Upload über Netzwerk“ wählen, sobald das Gerät erreichbar ist.

## Anpassungen

- Die gespeicherten WLAN-Zugangsdaten werden im Flash (EEPROM-Emulation) abgelegt. Ein erneutes Öffnen des Konfigurationsportals ist möglich, indem das WLAN vorübergehend deaktiviert wird.
- `RELAY_PINS` kann für andere Boards angepasst werden. Die Anzahl der Relais ergibt sich aus `RELAY_COUNT`.
- Die Impulsdauer der Taster (Relais 1–6) legst du über `MOMENTARY_PULSE_MS` im Sketch fest.
- Die Sonoff-Antworten sind bewusst einfach gehalten. Bei Bedarf lassen sich zusätzliche Felder ergänzen.
- Setze bei produktivem Einsatz ein OTA-Passwort (`ArduinoOTA.setPassword()` oder `setPasswordHash()` im Sketch aktivieren) und sichere den Zugriff auf den DIY-Port 8081.

## Fehlerbehebung

- **Keine Verbindung im Normalmodus:** Prüfe, ob die hinterlegten Zugangsdaten korrekt sind. Das Gerät fällt nach einigen Sekunden automatisch wieder in den Einrichtungsmodus zurück.
- **Sonoff-App findet das Gerät nicht:** Stelle sicher, dass die App und der Controller im selben Netzwerk sind und dass Port 8081 nicht blockiert wird.
- **OTA-Upload schlägt fehl:** Stelle sicher, dass sich Board und Rechner im selben Subnetz befinden und dass keine Firewall den mDNS-Dienst (`_arduino._tcp`) blockiert.
