#include "Arduino.h"
#include "Stellplatz.h"

Stellplatz::Stellplatz(Strommessung& strommessung)
    : _strommessung(strommessung) {}

bool Stellplatz::setupStrommessung() {
    return _strommessung.starten(Wire);
}

void Stellplatz::messenUndSpeichern() {
    _strommessung.messen();

    // Ältesten Wert entfernen und die neuesten Messwerte ans Ende schieben.
    for (uint8_t i = 0; i < ANZAHL_WERTE - 1; ++i) {
        _loadvoltage_V[i] = _loadvoltage_V[i + 1];
        _current_A[i] = _current_A[i + 1];
        _power_W[i] = _power_W[i + 1];
    }

    _loadvoltage_V[ANZAHL_WERTE - 1] = _strommessung.getLoadvoltage();
    _current_A[ANZAHL_WERTE - 1] = _strommessung.getCurrent_A();
    _power_W[ANZAHL_WERTE - 1] = _strommessung.getPower_W();
}

boolean Stellplatz::isConnected(){
  if(*getCurrent_A()>Konfig::FREI_UNTER_mA){
    return true;
  }
  return false;
}

float* Stellplatz::getLoadvoltage() { return _loadvoltage_V; }
float* Stellplatz::getCurrent_A() { return _current_A; }
float* Stellplatz::getPower_W() { return _power_W; }
