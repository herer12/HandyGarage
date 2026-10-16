#include "Webclient.h"
#include <WiFi.h>
#include <HTTPClient.h>

static const uint32_t WIEDERVERBINDEN_MS = 10000;

Webclient::Webclient(const char *ssid, const char *passwort, const char *serverUrl)
    : _ssid(ssid), _passwort(passwort), _url(serverUrl) {}

void Webclient::starten() {
  _aktiv = (_ssid && _ssid[0] != '\0' && _url && _url[0] != '\0');
  if (!_aktiv) return;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(_ssid, _passwort);        // blockiert nicht
  _tLetzterVersuch = millis();
}

void Webclient::aktualisieren() {
  if (!_aktiv || istVerbunden()) return;

  const uint32_t jetzt = millis();
  if (jetzt - _tLetzterVersuch >= WIEDERVERBINDEN_MS) {
    _tLetzterVersuch = jetzt;
    WiFi.disconnect();
    WiFi.begin(_ssid, _passwort);
  }
}

bool Webclient::istVerbunden() const {
  return _aktiv && WiFi.status() == WL_CONNECTED;
}

bool Webclient::senden(const String &json) {
  if (!istVerbunden()) return false;

  HTTPClient http;
  http.setConnectTimeout(1000);
  http.setTimeout(1500);
  if (!http.begin(_url)) return false;

  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(json);
  http.end();
  return code >= 200 && code < 300;
}
