#pragma once
#include <Arduino.h>
#include "Strommessung.h"
#include "Konfig.h"

// Zustand eines Stellplatzes
enum class Zustand : uint8_t { FEHLER, FREI, BELEGT };

// Was bei der letzten Messung passiert ist (fuer Anzeige und Webclient)
enum class Ereignis : uint8_t { KEINES, ANGESTECKT, ABGEZOGEN, SENSOR_AUS, SENSOR_WIEDER };

// Durchschnitt der Messungen der letzten 5 Sekunden
struct Mittelwert {
  bool    gueltig    = false;
  uint8_t anzahl     = 0;        // wie viele Messungen eingeflossen sind
  float   spannung_V = 0.0f;
  float   strom_mA   = 0.0f;
  float   leistung_W = 0.0f;
  uint32_t zeit_ms   = 0;        // millis() zum Zeitpunkt der Bildung (0 = noch nie gebildet)
};

// Meldung alle 60 Sekunden (letzter Mittelwert + Zustand + Steckdauer)
struct Meldung {
  uint8_t    platz       = 0;    // 1..ANZAHL_PLAETZE
  Zustand    zustand     = Zustand::FEHLER;
  Mittelwert werte;
  uint32_t   steckdauer_s = 0;   // laufende Dauer, bei "frei" die letzte Dauer
  uint32_t   zeit_ms     = 0;    // millis() zum Zeitpunkt der Meldung
};

class Stellplatz {
 public:
  Stellplatz(uint8_t nummer, uint8_t inaAdresse);

  bool starten(TwoWire &wire = Wire);

  // Alle 0,5 s aufrufen. 'jetzt' = millis(). Sammelt die Messung fuer den Durchschnitt.
  // Gibt nur Sensor-Ereignisse zurueck (SENSOR_AUS / SENSOR_WIEDER).
  Ereignis messen(uint32_t jetzt);

  // Alle 5 s aufrufen: bildet den Durchschnitt, ENTSCHEIDET damit ueber frei/belegt
  // und beginnt eine neue Runde. 'ereignis' = ANGESTECKT / ABGEZOGEN / KEINES.
  Mittelwert mittelwertBilden(uint32_t jetzt, Ereignis &ereignis);

  // Alle 60 s aufrufen: Meldung aus dem letzten Mittelwert.
  Meldung meldungErstellen(uint32_t jetzt) const;

  uint8_t           nummer() const { return _nummer; }
  Zustand           zustand() const { return _zustand; }
  const Messwert&   letzteMessung() const { return _letzte; }
  const Mittelwert& letzterMittelwert() const { return _mittelwert; }
  uint32_t          steckdauer_s(uint32_t jetzt) const;
  uint32_t          angestecktSeit_ms() const { return _angestecktAb_ms; }

  static const char* zustandText(Zustand z);

 private:
  uint8_t      _nummer;
  Strommessung _sensor;
  Zustand      _zustand = Zustand::FEHLER;   // bis zur ersten gueltigen Messung

  Messwert   _letzte;
  Mittelwert _mittelwert;

  // Messungen der laufenden 5-s-Runde. Nach dem Durchschnitt werden sie verworfen.
  Messwert _messungen[Konfig::MAX_MESSUNGEN];
  uint8_t  _anzahl = 0;

  uint8_t  _fehlZaehler = 0;       // Messungen ohne Antwort in Folge
  uint32_t _fensterStart_ms = 0;   // Zeit der ersten Messung der laufenden 5-s-Runde

  // Steckdauer
  uint32_t _angestecktAb_ms = 0;
  uint32_t _letzteDauer_ms  = 0;
  bool     _hatLetzteDauer  = false;
};
