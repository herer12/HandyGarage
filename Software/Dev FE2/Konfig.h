#pragma once
#include <Arduino.h>

// ============================================================================
//  FE 1: Strommessung / Belegungserkennung - alle Einstellungen an EINER Stelle
//  Board: ESP32-S3 DevKitC-1
// ============================================================================
namespace Konfig {

// --- I2C-Bus (4x INA219) -------------------------------------------------------
constexpr int      PIN_SDA     = 8;
constexpr int      PIN_SCL     = 9;
constexpr uint32_t I2C_TAKT_HZ = 400000;
constexpr uint16_t I2C_TIMEOUT_MS = 20;       // fehlender Sensor blockiert nie lange

// --- Stellplaetze ------------------------------------------------------------------
constexpr uint8_t ANZAHL_PLAETZE = 4;
// I2C-Adresse des INA219 pro Platz (A0/A1-Loetbruecken, alle verschieden!)
constexpr uint8_t INA_ADRESSE[ANZAHL_PLAETZE] = {0x40, 0x41, 0x44, 0x45};

// --- Belegungserkennung (Hysterese: BELEGT_AB > FREI_UNTER) ---------------------------
// Entscheidend ist der 5-s-DURCHSCHNITT des Stroms, nicht der einzelne Messwert.
// Startwerte, die endgueltigen Grenzwerte werden mit echten Handys bestimmt (S3).
// Gemessen wird auf der 24-V-Seite des Lademoduls.
constexpr float BELEGT_AB_mA   = 50.0f;       // Durchschnitt ab hier -> "belegt"
constexpr float FREI_UNTER_mA  = 20.0f;       // Durchschnitt darunter -> wieder "frei"
constexpr uint8_t AUSFALL_NACH_MESSUNGEN = 3; // so oft hintereinander keine Antwort -> Fehler

// --- Zeiten --------------------------------------------------------------------------
constexpr uint32_t MESS_INTERVALL_MS    = 500;     // eine Messung alle 0,5 s
constexpr uint32_t MITTEL_INTERVALL_MS  = 5000;    // alle 5 s der Durchschnitt
constexpr uint32_t MELDUNG_INTERVALL_MS = 60000;   // alle 60 s eine Meldung

// --- Kalibrierung (gegen ein Multimeter abgleichen) ----------------------------------
constexpr float BUS_FAKTOR      = 1.0f;     // empirisch aus dem Testaufbau
constexpr float BUS_OFFSET_V    = 0.0f;
constexpr float STROM_FAKTOR    = 1.0f;
constexpr float STROM_OFFSET_mA = 0.0f;
constexpr float GENAUIGKEIT_ZIEL = 0.92f;          // Ziel laut FES: 92 %

// Wie viele Messungen bis zum naechsten Durchschnitt zwischengespeichert werden
// (10 bei 0,5 s / 5 s, dazu etwas Reserve).
constexpr uint8_t MAX_MESSUNGEN = (MITTEL_INTERVALL_MS / MESS_INTERVALL_MS) + 2;

// --- Messtask (FreeRTOS) -------------------------------------------------------------
// Die Task laeuft im 0,5-s-Takt; Mittelwert und Meldung werden nach einer festen
// Anzahl Zyklen ausgeloest (kein Zeitdrift, keine verlorenen Messungen).
static_assert(MITTEL_INTERVALL_MS  % MESS_INTERVALL_MS   == 0, "Mittel-Intervall muss ein Vielfaches des Mess-Intervalls sein");
static_assert(MELDUNG_INTERVALL_MS % MITTEL_INTERVALL_MS == 0, "Meldungs-Intervall muss ein Vielfaches des Mittel-Intervalls sein");
constexpr uint32_t MITTEL_ZYKLEN  = MITTEL_INTERVALL_MS  / MESS_INTERVALL_MS;    // 10
constexpr uint32_t MELDUNG_ZYKLEN = MELDUNG_INTERVALL_MS / MESS_INTERVALL_MS;    // 120

constexpr uint32_t TASK_STACK_BYTES = 6144;   // Reserve, weil Callbacks (z. B. Serial.printf) in der Task laufen
constexpr uint8_t  TASK_PRIORITAET  = 2;      // hoeher als loop() (1)
constexpr uint8_t  TASK_KERN        = 1;      // Core 1 (wie loop); Core 0 bleibt fuer WLAN frei

// ============================================================================
//  Menues: OLED SSD1306 128x64 (0,96") + Drehgeber (RE)
// ============================================================================

// --- OLED (I2C) -------------------------------------------------------------------
constexpr int      PIN_OLED_SDA  = 10;
constexpr int      PIN_OLED_SCL  = 11;
constexpr uint8_t  OLED_ADRESSE  = 0x3C;        
constexpr uint32_t OLED_TAKT_HZ  = 400000;

// --- Drehgeber (z. B. KY-040) ---------------------------------------------------------
// GPIO 4/5/6 sind auf dem DevKitC-1 frei (nicht 0, 3, 45, 46 = Strapping-Pins).
constexpr int     PIN_RE_A       = 4;           // CLK
constexpr int     PIN_RE_B       = 5;           // DT
constexpr int     PIN_RE_TASTER  = 6;           // SW (schaltet nach GND)
constexpr uint8_t RE_SCHRITTE_PRO_RASTUNG = 4;  // KY-040: 4, manche Geber: 2 oder 1
constexpr bool    RE_INVERTIEREN = false;       // true, falls die Drehrichtung falsch herum ist
constexpr uint32_t TASTER_ENTPRELL_MS = 30;

// --- Anzeige ------------------------------------------------------------------------------
constexpr const char* PLATZ_NAME = "Platz";     // Text in der Liste: "Platz 1" ... (z. B. "Garage")
constexpr uint32_t ANZEIGE_INTERVALL_MS = 250;  // Auffrischen der Menues
constexpr uint32_t START_ANIMATION_MS   = 80;   // Auffrischen des Startbildschirms
constexpr uint32_t START_MINDESTZEIT_MS = 1000; // Startbildschirm mindestens so lange (0 = genau so lange wie der Start)
constexpr uint32_t HINWEIS_DAUER_MS     = 1500; // "Platz 1 abgemeldet"

}  // namespace Konfig
