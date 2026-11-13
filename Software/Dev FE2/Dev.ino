#include <Arduino.h>
#include "Konfig.h"
#include "Belegungserkennung.h"
#include "Menue.h"

Belegungserkennung belegung;
Menue menue(belegung);

static const char *ereignisText(Ereignis e) {
  switch (e) {
    case Ereignis::ANGESTECKT:    return "angesteckt";
    case Ereignis::ABGEZOGEN:     return "abgezogen";
    case Ereignis::SENSOR_AUS:    return "Sensor aus";
    case Ereignis::SENSOR_WIEDER: return "Sensor wieder da";
    default:                      return "keines";
  }
}

void ereignis(Ereignis e, const Meldung &m) {
  Serial.printf("Ereignis: %s, Platz %u, Steckdauer %lu s\n",
                ereignisText(e), m.platz, (unsigned long)m.steckdauer_s);
}


void abmelden(uint8_t platz) {
  Serial.printf("Abmelden: Platz %u\n", platz);
}

void setup() {
  Serial.begin(115200);

  menue.starten();                              
  menue.beiAbmelden(abmelden);

  menue.startFortschritt(30, "Sensoren");
  belegung.beiEreignis(ereignis);
  belegung.starten();

  // menue.startFortschritt(60, "WLAN");     ... WLAN verbinden ...
  // menue.startFortschritt(85, "Server");   ... Server erreichen ...
  menue.startFortschritt(90, "Fertigstellen");

  menue.startAbschliessen();                    
}

void loop() {
  menue.aktualisieren();                       

  // Debug-Ausgabe
  static uint32_t letzte_ms = 0;
  static uint8_t  count = 0;
  if (millis() - letzte_ms >= 5000) {
    letzte_ms = millis();
    const Mittelwert w = belegung.letzterMittelwert(1);
    Serial.printf("M: %.2f V, %.1f mA, %.2f W | Z: %s | S: %lu s\n", w.spannung_V, w.strom_mA, w.leistung_W,
                  Stellplatz::zustandText(belegung.zustand(1)), (unsigned long)belegung.steckdauer_s(1));
    if (++count >= 12) {
      count = 0;
      const Meldung m = belegung.letzteMeldung(1);
      Serial.printf("Meldung: %s, %.2f V, %.1f mA, %lu s\n", Stellplatz::zustandText(m.zustand),
                    m.werte.spannung_V, m.werte.strom_mA, (unsigned long)m.steckdauer_s);
    }
  }

  delay(2);
}
