#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <functional>
#include "Konfig.h"
#include "Drehgeber.h"
#include "Belegungserkennung.h"

// ============================================================================
//  Menues fuer das OLED (SSD1306 128x64) bedient mit Drehgeber (RE)
//
//    Startbildschirm  Ladebalken, der den echten Start des Systems zeigt
//    Hauptmenue       alle Plaetze mit angesteckt / frei, RE drehen = waehlen, druecken = oeffnen
//    Steckplatz       Benutzer, Energie, Steckdauer + 2 Knoepfe: Abmelden | Zurueck
//
//  Benutzung (siehe Dev.ino):
//    setup():  menue.starten();
//              menue.startFortschritt(40, "Sensoren");  ... je Startschritt
//              menue.startAbschliessen();
//    loop():   menue.aktualisieren();     // oft aufrufen, NICHT mit delay(5000) blockieren
// ============================================================================
class Menue {
 public:
  using AbmeldenCallback = std::function<void(uint8_t platz)>;

  explicit Menue(Belegungserkennung &belegung);

  // Drehgeber + Display starten, Startbildschirm mit 0 % zeigen.
  // Rueckgabe false = Display antwortet nicht (das System laeuft trotzdem weiter).
  bool starten();

  // Startbildschirm: Fortschritt 0..100 und Text des aktuellen Startschritts.
  void startFortschritt(uint8_t prozent, const char *text);

  // System ist oben: Balken auf 100 %, danach wechselt die Anzeige ins Hauptmenue.
  void startAbschliessen();

  // Eingaben lesen, Energie mitzaehlen, Anzeige auffrischen.
  void aktualisieren();

  // Benutzername pro Platz (vom Server / der Anmeldung). Standard: "Gast".
  void benutzerSetzen(uint8_t platz, const char *name);
  void benutzerLoeschen(uint8_t platz);                  // zurueck auf "Gast"

  // Wird aufgerufen, wenn im Steckplatz-Menue "Abmelden" bestaetigt wurde.
  void beiAbmelden(AbmeldenCallback callback);

 private:
  enum class Ansicht : uint8_t { START, HAUPTMENUE, STECKPLATZ };
  enum class Knopf   : uint8_t { ABMELDEN, ZURUECK };

  struct PlatzInfo {
    char     benutzer[48] = "Gast";
    float    energie_Wh   = 0.0f;                 // seit dem Anstecken
    Zustand  zustand      = Zustand::FEHLER;      // zuletzt gesehen (fuer Anstecken-Erkennung)
    uint32_t mittelwertZeit_ms = 0;               // zuletzt eingerechneter 5-s-Mittelwert
  };

  // --- Logik ---
  void energieNachfuehren();
  void eingabe(int8_t drehung, bool klick);
  void scrollAnpassen();
  void abmelden(uint8_t platz);
  bool abmeldenMoeglich(uint8_t platz) const;

  // --- Zeichnen ---
  void zeichnen();
  void zeichneStart();
  void zeichneHauptmenue();
  void zeichneSteckplatz();
  void zeichneHinweis();
  void zeichneKnopf(int x, const char *text, bool gewaehlt, bool aktiv);
  void textRechts(int xRechts, int yBasis, const char *text);
  void kuerzen(const char *quelle, char *ziel, size_t groesse, int maxBreite);
  static const char *statusText(Zustand z);

  Belegungserkennung &_belegung;
  U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C _u8g2;     // 2ND = Wire1
  Drehgeber           _re;
  AbmeldenCallback    _abmelden;

  bool     _displayDa = false;
  Ansicht  _ansicht   = Ansicht::START;
  Knopf    _knopf     = Knopf::ZURUECK;
  uint8_t  _auswahl   = 0;        // 0-basiert, Zeile im Hauptmenue
  uint8_t  _scroll    = 0;        // erste sichtbare Zeile
  uint8_t  _aktPlatz  = 1;        // 1-basiert, geoeffneter Steckplatz

  // Startbildschirm
  uint8_t  _startProzent = 0;
  char     _startText[24] = "Start";
  bool     _startFertig  = false;
  uint32_t _startZeit_ms = 0;

  // Hinweis ("Platz 1 abgemeldet")
  bool     _hinweisAktiv   = false;
  uint32_t _hinweisBis_ms  = 0;
  char     _hinweisText[28] = "";

  bool     _neuZeichnen = true;
  uint32_t _letzteZeichnung_ms = 0;

  PlatzInfo _info[Konfig::ANZAHL_PLAETZE];
};
