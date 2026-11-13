#include <Arduino.h>
#include "Konfig.h"
#include "Belegungserkennung.h"

Belegungserkennung belegung;
int count = 0;

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

void setup() {
  Serial.begin(115200);
  belegung.beiEreignis(ereignis);   
  belegung.starten();
}

void loop() {
  delay(5000);

  const Mittelwert w = belegung.letzterMittelwert(1);
  Serial.printf("M: %.2f V, %.1f mA, %.2f W | Z: %s | S: %lu s\n", w.spannung_V, w.strom_mA, w.leistung_W, Stellplatz::zustandText(belegung.zustand(1)),(unsigned long)belegung.steckdauer_s(1));

  if (++count >= 12) {             
    count = 0;
    const Meldung m = belegung.letzteMeldung(1);
    Serial.printf("Meldung: %s, %.2f V, %.1f mA, %lu s\n", Stellplatz::zustandText(m.zustand), m.werte.spannung_V, m.werte.strom_mA, (unsigned long)m.steckdauer_s);
  }
}