#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Misst Spannung, Strom und Leistung mit einem INA219.
// Hat keine Logik darüber, ob ein Handy steckt (das macht der Stellplatz).
class Strommessung {
 public:
  explicit Strommessung(uint8_t i2cAdresse);

  // Blockiert NICHT, wenn der Sensor fehlt. Rückgabe: Sensor gefunden?
  bool starten(TwoWire &wire = Wire);

  // Einmal messen. Erkennt Ausfall und startet den Sensor bei Rückkehr neu.
  void messen();

  bool    istVerbunden() const { return _verbunden; }
  uint8_t getAdresse() const   { return _adresse; }

  float getLoadvoltage() const { return _loadvoltage_V; }
  float getCurrent_mA() const  { return _strom_mA; }
  float getCurrent_A() const   { return _strom_mA / 1000.0f; }
  float getPower_W() const     { return _leistung_W; }

 private:
  // Empirische Korrektur der Busspannung (aus dem Testaufbau übernommen).
  static constexpr float BUS_FAKTOR = 0.95779409f;
  static constexpr float BUS_OFFSET = 1.82118029f;

  Adafruit_INA219 _ina219;
  TwoWire *_wire = &Wire;
  uint8_t _adresse;
  bool _verbunden = false;

  float _strom_mA = 0;
  float _loadvoltage_V = 0;
  float _leistung_W = 0;
};
