#include "Stellplatz.h"
#include "Konfig.h"

Stellplatz::Stellplatz(uint8_t nummer, uint8_t inaAdresse)
    : _nummer(nummer), _sensor(inaAdresse) {}

bool Stellplatz::starten(TwoWire &wire) {
  const bool ok = _sensor.starten(wire);
  _zustand = ok ? Zustand::FREI : Zustand::FEHLER;
  return ok;
}

Ereignis Stellplatz::messen(uint32_t jetzt) {
  const Messwert m = _sensor.messen();
  Ereignis e = Ereignis::KEINES;

  // --- Sensor antwortet nicht -------------------------------------------------
  if (!m.gueltig) {
    _letzte = m;
    if (_zustand != Zustand::FEHLER && ++_fehlZaehler >= Konfig::AUSFALL_NACH_MESSUNGEN) {
      if (_zustand == Zustand::BELEGT) {          // Steckdauer abschliessen
        _letzteDauer_ms = jetzt - _angestecktAb_ms;
        _hatLetzteDauer = true;
      }
      _zustand = Zustand::FEHLER;
      e = Ereignis::SENSOR_AUS;
    }
    return e;
  }

  // --- gueltige Messung: nur sammeln, entschieden wird im Mittelwert --------------
  _fehlZaehler = 0;
  _letzte = m;
  if (_anzahl == 0) _fensterStart_ms = jetzt;
  if (_anzahl < Konfig::MAX_MESSUNGEN) _messungen[_anzahl++] = m;

  if (_zustand == Zustand::FEHLER) {              // Sensor ist wieder da
    _zustand = Zustand::FREI;
    e = Ereignis::SENSOR_WIEDER;
  }
  return e;
}

Mittelwert Stellplatz::mittelwertBilden(uint32_t jetzt, Ereignis &ereignis) {
  ereignis = Ereignis::KEINES;
  Mittelwert w;
  w.zeit_ms = jetzt;
  if (_anzahl > 0) {
    float summeU = 0, summeI = 0, summeP = 0;
    for (uint8_t i = 0; i < _anzahl; ++i) {
      summeU += _messungen[i].spannung_V;
      summeI += _messungen[i].strom_mA;
      summeP += _messungen[i].leistung_W;
    }
    w.gueltig    = true;
    w.anzahl     = _anzahl;
    w.spannung_V = summeU / _anzahl;
    w.strom_mA   = summeI / _anzahl;
    w.leistung_W = summeP / _anzahl;

    // Belegung aus dem Durchschnitt (Hysterese: zwischen den Schwellen bleibt es wie es ist).
    // Die Zeit wird auf den Beginn der 5-s-Runde gelegt, also auf ca. 5 s genau.
    if (_zustand == Zustand::FREI && w.strom_mA >= Konfig::BELEGT_AB_mA) {
      _zustand = Zustand::BELEGT;
      _angestecktAb_ms = _fensterStart_ms;
      ereignis = Ereignis::ANGESTECKT;
    } else if (_zustand == Zustand::BELEGT && w.strom_mA < Konfig::FREI_UNTER_mA) {
      _zustand = Zustand::FREI;
      _letzteDauer_ms = _fensterStart_ms - _angestecktAb_ms;
      _hatLetzteDauer = true;
      ereignis = Ereignis::ABGEZOGEN;
    }
  }
  _anzahl = 0;                                    // Einzelmessungen verwerfen
  _mittelwert = w;
  return w;
}

Meldung Stellplatz::meldungErstellen(uint32_t jetzt) const {
  Meldung m;
  m.platz        = _nummer;
  m.zustand      = _zustand;
  m.werte        = _mittelwert;
  m.steckdauer_s = steckdauer_s(jetzt);
  m.zeit_ms      = jetzt;
  return m;
}

uint32_t Stellplatz::steckdauer_s(uint32_t jetzt) const {
  if (_zustand == Zustand::BELEGT) return (jetzt - _angestecktAb_ms) / 1000;
  return _hatLetzteDauer ? _letzteDauer_ms / 1000 : 0;
}

const char* Stellplatz::zustandText(Zustand z) {
  switch (z) {
    case Zustand::FREI:   return "frei";
    case Zustand::BELEGT: return "belegt";
    default:              return "fehler";
  }
}
