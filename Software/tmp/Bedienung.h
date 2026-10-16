#pragma once
#include <Arduino.h>

// Rotary-Encoder KY-040. Drehen wird per Interrupt erfasst (geht auch dann
// nicht verloren, wenn loop() kurz beschäftigt ist), der Taster per Polling.
//
// Benutzung:
//   int8_t schritte = bedienung.leseDrehung();   // + rechts, - links, 0 nichts
//   if (bedienung.wurdeGeklickt()) { ... }
class Bedienung {
 public:
  Bedienung(int pinClk, int pinDt, int pinSw, bool richtungUmkehren = false);

  void starten();
  void aktualisieren();          // zyklisch aufrufen (Taster entprellen)

  int8_t leseDrehung();          // Rastungen seit dem letzten Aufruf
  bool   wurdeGeklickt();        // true genau einmal pro Tastendruck

 private:
  static void IRAM_ATTR isr(void *arg);
  void IRAM_ATTR isrAuswerten();

  int  _pinClk, _pinDt, _pinSw;
  bool _umkehren;

  // Drehgeber (wird im Interrupt verändert)
  volatile int8_t _schritte = 0;
  volatile int8_t _summe = 0;
  volatile uint8_t _alt = 0;

  // Taster
  bool     _rohAlt = HIGH;
  bool     _stabil = HIGH;
  uint32_t _tWechsel = 0;
  bool     _klick = false;
};
