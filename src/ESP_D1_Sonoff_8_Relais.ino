#include <Arduino.h>

String buildHtmlPage() {
  String html;
  html.reserve(2048);
  html += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">");
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += F("<title>ESP D1 Sonoff 8 Relais</title>");
  html += F("<style>body{font-family:Arial,sans-serif;background-color:#f7f7f7;margin:0;padding:20px;}\n");
  html += F("h1{margin-top:0;}\n");
  html += F("button{margin:6px;padding:12px 20px;font-size:16px;}\n");
  html += F("#status{margin-top:20px;}</style>");
  html += F("</head><body>");
  html += F("<h1>ESP D1 Sonoff 8 Relais</h1>");
  html += F("<div class=\"relays\">");
  for (uint8_t relayIndex = 1; relayIndex <= 8; ++relayIndex) {
    html += F("<button class=\"relay-button\" data-relay=\"");
    html += String(relayIndex);
    html += F("\">Relay ");
    html += String(relayIndex);
    html += F("</button>");
  }
  html += F("</div>");
  html += F("<div id=\"status\"></div>");
  html += F("<script>");
  html += F("function sendRelayCommand(relay) {");
  html += F("  var xhr = new XMLHttpRequest();");
  html += F("  xhr.open('GET', '/relay?index=' + relay, true);");
  html += F("  xhr.onload = function () {");
  html += F("    if (xhr.status === 200) {");
  html += F("      document.getElementById('status').textContent = xhr.responseText;");
  html += F("    } else {");
  html += F("      document.getElementById('status').textContent = 'Error sending command';");
  html += F("    }");
  html += F("  };");
  html += F("  xhr.send();");
  html += F("}");
  html += F("document.querySelectorAll('.relay-button').forEach(function (button) {");
  html += F("  button.addEventListener('click', function () {");
  html += F("    var relay = this.getAttribute('data-relay');");
  html += F("    sendRelayCommand(relay);");
  html += F("  });");
  html += F("});");
  html += F("</script></body></html>");
  return html;
}
