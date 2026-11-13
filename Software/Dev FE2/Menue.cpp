#include "Menue.h"
#include <Wire.h>
#include <string.h>

namespace {
// Nur Schriften mit Latin-1 ("_tf"), damit ae/oe/ue-Umlaute gehen.
const uint8_t *const FONT_GROSS = u8g2_font_helvB14_tf;
const uint8_t *const FONT_TITEL = u8g2_font_helvB08_tf;
const uint8_t *const FONT_TEXT  = u8g2_font_6x12_tf;

constexpr int BREITE           = 128;
constexpr int SICHTBARE_ZEILEN = 4;     // 4 x 13 px + 12 px Titel = 64 px
constexpr int ZEILE_H          = 13;
constexpr int LISTE_Y          = 12;

constexpr int KNOPF_Y = 50, KNOPF_W = 62, KNOPF_H = 14;

// Eigener I2C-Callback fuer U8g2 auf Wire1.
// Der eingebaute (u8x8_byte_arduino_2nd_hw_i2c) ruft bei INIT Wire1.begin() OHNE Pins auf und
// kann den Bus damit auf falsche Pins zuruecksetzen -> Display wird erkannt, bleibt aber dunkel.
// Hier wird Wire1 nur benutzt, nicht neu gestartet (das macht Menue::starten() mit den Pins aus Konfig).
extern "C" uint8_t menueByteWire1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:
      Wire1.write(static_cast<const uint8_t *>(arg_ptr), arg_int);
      break;
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      Wire1.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      Wire1.endTransmission();
      break;
    default:
      return 0;
  }
  return 1;
}
}  // namespace

// ---------------------------------------------------------------------------------
//  Start
// ---------------------------------------------------------------------------------
Menue::Menue(Belegungserkennung &belegung)
    : _belegung(belegung) {}

bool Menue::starten() {
  _re.starten(Konfig::PIN_RE_A, Konfig::PIN_RE_B, Konfig::PIN_RE_TASTER);

  Wire1.begin(Konfig::PIN_OLED_SDA, Konfig::PIN_OLED_SCL, Konfig::OLED_TAKT_HZ);
  Wire1.setTimeOut(Konfig::I2C_TIMEOUT_MS);

  // Display da? Ein fehlendes Display darf das System nicht blockieren.
  Wire1.beginTransmission(Konfig::OLED_ADRESSE);
  _displayDa = (Wire1.endTransmission() == 0);
  if (!_displayDa) return false;

  u8g2_Setup_ssd1306_i2c_128x64_noname_f(_u8g2.getU8g2(), U8G2_R0, menueByteWire1,
                                         u8x8_gpio_and_delay_arduino);
  u8x8_SetPin_HW_I2C(_u8g2.getU8x8(), U8X8_PIN_NONE);   // kein Reset-Pin (sonst wuerde GPIO 0 geschaltet)
  _u8g2.setI2CAddress(Konfig::OLED_ADRESSE << 1);      // U8g2 erwartet die 8-Bit-Adresse
  _u8g2.initDisplay();
  _u8g2.clearDisplay();
  _u8g2.setPowerSave(0);                               // Display einschalten
  _u8g2.setContrast(255);

  _startZeit_ms = millis();
  _ansicht = Ansicht::START;
  zeichnen();
  return true;
}

void Menue::startFortschritt(uint8_t prozent, const char *text) {
  _startProzent = prozent > 100 ? 100 : prozent;
  strlcpy(_startText, text ? text : "", sizeof _startText);
  if (_displayDa && _ansicht == Ansicht::START) zeichnen();
}

void Menue::startAbschliessen() {
  _startProzent = 100;
  strlcpy(_startText, "Bereit", sizeof _startText);
  _startFertig = true;
  if (_displayDa && _ansicht == Ansicht::START) zeichnen();
}

void Menue::benutzerSetzen(uint8_t platz, const char *name) {
  if (platz < 1 || platz > Konfig::ANZAHL_PLAETZE) return;
  strlcpy(_info[platz - 1].benutzer, (name && *name) ? name : "Gast", sizeof _info[0].benutzer);
  _neuZeichnen = true;
}

