#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Konfig.h"

/**
 * Messwerte eines Stellplatzes, wie sie das Display anzeigen soll.
 */
struct PlatzDaten {
    float spannung_V = 0.0f;
    float strom_mA   = 0.0f;
    float leistung_W = 0.0f;
    bool  belegt     = false;   // Handy erkannt (Strom über Schwelle)
    bool  ladenAn    = true;    // MOSFET schaltet das Lademodul durch
};

/**
 * SSD1306 OLED (128x64, I2C). Alle Einstellungen kommen aus Konfig.h:
 *   Konfig::PIN_SDA / PIN_SCL / I2C_TAKT_HZ / OLED_ADRESSE
 *   Konfig::WIDTH / HEIGHT, Konfig::ANZAHL_PLAETZE, Konfig::GERAETE_NAME
 *
 * Benötigte Bibliotheken: "Adafruit SSD1306" und "Adafruit GFX Library".
 */
class Display {
public:
    Display();

    bool begin();

    // --- Fertige Bildschirme -------------------------------------------------
    void zeigeStart();                                            // Gerätename beim Booten
    void zeigeUebersicht(const PlatzDaten daten[], uint8_t anzahl, bool wlanVerbunden);
    void zeigePlatz(uint8_t index, const PlatzDaten &daten);      // Detailseite eines Platzes
    void zeigeMeldung(const String &titel, const String &text);

    // --- Grundfunktionen -----------------------------------------------------
    void loeschen();
    void anzeigen();                                              // Puffer ans Display senden
    void text(int16_t x, int16_t y, const String &s, uint8_t groesse = 1);
    void textMitte(int16_t y, const String &s, uint8_t groesse = 1);
    void balken(int16_t x, int16_t y, int16_t b, int16_t h, uint8_t prozent);

    // --- Einstellungen -------------------------------------------------------
    void helligkeit(uint8_t wert);        // 0..255
    void anAus(bool an);
    void invertieren(bool invertiert);
    void drehen(uint8_t rotation);        // 0..3

    // Direkter Zugriff auf Adafruit_GFX (Linien, Kreise, Bitmaps, ...)
    Adafruit_SSD1306 &roh() { return _oled; }
    bool bereit() const { return _bereit; }

private:
    static const char *statusText(const PlatzDaten &d);
    void kopfzeile(const String &links, const String &rechts = "");

    Adafruit_SSD1306 _oled;
    bool _bereit = false;
};
