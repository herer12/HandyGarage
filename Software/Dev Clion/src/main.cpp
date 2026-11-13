#include <Arduino.h>
#include "Konfig.h"
#include "Belegungserkennung.h"
#include "Menue.h"
#include "Webclient.h"

Belegungserkennung belegung;
Menue menue(belegung);
Webclient web(belegung);                 // FE 3

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
  web.ereignisMelden(e, m);                     // FE 3: kurz, blockiert die Messtask nie
}


void abmelden(uint8_t platz) {
  Serial.printf("Abmelden: Platz %u\n", platz);
  web.abmelden(platz);
}

void setup() {
  Serial.begin(115200);

  menue.starten();
  menue.beiAbmelden(abmelden);

  menue.startFortschritt(30, "Sensoren");
  belegung.beiEreignis(ereignis);
  belegung.starten();

  menue.startFortschritt(60, "WLAN");
  web.starten();
  const uint32_t t0 = millis();
  while (!web.wlanVerbunden() && millis() - t0 < Konfig::WLAN_START_TIMEOUT_MS) {
    menue.aktualisieren();     // Balken und Punkte laufen weiter
    delay(10);
  }
  const bool wlanOk = web.wlanVerbunden();

  menue.startFortschritt(85, "Server");
  if (wlanOk) web.serverPruefen();
  Serial.printf("WLAN: %s, Server: %s\n", wlanOk ? "ok" : "fehlt", web.serverErreichbar() ? "ok" : "fehlt");

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
