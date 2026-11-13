#pragma once
#include <Arduino.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "Konfig.h"
#include "Stellplatz.h"

// ============================================================================
//  FE 1: Strommessung / Belegungserkennung
//
//  Kapselt die komplette Logik, die frueher in loop() stand, und laeuft in einer
//  eigenen FreeRTOS-Task:
//    alle 0,5 s  eine Messung pro Stellplatz
//    alle 5 s    Durchschnitt -> entscheidet ueber frei/belegt
//    alle 60 s   Meldung (letzter Durchschnitt + Zustand + Steckdauer)
//
//  Schnittstellen nach aussen (alles andere ist privat):
//    starten()          Bus, Sensoren und Task starten
//    beiEreignis(cb)    Callback bei Ereignis (angesteckt, abgezogen, Sensor aus/wieder da)
//    Getter             zustand(), letzteMessung(), letzterMittelwert(),
//                       steckdauer_s(), letzteMeldung()   (Platz = 1..ANZAHL_PLAETZE)
//
//  Die Getter sind aus jeder Task heraus aufrufbar (Mutex, es werden Kopien geliefert).
// ============================================================================
class Belegungserkennung {
 public:
  // Wird aus der Messtask aufgerufen -> kurz halten (z. B. Queue befuellen, Flag setzen).
  // Die Meldung enthaelt Platz, neuen Zustand, letzten Mittelwert und Steckdauer
  // (bei ABGEZOGEN: die Dauer dieses Ladevorgangs). Getter duerfen im Callback benutzt werden.
  using EreignisCallback = std::function<void(Ereignis, const Meldung &)>;

  // I2C-Bus initialisieren, Stellplaetze anlegen, Messtask starten.
  // Rueckgabe: Task laeuft. Ein fehlender Sensor ist KEIN Fehler (zustand(platz) == FEHLER,
  // der Platz startet automatisch, sobald der Sensor antwortet).
  bool starten();

  // Callback setzen (auch nach starten() moeglich). nullptr = kein Callback.
  void beiEreignis(EreignisCallback callback);

  Zustand    zustand(uint8_t platz) const;
  Mittelwert letzterMittelwert(uint8_t platz) const;    // letzter 5-s-Durchschnitt
  uint32_t   steckdauer_s(uint8_t platz) const;         // laufende Dauer, bei "frei" die letzte
  Meldung    letzteMeldung(uint8_t platz) const;        // letzte 60-s-Meldung (zeit_ms == 0: noch keine)

 private:
  struct Status {                    // von der Task geschrieben, von den Gettern gelesen
    Zustand    zustand = Zustand::FEHLER;
    Messwert   letzte;
    Mittelwert mittelwert;
    uint32_t   steckdauer_s = 0;
    Meldung    meldung;
  };

  Messwert   letzteMessung(uint8_t platz) const;        // letzte Einzelmessung (max. 0,5 s alt)

  static void taskStart(void *self);
  void        taskSchleife();
  void        statusAktualisieren(uint8_t index, uint32_t jetzt);
  void        ereignisMelden(uint8_t index, Ereignis e, uint32_t jetzt);
  static bool indexGueltig(uint8_t platz) { return platz >= 1 && platz <= Konfig::ANZAHL_PLAETZE; }

  Stellplatz       *_plaetze[Konfig::ANZAHL_PLAETZE] = {};   // nur die Messtask greift darauf zu
  Status            _status[Konfig::ANZAHL_PLAETZE];         // geschuetzt durch _mutex
  EreignisCallback  _callback;                               // geschuetzt durch _mutex
  mutable SemaphoreHandle_t _mutex = nullptr;
  TaskHandle_t      _task = nullptr;
};
