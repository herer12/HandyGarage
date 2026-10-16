#include "Strommessung.h"

Strommessung::Strommessung(uint8_t i2cAdresse)
    : _ina219(i2cAdresse), _adresse(i2cAdresse) {}

bool Strommessung::starten(TwoWire &wire) {
  _wire = &wire;
  // begin() nutzt den Messbereich 32 V / 2 A (Shunt 0,1 Ohm -> max. ca. 3,2 A).
  _verbunden = _ina219.begin(_wire);
  return _verbunden;
}

void Strommessung::messen() {
  // Antwortet der Sensor noch? (Funktionseinheiten sind demontierbar)
  _wire->beginTransmission(_adresse);
  const bool antwortet = (_wire->endTransmission() == 0);

  if (!antwortet) {
    _verbunden = false;
    _strom_mA = 0;
    _leistung_W = 0;
    _loadvoltage_V = 0;
    return;
  }

  if (!_verbunden && !starten(*_wire)) return;   // wieder da -> neu starten

  const float shunt_mV = _ina219.getShuntVoltage_mV();
  const float bus_V    = _ina219.getBusVoltage_V() * BUS_FAKTOR + BUS_OFFSET;
  _strom_mA      = _ina219.getCurrent_mA();
  _leistung_W    = _ina219.getPower_mW() / 1000.0f;
  _loadvoltage_V = bus_V + (shunt_mV / 1000.0f);
}
