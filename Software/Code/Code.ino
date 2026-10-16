#include "Strommessung.h"
#include "Stellplatz.h"
#include "Konfig.h"
#include "Display.h"

Strommessung strom1(Konfig::INA_ADRESSE[0]);

Stellplatz platz1(strom1);

Stellplatz* plaetze[Konfig::ANZAHL_PLAETZE] = {
    &platz1
};


Display display;

void setup() {
  Serial.begin(115200);
  Wire.begin(Konfig::PIN_SDA, Konfig::PIN_SCL);
  for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
        if (!plaetze[i]->setupStrommessung()) {
            Serial.printf("Stellplatz %u: INA219 nicht gefunden\\n", i + 1);
            delay(1000);
        }
  }

  display.begin();

}


void loop() {
    plaetze[0]->messenUndSpeichern();

    

    for (uint8_t i = 0; i < 5; ++i) {
        display.print("Messwert: " + i);
        display.print(String(i));
        display.print(": ");

        display.print(plaetze[0]->getLoadvoltage()[i]);
        display.print(" V, ");

        display.print(plaetze[0]->getCurrent_A()[i]);
        display.print(" A, ");

        display.print(plaetze[0]->getPower_W()[i]);
        display.println(" W");
        Serial.print(plaetze[0]->getPower_W()[i]);
        Serial.println(" W");
    }

    display.println("--------------------");
    

    

    delay(500);
}

