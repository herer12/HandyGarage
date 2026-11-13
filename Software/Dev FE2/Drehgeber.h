#pragma once
#include <Arduino.h>

// ============================================================================
//  Drehgeber (RE) mit Taster
//    Drehen  -> per Interrupt (keine Raste geht verloren, auch wenn loop() mal traege ist)
//    Taster  -> entprellt, wird in gedrueckt() abgefragt
// ============================================================================
class Drehgeber {
 public:
  void starten(uint8_t pinA, uint8_t pinB, uint8_t pinTaster);

  // Rasten seit dem letzten Aufruf: + = rechts herum, - = links herum.
  int8_t drehung();

  // true genau EINMAL pro Tastendruck.
  bool gedrueckt();

 private:
  static void flanke(void *self);   // Interrupt (A oder B hat gewechselt)
  void        flankeBehandeln();

  uint8_t _pinA = 0, _pinB = 0, _pinTaster = 0;

  volatile uint8_t  _quad     = 0;      // letzter + neuer Zustand von A/B
  volatile int16_t  _schritte = 0;      // Viertelschritte, noch nicht in Rasten umgerechnet
  portMUX_TYPE      _sperre   = portMUX_INITIALIZER_UNLOCKED;

  bool     _tasterRoh    = false;       // true = gedrueckt (rohes Signal)
  bool     _tasterStabil = false;       // true = gedrueckt (entprellt)
  uint32_t _wechsel_ms   = 0;
};