void Menue::benutzerLoeschen(uint8_t platz) { benutzerSetzen(platz, "Gast"); }

void Menue::beiAbmelden(AbmeldenCallback callback) { _abmelden = std::move(callback); }

// ---------------------------------------------------------------------------------
//  Hauptschleife der Anzeige
// ---------------------------------------------------------------------------------
void Menue::aktualisieren() {
  const uint32_t jetzt = millis();

  energieNachfuehren();                       // auch im Hintergrund, egal welches Menue offen ist

  const int8_t drehung = _re.drehung();
  const bool   klick   = _re.gedrueckt();
  if (!_displayDa) return;

  if (_ansicht == Ansicht::START) {
    // Eingaben ignorieren; wechseln, sobald der Start fertig ist (und der Schirm lesbar war)
    if (_startFertig && (jetzt - _startZeit_ms) >= Konfig::START_MINDESTZEIT_MS) {
      _ansicht = Ansicht::HAUPTMENUE;
      _neuZeichnen = true;
    }
  } else {
    if (drehung || klick) {
      eingabe(drehung, klick);
      _neuZeichnen = true;
    }
    if (_hinweisAktiv && (int32_t)(jetzt - _hinweisBis_ms) >= 0) {
      _hinweisAktiv = false;
      _neuZeichnen = true;
    }
  }

  const uint32_t intervall = (_ansicht == Ansicht::START) ? Konfig::START_ANIMATION_MS
                                                          : Konfig::ANZEIGE_INTERVALL_MS;
  if (_neuZeichnen || (jetzt - _letzteZeichnung_ms) >= intervall) zeichnen();
}

// Energie = Leistung x Zeit, aufsummiert aus den 5-s-Mittelwerten, solange ein Handy steckt.
// Gemessen wird auf der 24-V-Seite des Lademoduls -> inklusive Verluste des Moduls.
void Menue::energieNachfuehren() {
  for (uint8_t platz = 1; platz <= Konfig::ANZAHL_PLAETZE; ++platz) {
    PlatzInfo &info = _info[platz - 1];
    const Zustand z = _belegung.zustand(platz);

    if (z != info.zustand) {
      if (z == Zustand::BELEGT) info.energie_Wh = 0.0f;       // neuer Ladevorgang
      info.zustand = z;
    }

    const Mittelwert w = _belegung.letzterMittelwert(platz);
    if (w.zeit_ms != info.mittelwertZeit_ms) {                // neuer 5-s-Mittelwert
      info.mittelwertZeit_ms = w.zeit_ms;
      if (z == Zustand::BELEGT && w.gueltig && w.leistung_W > 0.0f) {
        info.energie_Wh += w.leistung_W * (Konfig::MITTEL_INTERVALL_MS / 1000.0f) / 3600.0f;
      }
    }
  }
}

// ---------------------------------------------------------------------------------
//  Eingabe
// ---------------------------------------------------------------------------------
void Menue::eingabe(int8_t drehung, bool klick) {
  if (Konfig::RE_INVERTIEREN) drehung = -drehung;

  switch (_ansicht) {
    case Ansicht::HAUPTMENUE: {
      if (_hinweisAktiv) {                      // Hinweis mit Tastendruck wegklicken
        _hinweisAktiv = false;
        if (klick) return;
      }
      if (drehung) {
        const int anzahl = Konfig::ANZAHL_PLAETZE;
        int n = ((int)_auswahl + drehung) % anzahl;
        if (n < 0) n += anzahl;
        _auswahl = (uint8_t)n;
        scrollAnpassen();
      }
      if (klick) {
        _aktPlatz = _auswahl + 1;
        _knopf = Knopf::ZURUECK;                // sicherer Standard: nicht versehentlich abmelden
        _ansicht = Ansicht::STECKPLATZ;
      }
      break;
    }

    case Ansicht::STECKPLATZ: {
      const bool abmeldenOk = abmeldenMoeglich(_aktPlatz);
      if (drehung > 0)                    _knopf = Knopf::ZURUECK;     // rechts
      else if (drehung < 0 && abmeldenOk) _knopf = Knopf::ABMELDEN;    // links
      if (klick) {
        if (_knopf == Knopf::ZURUECK)  _ansicht = Ansicht::HAUPTMENUE;
        else if (abmeldenOk)           abmelden(_aktPlatz);
      }
      break;
    }

    default: break;
  }
}

