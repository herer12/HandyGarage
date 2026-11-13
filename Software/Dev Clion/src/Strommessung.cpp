#include "Strommessung.h"
#include "Konfig.h"
#include <math.h>

Strommessung::Strommessung(uint8_t i2cAdresse)
    : _ina219(i2cAdresse), _adresse(i2cAdresse) {}

bool Strommessung::starten(TwoWire &wire) {
  _wire = &wire;
  // begin() nutzt den Messbereich 32 V / 2 A (Shunt 0,1 Ohm, max. ca. 3,2 A).
  // Das passt fuer 20 V und 24 V Versorgung (INA219: max. 26 V am Eingang).
  _verbunden = _ina219.begin(_wire);
  return _verbunden;
}

Messwert Strommessung::messen() {
  Messwert m;

  // Antwortet der Sensor? (Funktionseinheiten sind demontierbar)
  _wire->beginTransmission(_adresse);
  if (_wire->endTransmission() != 0) {
    _verbunden = false;
    return m;                                   // gueltig = false
  }

  // Sensor ist wieder da -> neu starten
  if (!_verbunden && !starten(*_wire)) return m;

  const float bus_V    = _ina219.getBusVoltage_V() * Konfig::BUS_FAKTOR + Konfig::BUS_OFFSET_V;
  const float strom_mA = _ina219.getCurrent_mA() * Konfig::STROM_FAKTOR + Konfig::STROM_OFFSET_mA;

  m.spannung_V = bus_V;
  m.strom_mA   = strom_mA;
  m.leistung_W = m.spannung_V * strom_mA / 1000.0f;  
  m.gueltig    = isfinite(m.spannung_V) && isfinite(m.strom_mA);
  return m;
}
