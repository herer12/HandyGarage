#pragma once
#include <Arduino.h>

// ============================================================================
//  Alle Einstellungen der Handygarage an EINER Stelle.
//  Board: ESP32-S3 (Pins sind für das DevKitC-1 gewählt - bei Bedarf ändern)
// ============================================================================
namespace Konfig {

// --- Allgemein ---------------------------------------------------------------
constexpr const char* GERAETE_NAME = "Handygarage-1";
constexpr bool        SERIAL_DEBUG = true;   // Messwerte im Serial Monitor ausgeben

// --- I2C-Bus (INA219 x4 + OLED) -----------------------------------------------
constexpr int      PIN_SDA       = 8;
constexpr int      PIN_SCL       = 9;
constexpr uint32_t I2C_TAKT_HZ   = 400000;
constexpr uint8_t  OLED_ADRESSE  = 0x3C;     // SSD1306: meist 0x3C (selten 0x3D)

// --- Stellplätze --------------------------------------------------------------
constexpr uint8_t ANZAHL_PLAETZE = 4;        // Handygarage.cpp ist für 4 Plätze ausgelegt

// I2C-Adresse des INA219 pro Platz (A0/A1-Lötbrücken, alle verschieden!)
constexpr uint8_t INA_ADRESSE[ANZAHL_PLAETZE]      = {0x40, 0x41, 0x44, 0x45};

// GPIO des MOSFETs (IRLB8721), der das Lademodul des Platzes abschaltet. -1 = keiner.
constexpr int8_t  LADESCHALTER_PIN[ANZAHL_PLAETZE] = {10, 11, 12, 13};
constexpr bool    LADESCHALTER_AKTIV_HIGH          = true;   // Gate HIGH = Laden an

// Handy-Erkennung über den Strom (Hysterese: EIN-Schwelle > AUS-Schwelle)
constexpr float BELEGT_AB_mA   = 50.0f;      // ab hier "belegt"
constexpr float FREI_UNTER_mA  = 20.0f;      // darunter wieder "frei"

// --- Rotary-Encoder KY-040 ------------------------------------------------------
constexpr int  PIN_ENC_CLK            = 4;
constexpr int  PIN_ENC_DT             = 5;
constexpr int  PIN_ENC_SW             = 6;
constexpr bool ENCODER_RICHTUNG_UMKEHREN = false;  // true, falls links/rechts vertauscht

// --- Zeiten ---------------------------------------------------------------------
constexpr uint32_t MESS_INTERVALL_MS    = 500;    // Strom/Spannung lesen
constexpr uint32_t ANZEIGE_INTERVALL_MS = 200;    // Display auffrischen
constexpr uint32_t SENDE_INTERVALL_MS   = 5000;   // Daten an den Server senden
constexpr uint32_t SEITE_TIMEOUT_MS     = 15000;  // zurück zur Übersicht

// --- WLAN / Server ---------------------------------------------------------------
// Leer lassen = WLAN und Senden sind deaktiviert, der Rest läuft trotzdem.
constexpr const char* WLAN_SSID     = "";
constexpr const char* WLAN_PASSWORT = "";
// Beispiel: "http://192.168.0.10:3000/api/garage"  (nur http, kein https)
constexpr const char* SERVER_URL    = "";

}  // namespace Konfig