void Menue::scrollAnpassen() {
  if (_auswahl < _scroll) _scroll = _auswahl;
  if (_auswahl >= _scroll + SICHTBARE_ZEILEN) _scroll = _auswahl - SICHTBARE_ZEILEN + 1;
}

bool Menue::abmeldenMoeglich(uint8_t platz) const {
  return _belegung.zustand(platz) == Zustand::BELEGT;   // ohne Handy gibt es nichts abzumelden
}

void Menue::abmelden(uint8_t platz) {
  if (_abmelden) _abmelden(platz);
  benutzerLoeschen(platz);                              // Anzeige: wieder "Gast"

  snprintf(_hinweisText, sizeof _hinweisText, "%s %u abgemeldet", Konfig::PLATZ_NAME, platz);
  _hinweisAktiv  = true;
  _hinweisBis_ms = millis() + Konfig::HINWEIS_DAUER_MS;
  _ansicht = Ansicht::HAUPTMENUE;
}

// ---------------------------------------------------------------------------------
//  Zeichnen
// ---------------------------------------------------------------------------------
void Menue::zeichnen() {
  switch (_ansicht) {
    case Ansicht::START:      zeichneStart();      break;
    case Ansicht::HAUPTMENUE: zeichneHauptmenue(); if (_hinweisAktiv) zeichneHinweis(); break;
    case Ansicht::STECKPLATZ: zeichneSteckplatz(); break;
  }
  _u8g2.sendBuffer();
  _letzteZeichnung_ms = millis();
  _neuZeichnen = false;
}

const char *Menue::statusText(Zustand z) {
  switch (z) {
    case Zustand::BELEGT: return "angesteckt";
    case Zustand::FREI:   return "frei";
    default:              return "Fehler";
  }
}

void Menue::textRechts(int xRechts, int yBasis, const char *text) {
  _u8g2.drawUTF8(xRechts - _u8g2.getUTF8Width(text) + 1, yBasis, text);
}

// Text auf maxBreite Pixel kuerzen und ".." anhaengen (UTF-8-sicher, schneidet keine Umlaute durch)
void Menue::kuerzen(const char *quelle, char *ziel, size_t groesse, int maxBreite) {
  strlcpy(ziel, quelle, groesse);
  if (_u8g2.getUTF8Width(ziel) <= maxBreite) return;

  size_t n = strlen(ziel);
  while (n > 0) {
    do { --n; } while (n > 0 && (static_cast<uint8_t>(ziel[n]) & 0xC0) == 0x80);
    if (n + 3 > groesse) continue;
    ziel[n] = '.'; ziel[n + 1] = '.'; ziel[n + 2] = '\0';
    if (_u8g2.getUTF8Width(ziel) <= maxBreite) return;
  }
}

// --- Startbildschirm -------------------------------------------------------------------
void Menue::zeichneStart() {
  _u8g2.clearBuffer();
  _u8g2.setDrawColor(1);

  _u8g2.setFont(FONT_GROSS);
  const char *titel = "Handygarage";
  _u8g2.drawUTF8((BREITE - _u8g2.getUTF8Width(titel)) / 2, 22, titel);

  // Aktueller Startschritt mit wandernden Punkten, damit man sieht, dass es nicht haengt
  _u8g2.setFont(FONT_TEXT);
  char zeile[40];
  const int punkte = (millis() / 300) % 4;
  snprintf(zeile, sizeof zeile, "%s%.*s", _startText, _startFertig ? 0 : punkte, "...");
  _u8g2.drawUTF8(4, 37, zeile);

  // Ladebalken
  _u8g2.drawFrame(4, 44, 120, 9);
  _u8g2.drawBox(6, 46, (116 * _startProzent) / 100, 5);

  char prozent[8];
  snprintf(prozent, sizeof prozent, "%u %%", _startProzent);
  textRechts(BREITE - 4, 63, prozent);
}

