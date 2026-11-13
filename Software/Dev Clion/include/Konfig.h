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

// ============================================================================
//  FE 3: Webclient (WLAN, HTTP, JSON)
//  Zugangsdaten (WLAN, Server-Adresse, API-Key) stehen NICHT hier, sondern in Zugang.h
// ============================================================================

// --- Identitaet (entspricht handyGarageNumber / stellplatzNumber in der Datenbank) ---
constexpr uint8_t  GARAGE_NUMMER = 1;                // jede Handygarage hat eine eigene Nummer

// --- Senden ----------------------------------------------------------------------------
constexpr uint32_t WEB_SENDE_INTERVALL_MS = 300000;  // alle 5 Minuten (Aenderungen gehen sofort raus)
constexpr uint16_t WEB_PUFFER_EINTRAEGE   = 120;     // Ringpuffer; ist er voll, faellt der aelteste Eintrag weg

// --- WLAN ------------------------------------------------------------------------------
constexpr uint32_t WLAN_START_TIMEOUT_MS  = 15000;   // so lange wartet der Start auf das WLAN
constexpr uint32_t WLAN_NEUVERSUCH_MS     = 15000;   // alle 15 s neu verbinden, solange WLAN fehlt

// --- HTTP ------------------------------------------------------------------------------
constexpr uint16_t HTTP_VERBINDUNG_TIMEOUT_MS = 5000;
constexpr uint16_t HTTP_ANTWORT_TIMEOUT_MS    = 8000;
constexpr size_t   MAX_ANTWORT_BYTES          = 4096; // groessere Antworten werden verworfen
constexpr uint8_t  MAX_BENUTZER               = 20;   // Laenge der Benutzerliste im ESP

// --- Wiederholung bei Fehlern (Backoff verdoppelt sich bis zum Maximum) ----------------
constexpr uint32_t BACKOFF_START_MS       = 2000;
constexpr uint32_t BACKOFF_MAX_MS         = 300000;
constexpr uint32_t RETRY_AFTER_STANDARD_S = 60;       // falls bei 429 kein Retry-After kommt

// --- Webclient-Task (Core 0 bleibt fuer WLAN, TLS braucht viel Stack) ----------------
constexpr uint32_t WEB_TASK_STACK_BYTES = 16384;
constexpr uint8_t  WEB_TASK_PRIORITAET  = 1;
constexpr uint8_t  WEB_TASK_KERN        = 0;

// --- API-Endpunkte: NOCH FESTZULEGEN (Server.md 5.1) -------------------------------------
// Nur hier aendern, sonst nirgends. "/health" gibt es schon (ohne Key, nur UP/DOWN).
constexpr const char* PFAD_GESUNDHEIT    = "/health";
constexpr const char* PFAD_MESSUNG       = "/api/handygarage/TODO-messung";       // Sensordaten + Zeitmeldung
constexpr const char* PFAD_BENUTZERLISTE = "/api/handygarage/TODO-benutzer";
constexpr const char* PFAD_ZUORDNEN      = "/api/handygarage/TODO-zuordnen";      // 200 oder 300
constexpr const char* PFAD_ABMELDEN      = "/api/handygarage/TODO-abmelden";
constexpr const char* METHODE_MESSUNG       = "POST";
constexpr const char* METHODE_BENUTZERLISTE = "GET";
constexpr const char* METHODE_ZUORDNEN      = "POST";
constexpr const char* METHODE_ABMELDEN      = "POST";

}  // namespace Konfig
