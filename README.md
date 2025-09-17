# ESP Mini D1 mit Alexa und fünf Relais

Dieses Projekt stellt einen vollständigen Arduino-Sketch bereit, der einen
Wemos/ESP8266 Mini D1 mit fünf Relais über Amazon Alexa steuern kann. Jeder
Relaisausgang arbeitet als Taster und wird für eine Sekunde aktiviert, um
Rollladenantriebe oder ähnliche Steuerungen zu bedienen.

## Funktionsumfang

- **Alexa-Steuerung:** Die fünf Relais erscheinen als einzelne Geräte
  (Wemo-kompatibel) in Alexa und lassen sich bequem per Sprachbefehl schalten.
- **Tasterbetrieb:** Jeder Befehl aktiviert das zugehörige Relais für genau eine
  Sekunde, ohne `delay()`-Aufrufe zu verwenden.
- **WLAN-Konfiguration per Webportal:** Wenn das Gerät keine gespeicherten
  WLAN-Zugangsdaten findet oder keine Verbindung herstellen kann, startet es
  einen eigenen Access Point (`Jalousie-Konfigurator`) mit einem
  Konfigurationsportal.
- **OTA-Updates:** Firmware-Aktualisierungen lassen sich komfortabel über das
  Netzwerk durchführen, ohne ein USB-Kabel anschließen zu müssen.

## Hardware-Zuordnung

| Relais | Funktion                                      | ESP8266-Pin |
|--------|-----------------------------------------------|-------------|
| 1      | Jalousie 1 schließt                           | D1          |
| 2      | Jalousie 1 öffnet                             | D2          |
| 3      | Jalousie 2 schließt                           | D3          |
| 4      | Jalousie 2 öffnet                             | D4          |
| 5      | Stop für Jalousie 1 und 2                     | D5          |

> Hinweis: Viele Relais-Boards arbeiten aktiv **LOW**. Falls Ihr Modul aktiv
> HIGH geschaltet wird, kann dies im Sketch mit der Konstante
> `RELAY_ACTIVE_STATE` angepasst werden.

## Voraussetzungen

Installiere vor dem Kompilieren in der Arduino IDE folgende Bibliotheken über
den Bibliotheksverwalter oder als ZIP-Download:

- **fauxmoESP** – stellt die Wemo-Emulation für Alexa bereit.
- **WiFiManager** – erzeugt das Konfigurationsportal für WLAN-Zugangsdaten.

Der Sketch befindet sich in der Datei
[`ESP-Mini-D1-mit-Alexa-und-5-Relais.ino`](ESP-Mini-D1-mit-Alexa-und-5-Relais.ino).
Öffne den Ordner in der Arduino IDE, wähle als Board „LOLIN(WEMOS) D1 R2 & mini“
oder ein kompatibles ESP8266-Board und lade die Firmware hoch.

## OTA und Alexa-Ersteinrichtung

1. **WLAN einrichten:** Nach dem ersten Start verbindet sich der ESP mit dem
   Access Point `Jalousie-Konfigurator`. Verbinde dich mit diesem Netzwerk und
   öffne die Konfigurationsseite (normalerweise `http://192.168.4.1/`), um deine
   WLAN-Zugangsdaten zu hinterlegen.
2. **Alexa-Geräte suchen:** Sobald der Controller im Heimnetz eingebunden ist,
   lasse Alexa nach neuen Geräten suchen. Es erscheinen fünf Schalter mit den
   Namen „Jalousie eins runter/hoch“, „Jalousie zwei runter/hoch“ sowie
   „Jalousie stopp“.
3. **OTA verwenden:** Für zukünftige Firmware-Updates genügt es, in der Arduino
   IDE unter „Werkzeuge → Port“ den neuen Netzwerkport (Hostname
   `jalousie-controller`) auszuwählen.

Viel Erfolg beim Automatisieren deiner Rollläden!