// --- Hauptmenue ----------------------------------------------------------------------------
void Menue::zeichneHauptmenue() {
  _u8g2.clearBuffer();
  _u8g2.setDrawColor(1);

  uint8_t belegt = 0;
  for (uint8_t p = 1; p <= Konfig::ANZAHL_PLAETZE; ++p)
    if (_belegung.zustand(p) == Zustand::BELEGT) ++belegt;

  _u8g2.setFont(FONT_TITEL);
  _u8g2.drawUTF8(0, 9, "Handygarage");
  char zaehler[12];
  snprintf(zaehler, sizeof zaehler, "%u/%u", belegt, Konfig::ANZAHL_PLAETZE);
  textRechts(BREITE - 1, 9, zaehler);
  _u8g2.drawHLine(0, 11, BREITE);

  const bool scrollbar = Konfig::ANZAHL_PLAETZE > SICHTBARE_ZEILEN;
  const int  rechts    = scrollbar ? BREITE - 5 : BREITE - 1;     // letzte Pixelspalte der Liste

  _u8g2.setFont(FONT_TEXT);
  for (int z = 0; z < SICHTBARE_ZEILEN; ++z) {
    const int idx = _scroll + z;
    if (idx >= Konfig::ANZAHL_PLAETZE) break;

    const uint8_t platz = idx + 1;
    const Zustand zs    = _belegung.zustand(platz);
    const int     y     = LISTE_Y + z * ZEILE_H;

    if (idx == _auswahl) {                         // gewaehlte Zeile: weisser Balken, schwarze Schrift
      _u8g2.setDrawColor(1);
      _u8g2.drawBox(0, y, rechts + 1, ZEILE_H);
      _u8g2.setDrawColor(0);
    } else {
      _u8g2.setDrawColor(1);
    }

    // Symbol: ausgefuellt = angesteckt, Ring = frei, Kreuz = Fehler
    const int cx = 6, cy = y + 6;
    if (zs == Zustand::BELEGT)      _u8g2.drawDisc(cx, cy, 3);
    else if (zs == Zustand::FREI)   _u8g2.drawCircle(cx, cy, 3);
    else {
      _u8g2.drawLine(cx - 3, cy - 3, cx + 3, cy + 3);
      _u8g2.drawLine(cx - 3, cy + 3, cx + 3, cy - 3);
    }

    char name[20];
    snprintf(name, sizeof name, "%s %u", Konfig::PLATZ_NAME, platz);
    _u8g2.drawUTF8(15, y + 10, name);
    textRechts(rechts - 2, y + 10, statusText(zs));
  }
  _u8g2.setDrawColor(1);

  if (scrollbar) {                                  // nur noetig, wenn mehr Plaetze als Zeilen
    const int hoehe  = SICHTBARE_ZEILEN * ZEILE_H;
    const int spanne = max(1, (int)Konfig::ANZAHL_PLAETZE - SICHTBARE_ZEILEN);
    const int daumen = max(6, hoehe * SICHTBARE_ZEILEN / (int)Konfig::ANZAHL_PLAETZE);
    const int daumenY = LISTE_Y + (hoehe - daumen) * _scroll / spanne;
    _u8g2.drawVLine(126, LISTE_Y, hoehe);
    _u8g2.drawBox(125, daumenY, 3, daumen);
  }
}

