#pragma once
#include <Arduino.h>

// ============================================================================
//  FE 1: Strommessung / Belegungserkennung - alle Einstellungen an EINER Stelle
//  Board: ESP32-S3 DevKitC-1
// ============================================================================
namespace Konfig {

// --- I2C-Bus (4x INA219) -------------------------------------------------------
// ACHTUNG: Der ESP32-S3 hat keine GPIO22 (es gibt nur 0-21 und 26-48).
// Deshalb hier die Standard-I2C-Pins des DevKitC-1 (SDA = 8, SCL = 9).
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

}  // namespace Konfig
