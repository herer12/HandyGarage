#pragma once
#include <Arduino.h>

// WLAN-Verbindung halten und JSON per HTTP-POST an den Server schicken.
// Sind SSID oder URL leer, ist der Webclient inaktiv (kein Fehler).
class Webclient {
 public:
  Webclient(const char *ssid, const char *passwort, const char *serverUrl);

  void starten();
  void aktualisieren();                 // zyklisch: Verbindung überwachen

  bool istAktiv() const      { return _aktiv; }
  bool istVerbunden() const;

  // Blockiert höchstens ca. 1,5 s. Rückgabe: Server hat mit 2xx geantwortet.
  bool senden(const String &json);

 private:
  const char *_ssid;
  const char *_passwort;
  const char *_url;
  bool _aktiv = false;
  uint32_t _tLetzterVersuch = 0;
};
