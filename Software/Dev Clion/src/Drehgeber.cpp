#include "Drehgeber.h"
#include "Konfig.h"
#include "driver/gpio.h"

void Drehgeber::starten(uint8_t pinA, uint8_t pinB, uint8_t pinTaster) {
  _pinA = pinA;
  _pinB = pinB;
  _pinTaster = pinTaster;

  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);
  pinMode(_pinTaster, INPUT_PULLUP);

  _quad = (digitalRead(_pinA) << 1) | digitalRead(_pinB);
  _schritte = 0;

  attachInterruptArg(digitalPinToInterrupt(_pinA), flanke, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(_pinB), flanke, this, CHANGE);
}

void IRAM_ATTR Drehgeber::flanke(void *self) {
  static_cast<Drehgeber *>(self)->flankeBehandeln();
}

void IRAM_ATTR Drehgeber::flankeBehandeln() {
  // Index = (alter Zustand << 2) | neuer Zustand, jeweils (A << 1) | B.
  // Gueltige Uebergaenge zaehlen +1 / -1, Prellen und ungueltige Spruenge 0.
  static DRAM_ATTR const int8_t TABELLE[16] = {0, -1, 1, 0,  1, 0, 0, -1,  -1, 0, 0, 1,  0, 1, -1, 0};

  const uint8_t a = gpio_get_level((gpio_num_t)_pinA);
  const uint8_t b = gpio_get_level((gpio_num_t)_pinB);
  _quad = ((_quad << 2) | (a << 1) | b) & 0x0F;

  portENTER_CRITICAL_ISR(&_sperre);
  _schritte += TABELLE[_quad];
  portEXIT_CRITICAL_ISR(&_sperre);
}

int8_t Drehgeber::drehung() {
  int16_t rasten;
  portENTER_CRITICAL(&_sperre);
  rasten = _schritte / Konfig::RE_SCHRITTE_PRO_RASTUNG;
  _schritte -= rasten * Konfig::RE_SCHRITTE_PRO_RASTUNG;   // Rest bleibt fuer die naechste Raste
  portEXIT_CRITICAL(&_sperre);
  return (int8_t)constrain(rasten, -127, 127);
}

bool Drehgeber::gedrueckt() {
  const uint32_t jetzt = millis();
  const bool roh = (digitalRead(_pinTaster) == LOW);

  if (roh != _tasterRoh) {                 // Signal hat gewechselt -> Entprellzeit neu starten
    _tasterRoh  = roh;
    _wechsel_ms = jetzt;
  }
  if (roh != _tasterStabil && (jetzt - _wechsel_ms) >= Konfig::TASTER_ENTPRELL_MS) {
    _tasterStabil = roh;
    return roh;                            // nur beim Druecken, nicht beim Loslassen
  }
  return false;
}
