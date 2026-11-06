#include "Display.h"

Display::Display()
    : _oled(Konfig::WIDTH, Konfig::HEIGHT, &Wire, -1) {}   // -1 = kein Reset-Pin

bool Display::begin() {
    Wire.begin();

    // reset = true, periphBegin = false (Wire wurde oben schon gestartet)
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, Konfig::OLED_ADRESSE, true, false)) {
        if (Konfig::SERIAL_DEBUG) {
            Serial.print("SSD1306 nicht gefunden (Adresse 0x%02X, SDA=%d, SCL=%d)\n");
        }
        _bereit = false;
        return false;
    }

    _oled.setTextColor(SSD1306_WHITE);
    _oled.setTextWrap(false);
    _oled.clearDisplay();
    _oled.display();
    _bereit = true;
    return true;
}

// ---------------------------------------------------------------------------
//  Fertige Bildschirme
// ---------------------------------------------------------------------------

void Display::zeigeStart() {
    loeschen();
    textMitte(14, "Handygarage", 2);
    textMitte(40, Konfig::GERAETE_NAME, 1);
    anzeigen();
}

void Display::zeigeUebersicht(const PlatzDaten daten[], uint8_t anzahl, bool wlanVerbunden) {
    if (anzahl > Konfig::ANZAHL_PLAETZE) anzahl = Konfig::ANZAHL_PLAETZE;
    if (anzahl > 4) anzahl = 4;

    loeschen();
    kopfzeile(Konfig::GERAETE_NAME, wlanVerbunden ? "WLAN" : "---");

    char zeile[24];
    for (uint8_t i = 0; i < anzahl; i++) {
        // Beispiel: "1 BELEGT 5.02V  420mA"  (max. 21 Zeichen = 126 px)
        snprintf(zeile, sizeof(zeile), "%u %-6s %4.2fV %4.0fmA",
                 i + 1, statusText(daten[i]), daten[i].spannung_V, daten[i].strom_mA);
        text(0, 15 + i * 12, zeile, 1);
    }
    anzeigen();
}

void Display::zeigePlatz(uint8_t index, const PlatzDaten &d) {
    loeschen();
    kopfzeile("Platz " + String(index + 1), statusText(d));

    char zeile[24];

    // Strom groß in der Mitte
    snprintf(zeile, sizeof(zeile), "%.0f mA", d.strom_mA);
    textMitte(17, zeile, 2);

    // Spannung und Leistung unten
    snprintf(zeile, sizeof(zeile), "%.2f V", d.spannung_V);
    text(0, 42, zeile, 1);
    snprintf(zeile, sizeof(zeile), "%.2f W", d.leistung_W);
    text(Konfig::WIDTH - 6 * strlen(zeile), 42, zeile, 1);

    // Ladeschalter-Status
    text(0, 54, d.ladenAn ? "Laden: AN" : "Laden: AUS", 1);
    anzeigen();
}

void Display::zeigeMeldung(const String &titel, const String &txt) {
    loeschen();
    kopfzeile(titel);
    textMitte(30, txt, 2);
    anzeigen();
}

// ---------------------------------------------------------------------------
//  Grundfunktionen
// ---------------------------------------------------------------------------

void Display::loeschen() {
    _oled.clearDisplay();
}

void Display::anzeigen() {
    _oled.display();
}

void Display::text(int16_t x, int16_t y, const String &s, uint8_t groesse) {
    _oled.setTextSize(groesse);
    _oled.setCursor(x, y);
    _oled.print(s);
}

void Display::textMitte(int16_t y, const String &s, uint8_t groesse) {
    int16_t x1, y1;
    uint16_t w, h;
    _oled.setTextSize(groesse);
    _oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    _oled.setCursor((Konfig::WIDTH - w) / 2, y);
    _oled.print(s);
}

void Display::balken(int16_t x, int16_t y, int16_t b, int16_t h, uint8_t prozent) {
    if (prozent > 100) prozent = 100;
    _oled.drawRect(x, y, b, h, SSD1306_WHITE);
    _oled.fillRect(x + 2, y + 2, ((b - 4) * prozent) / 100, h - 4, SSD1306_WHITE);
}

// ---------------------------------------------------------------------------
//  Einstellungen
// ---------------------------------------------------------------------------

void Display::helligkeit(uint8_t wert) {
    _oled.ssd1306_command(SSD1306_SETCONTRAST);
    _oled.ssd1306_command(wert);
}

void Display::anAus(bool an) {
    _oled.ssd1306_command(an ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void Display::invertieren(bool invertiert) {
    _oled.invertDisplay(invertiert);
}

void Display::drehen(uint8_t rotation) {
    _oled.setRotation(rotation);
}

// ---------------------------------------------------------------------------
//  Intern
// ---------------------------------------------------------------------------

const char *Display::statusText(const PlatzDaten &d) {
    if (!d.ladenAn) return "AUS";
    return d.belegt ? "BELEGT" : "FREI";
}

void Display::kopfzeile(const String &links, const String &rechts) {
    text(0, 0, links, 1);
    if (rechts.length() > 0) {
        text(Konfig::WIDTH - 6 * rechts.length(), 0, rechts, 1);
    }
    _oled.drawFastHLine(0, 11, Konfig::WIDTH, SSD1306_WHITE);
}
