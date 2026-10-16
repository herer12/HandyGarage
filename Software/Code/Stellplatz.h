#include "Arduino.h"
#ifndef STELLPLATZ_H
#define STELLPLATZ_H

#include "Strommessung.h"
#include "Konfig.h"

class Stellplatz {
public:
    Stellplatz(Strommessung& strommessung);
    bool setupStrommessung();
    void messenUndSpeichern();

    float* getLoadvoltage();
    float* getCurrent_A();
    float* getPower_W();
    boolean isConnected();

private:
    Strommessung& _strommessung;

    static constexpr uint8_t ANZAHL_WERTE = 5;
    float _loadvoltage_V[ANZAHL_WERTE] = {};
    float _current_A[ANZAHL_WERTE] = {};
    float _power_W[ANZAHL_WERTE] = {};
};

#endif