// --- Steckplatz-Menue ------------------------------------------------------------------------
void Menue::zeichneSteckplatz() {
  const uint8_t   platz = _aktPlatz;
  const Zustand   z     = _belegung.zustand(platz);
  const PlatzInfo &info = _info[platz - 1];

  // Gewaehlter Knopf muss gueltig sein (Abmelden gibt es nur, solange ein Handy steckt)
  if (_knopf == Knopf::ABMELDEN && !abmeldenMoeglich(platz)) _knopf = Knopf::ZURUECK;

  _u8g2.clearBuffer();
  _u8g2.setDrawColor(1);

  // Titel
  _u8g2.setFont(FONT_TITEL);
  char titel[20];
  snprintf(titel, sizeof titel, "%s %u", Konfig::PLATZ_NAME, platz);
  _u8g2.drawUTF8(0, 9, titel);
  textRechts(BREITE - 1, 9, statusText(z));
  _u8g2.drawHLine(0, 11, BREITE);

  // Infozeilen
  _u8g2.setFont(FONT_TEXT);
  char wert[24];

  if (z == Zustand::BELEGT) {
    // Zeile 1: Symbol + Benutzer
    _u8g2.drawDisc(4, 17, 2);                       // Kopf
    _u8g2.drawRBox(0, 20, 9, 4, 2);                 // Schultern
    char name[48];
    kuerzen(info.benutzer, name, sizeof name, BREITE - 12);
    _u8g2.drawUTF8(12, 23, name);

    // Zeile 2: Energie
    if (info.energie_Wh < 1000.0f) snprintf(wert, sizeof wert, "%.1f Wh", info.energie_Wh);
    else                           snprintf(wert, sizeof wert, "%.2f kWh", info.energie_Wh / 1000.0f);
    _u8g2.drawUTF8(0, 35, "Energie");
    textRechts(BREITE - 1, 35, wert);

    // Zeile 3: wie lange es schon steckt
    const unsigned long s = _belegung.steckdauer_s(platz);
    snprintf(wert, sizeof wert, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
    _u8g2.drawUTF8(0, 47, "Dauer");
    textRechts(BREITE - 1, 47, wert);
  } else {
    _u8g2.drawUTF8(0, 23, z == Zustand::FREI ? "Kein Handy angesteckt" : "Sensor-Fehler");
    _u8g2.drawUTF8(0, 35, "Energie");
    textRechts(BREITE - 1, 35, "-");
    _u8g2.drawUTF8(0, 47, "Dauer");
    textRechts(BREITE - 1, 47, "-");
  }

  // Knoepfe
  zeichneKnopf(0,  "Abmelden", _knopf == Knopf::ABMELDEN, abmeldenMoeglich(platz));
  zeichneKnopf(66, "Zurück",   _knopf == Knopf::ZURUECK,  true);
}

void Menue::zeichneKnopf(int x, const char *text, bool gewaehlt, bool aktiv) {
  _u8g2.setDrawColor(1);
  if (gewaehlt) {
    _u8g2.drawRBox(x, KNOPF_Y, KNOPF_W, KNOPF_H, 3);
    _u8g2.setDrawColor(0);
  } else if (aktiv) {
    _u8g2.drawRFrame(x, KNOPF_Y, KNOPF_W, KNOPF_H, 3);
  } else {                                           // nicht waehlbar: gepunkteter Rahmen
    for (int i = 0; i < KNOPF_W; i += 2) {
      _u8g2.drawPixel(x + i, KNOPF_Y);
      _u8g2.drawPixel(x + i, KNOPF_Y + KNOPF_H - 1);
    }
    for (int i = 0; i < KNOPF_H; i += 2) {
      _u8g2.drawPixel(x, KNOPF_Y + i);
      _u8g2.drawPixel(x + KNOPF_W - 1, KNOPF_Y + i);
    }
  }
  _u8g2.drawUTF8(x + (KNOPF_W - _u8g2.getUTF8Width(text)) / 2, KNOPF_Y + 11, text);
  _u8g2.setDrawColor(1);
}

// --- Hinweis (Overlay ueber dem Hauptmenue) -----------------------------------------------
void Menue::zeichneHinweis() {
  _u8g2.setFont(FONT_TEXT);
  const int w = _u8g2.getUTF8Width(_hinweisText) + 16;
  const int x = (BREITE - w) / 2, y = 20, h = 24;
  _u8g2.setDrawColor(0);
  _u8g2.drawBox(x - 2, y - 2, w + 4, h + 4);
  _u8g2.setDrawColor(1);
  _u8g2.drawRFrame(x, y, w, h, 3);
  _u8g2.drawUTF8(x + 8, y + 16, _hinweisText);
}