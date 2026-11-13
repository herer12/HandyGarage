#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Ein einzelner Messwert. gueltig == false, wenn der Sensor nicht geantwortet hat.
struct Messwert {
  bool  gueltig    = false;
  float spannung_V = 0.0f;
  float strom_mA   = 0.0f;
  float leistung_W = 0.0f;
};

// Misst Spannung, Strom und Leistung mit einem INA219.
// Enthaelt keine Logik darueber, ob ein Handy steckt (das macht der Stellplatz).
class Strommessung {
 public:
  explicit Strommessung(uint8_t i2cAdresse);

  // Blockiert nicht, wenn der Sensor fehlt. Rueckgabe: Sensor gefunden?
  bool starten(TwoWire &wire = Wire);

  // Einmal messen. Erkennt einen Ausfall und startet den Sensor bei Rueckkehr neu.
  Messwert messen();

  bool    istVerbunden() const { return _verbunden; }
  uint8_t getAdresse() const   { return _adresse; }

 private:
  Adafruit_INA219 _ina219;
  TwoWire *_wire = &Wire;
  uint8_t  _adresse;
  bool     _verbunden = false;
};
