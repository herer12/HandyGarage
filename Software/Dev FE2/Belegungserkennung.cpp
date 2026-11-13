#include "Belegungserkennung.h"
#include <Wire.h>

namespace {
// Mutex nur fuer die Dauer eines Blocks halten
struct Sperre {
  SemaphoreHandle_t m;
  explicit Sperre(SemaphoreHandle_t mutex) : m(mutex) { xSemaphoreTake(m, portMAX_DELAY); }
  ~Sperre() { xSemaphoreGive(m); }
  Sperre(const Sperre &) = delete;
  Sperre &operator=(const Sperre &) = delete;
};
}  // namespace

// ---------------------------------------------------------------------------------
//  Start
// ---------------------------------------------------------------------------------
bool Belegungserkennung::starten() {
  if (_task) return true;                                  // laeuft schon

  _mutex = xSemaphoreCreateMutex();
  if (!_mutex) return false;

  Wire.begin(Konfig::PIN_SDA, Konfig::PIN_SCL);
  Wire.setClock(Konfig::I2C_TAKT_HZ);
  Wire.setTimeOut(Konfig::I2C_TIMEOUT_MS);

  const uint32_t jetzt = millis();
  for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
    _plaetze[i] = new Stellplatz(i + 1, Konfig::INA_ADRESSE[i]);
    _plaetze[i]->starten(Wire);       // kein Abbruch: Platz startet bei Rueckkehr des Sensors automatisch
    statusAktualisieren(i, jetzt);    // damit die Getter sofort einen gueltigen Zustand liefern
  }

  const BaseType_t ok = xTaskCreatePinnedToCore(taskStart, "Belegung", Konfig::TASK_STACK_BYTES, this,
                                                Konfig::TASK_PRIORITAET, &_task, Konfig::TASK_KERN);
  return ok == pdPASS;
}

void Belegungserkennung::beiEreignis(EreignisCallback callback) {
  if (!_mutex) _mutex = xSemaphoreCreateMutex();           // Callback darf vor starten() gesetzt werden
  Sperre s(_mutex);
  _callback = std::move(callback);
}

// ---------------------------------------------------------------------------------
//  Getter (liefern Kopien, damit die Task nie waehrend des Lesens ueberschreibt)
// ---------------------------------------------------------------------------------
Zustand Belegungserkennung::zustand(uint8_t platz) const {
  if (!_mutex || !indexGueltig(platz)) return Zustand::FEHLER;
  Sperre s(_mutex);
  return _status[platz - 1].zustand;
}

Messwert Belegungserkennung::letzteMessung(uint8_t platz) const {
  if (!_mutex || !indexGueltig(platz)) return Messwert();
  Sperre s(_mutex);
  return _status[platz - 1].letzte;
}

Mittelwert Belegungserkennung::letzterMittelwert(uint8_t platz) const {
  if (!_mutex || !indexGueltig(platz)) return Mittelwert();
  Sperre s(_mutex);
  return _status[platz - 1].mittelwert;
}

uint32_t Belegungserkennung::steckdauer_s(uint8_t platz) const {
  if (!_mutex || !indexGueltig(platz)) return 0;
  Sperre s(_mutex);
  return _status[platz - 1].steckdauer_s;
}

Meldung Belegungserkennung::letzteMeldung(uint8_t platz) const {
  if (!_mutex || !indexGueltig(platz)) return Meldung();
  Sperre s(_mutex);
  return _status[platz - 1].meldung;
}

// ---------------------------------------------------------------------------------
//  Messtask
// ---------------------------------------------------------------------------------
void Belegungserkennung::taskStart(void *self) {
  static_cast<Belegungserkennung *>(self)->taskSchleife();
}

void Belegungserkennung::taskSchleife() {
  TickType_t weckzeit = xTaskGetTickCount();
  uint32_t   zyklus   = 0;

  for (;;) {
    vTaskDelayUntil(&weckzeit, pdMS_TO_TICKS(Konfig::MESS_INTERVALL_MS));   // driftfrei im 0,5-s-Takt
    const uint32_t jetzt = millis();
    ++zyklus;

    // 1) Messung alle 0,5 s (nur Sensor-Ereignisse, keine Messwerte)
    for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
      const Ereignis e = _plaetze[i]->messen(jetzt);
      statusAktualisieren(i, jetzt);
      if (e != Ereignis::KEINES) ereignisMelden(i, e, jetzt);
    }

    // 2) Durchschnitt alle 5 s -> entscheidet ueber frei/belegt
    if (zyklus % Konfig::MITTEL_ZYKLEN == 0) {
      for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
        Ereignis e = Ereignis::KEINES;
        _plaetze[i]->mittelwertBilden(jetzt, e);
        statusAktualisieren(i, jetzt);
        if (e != Ereignis::KEINES) ereignisMelden(i, e, jetzt);
      }
    }

    // 3) Meldung alle 60 s (nach dem Durchschnitt, damit dessen Werte aktuell sind)
    if (zyklus % Konfig::MELDUNG_ZYKLEN == 0) {
      for (uint8_t i = 0; i < Konfig::ANZAHL_PLAETZE; ++i) {
        const Meldung m = _plaetze[i]->meldungErstellen(jetzt);
        Sperre s(_mutex);
        _status[i].meldung = m;
      }
      zyklus = 0;                     // MELDUNG_ZYKLEN ist ein Vielfaches von MITTEL_ZYKLEN
    }
  }
}

// Stellplatz-Zustand in den Status kopieren (nur die Messtask und starten() rufen das auf)
void Belegungserkennung::statusAktualisieren(uint8_t index, uint32_t jetzt) {
  const Stellplatz &p = *_plaetze[index];
  Sperre s(_mutex);
  Status &st      = _status[index];
  st.zustand      = p.zustand();
  st.letzte       = p.letzteMessung();
  st.mittelwert   = p.letzterMittelwert();
  st.steckdauer_s = p.steckdauer_s(jetzt);
}

// Callback ausserhalb des Mutex aufrufen, damit er die Getter benutzen darf
void Belegungserkennung::ereignisMelden(uint8_t index, Ereignis e, uint32_t jetzt) {
  const Meldung m = _plaetze[index]->meldungErstellen(jetzt);
  EreignisCallback cb;
  {
    Sperre s(_mutex);
    cb = _callback;
  }
  if (cb) cb(e, m);
}
