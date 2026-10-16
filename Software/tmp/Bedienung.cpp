#include "Bedienung.h"

// Gray-Code-Tabelle: Index = (alterZustand << 2) | neuerZustand, Zustand = (CLK<<1)|DT.
// Liefert pro gültigem Schritt +1 / -1, bei Prellen oder Fehlern 0.
static const DRAM_ATTR int8_t ENC_TABELLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0};

static const uint32_t ENTPRELL_MS = 30;
static const int8_t   FLANKEN_PRO_RASTUNG = 4;   // KY-040: 4 Zustandswechsel je Rastung

Bedienung::Bedienung(int pinClk, int pinDt, int pinSw, bool richtungUmkehren)
    : _pinClk(pinClk), _pinDt(pinDt), _pinSw(pinSw), _umkehren(richtungUmkehren) {}

void Bedienung::starten() {
  pinMode(_pinClk, INPUT_PULLUP);
  pinMode(_pinDt, INPUT_PULLUP);
  pinMode(_pinSw, INPUT_PULLUP);

  _alt = (digitalRead(_pinClk) << 1) | digitalRead(_pinDt);
  attachInterruptArg(digitalPinToInterrupt(_pinClk), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(_pinDt), isr, this, CHANGE);
}

void IRAM_ATTR Bedienung::isr(void *arg) {
  static_cast<Bedienung *>(arg)->isrAuswerten();
}

void IRAM_ATTR Bedienung::isrAuswerten() {
  const uint8_t neu = (digitalRead(_pinClk) << 1) | digitalRead(_pinDt);
  _summe += ENC_TABELLE[(_alt << 2) | neu];
  _alt = neu;

  if (_summe >= FLANKEN_PRO_RASTUNG) {
    _schritte += _umkehren ? -1 : 1;
    _summe = 0;
  } else if (_summe <= -FLANKEN_PRO_RASTUNG) {
    _schritte += _umkehren ? 1 : -1;
    _summe = 0;
  }
}

void Bedienung::aktualisieren() {
  // Taster (aktiv LOW) entprellen; Klick beim Drücken
  const bool roh = digitalRead(_pinSw);
  const uint32_t jetzt = millis();

  if (roh != _rohAlt) {
    _rohAlt = roh;
    _tWechsel = jetzt;
  }
  if ((jetzt - _tWechsel) > ENTPRELL_MS && roh != _stabil) {
    _stabil = roh;
    if (_stabil == LOW) _klick = true;
  }
}

int8_t Bedienung::leseDrehung() {
  noInterrupts();
  const int8_t s = _schritte;
  _schritte = 0;
  interrupts();
  return s;
}

bool Bedienung::wurdeGeklickt() {
  const bool k = _klick;
  _klick = false;
  return k;
}
