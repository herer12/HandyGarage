#include "Konfig.h"
#include "Display.h"
#include "Encoder.h"
#include "Strommessung.h"
#include "Stellplatz.h"

Display display;
Encoder encoder;

Strommessung strom1(Konfig::INA_ADRESSE[0]);

Stellplatz platz1(strom1);

Stellplatz* plaetze[Konfig::ANZAHL_PLAETZE] = {
    &platz1
};

// --- Menü-Seiten -------------------------------------------------------------
enum Seite : uint8_t {
    SEITE_UEBERSICHT,       // alle Plätze
    SEITE_PLATZ,            // Detailansicht von einem Platz
    SEITE_HELLIGKEIT,       // Drehen verändert Helligkeit
    ANZAHL_SEITEN
};

Seite seite = SEITE_UEBERSICHT;
uint8_t aktuellerPlatz = 0;
uint8_t helligkeit = 255;

PlatzDaten plaetze[4];

uint32_t letzteMessung = 0;
uint32_t letzteAnzeige = 0;

//TODO: Werte noch nehmen
void messwerteAktualisieren() {
    plaetze[0]->messenUndSpeichern();
}

void seiteZeichnen() {
    switch (seite) {
        case SEITE_UEBERSICHT:
            display.zeigeUebersicht(plaetze, Konfig::ANZAHL_PLAETZE, false);
            break;

        case SEITE_PLATZ:
            display.zeigePlatz(aktuellerPlatz, plaetze[aktuellerPlatz]);
            break;

        case SEITE_HELLIGKEIT:
            display.loeschen();
            display.text(0, 0, "Helligkeit", 1);
            display.roh().drawFastHLine(0, 11, Konfig::WIDTH, SSD1306_WHITE);
            display.balken(0, 24, Konfig::WIDTH, 16, (helligkeit * 100) / 255);
            display.textMitte(46, String((helligkeit * 100) / 255) + " %", 1);
            display.anzeigen();
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
        if (!plaetze[i]->setupStrommessung()) {
            Serial.printf("Stellplatz %u: INA219 nicht gefunden\\n", i + 1);
            delay(1000);
        }
  }

    if (!display.begin()) {
        while (true) delay(1000);
    }
    encoder.begin();

    display.zeigeStart();
    delay(1500);
}

void loop() {
    encoder.update();

    const int  dreh = encoder.drehung();
    const bool kurz = encoder.kurzDruck();
    const bool lang = encoder.langDruck();
    bool neuZeichnen = false;

    // --- Drücken: Menü wechseln ---------------------------------------------
    if (kurz) {                                   // kurz = nächste Seite
        seite = (Seite)((seite + 1) % ANZAHL_SEITEN);
        neuZeichnen = true;
    }
    if (lang) {                                   // lang = zurück zur Übersicht
        seite = SEITE_UEBERSICHT;
        neuZeichnen = true;
    }

    // --- Drehen: je nach Seite ---------------------------------------------
    if (dreh != 0) {
        if (seite == SEITE_PLATZ) {
            const int n = Konfig::ANZAHL_PLAETZE;
            aktuellerPlatz = (uint8_t)((((int)aktuellerPlatz + dreh) % n + n) % n);
        } else if (seite == SEITE_HELLIGKEIT) {
            helligkeit = (uint8_t)constrain((int)helligkeit + dreh * 15, 5, 255);
            display.helligkeit(helligkeit);
        }
        neuZeichnen = true;
    }

    // --- Timeout: zurück zur Übersicht ----------------------------------------
    if (seite != SEITE_UEBERSICHT &&
        millis() - encoder.letzteAktivitaet() > Konfig::SEITE_TIMEOUT_MS) {
        seite = SEITE_UEBERSICHT;
        neuZeichnen = true;
    }

    // --- Messen und Anzeigen -----------------------------------------------------
    if (millis() - letzteMessung >= Konfig::MESS_INTERVALL_MS) {
        letzteMessung = millis();
        messwerteAktualisieren();
    }

    if (neuZeichnen || millis() - letzteAnzeige >= Konfig::ANZEIGE_INTERVALL_MS) {
        letzteAnzeige = millis();
        seiteZeichnen();
    }
}
