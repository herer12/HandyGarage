#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "Konfig.h"
#include "Stellplatz.h"
#include "Belegungserkennung.h"

// ============================================================================
//  FE 3: Webclient
//
//  Der ESP32-S3 verbindet sich per WLAN mit dem Server und spricht JSON ueber HTTP(S).
//  Alles Langsame (WLAN, TLS, HTTP) laeuft in einer eigenen FreeRTOS-Task auf Core 0.
//  Die Messtask (FE1) und das Menue (FE2) werden nie blockiert.
//
//  Senden:   alle 5 Minuten ein Eintrag pro Stellplatz, sofort bei Ereignis (angesteckt,
//            abgezogen, Sensor aus/wieder da) und bei Abmelden.
//  Puffer:   Ist WLAN oder Server weg, bleiben die Eintraege im Ringpuffer und werden spaeter
//            der Reihe nach nachgesendet. Jeder Eintrag traegt sein Alter ("vor_s"), damit der
//            Server die echte Zeit rechnen kann (keine Uhr im ESP noetig).
//  Fehler:   400/415 -> Eintrag wird verworfen (wuerde nie klappen)
//            401, 404, 405, 429, 5xx, kein Netz -> Eintrag bleibt, Wiederholung mit Backoff
//            (429: Retry-After). Nichts davon fuehrt zu Absturz oder Blockade.
//
//  Dazu kommen Anfragen, auf die jemand wartet (kein Puffer, kein Nachsenden):
//    Benutzerliste holen, Stellplatz zuordnen (200 / 300).
//
//  Zugangsdaten: Zugang.h (nicht im Repository). Endpunkte und Zeiten: Konfig.h.
// ============================================================================

struct Benutzer {
  char id[40]       = "";      // userId (UUID)
  char vorname[16]  = "";      // firstName  (max. 15 Zeichen)
  char nachname[31] = "";      // lastName   (max. 30 Zeichen)
};

enum class ListenStatus : uint8_t { KEINE, WARTET, FERTIG, FEHLER };

enum class ZuordnungStatus : uint8_t {
  KEINE,        // noch nichts angefragt
  WARTET,       // Anfrage laeuft
  ZUGEORDNET,   // Server: 200, Platz gehoert jetzt dem Benutzer
  VERGEBEN,     // Server: 300, Benutzer hat schon einen anderen Platz (info sagt welchen)
  FEHLER        // Server nicht erreichbar oder Fehlerantwort -> Gast verwenden, spaeter nachsenden
};

struct ZuordnungErgebnis {
  ZuordnungStatus status = ZuordnungStatus::KEINE;
  char            info[40] = "";     // bei VERGEBEN z. B. "Garage 1, Platz 2" (falls der Server es liefert)
};

class Webclient {
 public:
  explicit Webclient(Belegungserkennung &belegung);

  // WLAN starten und Task anlegen. Blockiert nicht. Mehrfach aufrufen ist harmlos.
  bool starten();

  // Hilfen fuer den Startbildschirm (blockieren bis Erfolg oder Timeout).
  bool wlanAbwarten(uint32_t timeout_ms);
  bool serverPruefen();                       // GET /health ohne Key, true bei 200

  // --- Daten an den Server (gepuffert) -------------------------------------------------
  // Aus dem Ereignis-Callback von FE1 aufrufen. Ist kurz und blockiert nie.
  void ereignisMelden(Ereignis e, const Meldung &m);
  // Platz wurde abgemeldet (FE2). Wird gepuffert und nachgesendet.
  void abmelden(uint8_t platz);

  // --- Anfragen, auf die gewartet wird (Ergebnis abfragen, nicht blockierend) ---------
  void         benutzerListeAnfordern();
  ListenStatus benutzerListeStatus() const;
  size_t       benutzerListe(Benutzer *ziel, size_t max) const;   // Kopie, Rueckgabe: Anzahl

  // userId == nullptr -> Gast dieses Stellplatzes
  void              zuordnen(uint8_t platz, const char *userId);
  ZuordnungErgebnis zuordnung(uint8_t platz) const;

  // --- Status ---------------------------------------------------------------------------
  bool     wlanVerbunden() const;
  bool     serverErreichbar() const { return _serverOk; }   // letzte Anfrage kam an (egal welcher Code < 500)
  bool     keyAbgelehnt() const { return _keyAbgelehnt; }   // Server antwortete zuletzt mit 401
  size_t   gepuffert() const;                               // wartende Eintraege
  uint32_t verworfen() const { return _verworfen; }         // wegen vollem Puffer verlorene Eintraege

 private:
  enum class EintragArt : uint8_t { MESSUNG, ABMELDEN };

  struct Eintrag {
    uint32_t   seq = 0;
    EintragArt art = EintragArt::MESSUNG;
    uint8_t    platz = 0;
    Ereignis   ereignis = Ereignis::KEINES;
    Zustand    zustand = Zustand::FEHLER;
    float      spannung_V = 0, strom_mA = 0, leistung_W = 0;
    uint32_t   steckdauer_s = 0;
    uint32_t   zeit_ms = 0;                 // millis() beim Entstehen
  };

  struct Antwort {
    int      code = -1;                     // HTTP-Code, <= 0: kein Netz / Timeout
    String   text;
    uint32_t retryAfter_s = 0;
  };

  enum class Urteil : uint8_t { ERFOLG, VERWERFEN, WIEDERHOLEN };

  struct ZuordnungAuftrag {
    bool offen = false;
    char userId[40] = "";                   // leer = Gast
  };

  static void taskStart(void *self);
  void        taskSchleife();

  void   wlanPruefen(uint32_t jetzt);
  void   messungenEinreihen(uint32_t jetzt);
  void   einreihen(const Eintrag &e);
  bool   vorderstenHolen(Eintrag &e) const;
  void   vorderstenEntfernen(uint32_t seq);
  void   puffernSenden(uint32_t jetzt);
  bool   interaktivBearbeiten();

  Antwort anfrage(const char *methode, const char *pfad, const String *body, bool mitKey);
  Urteil  bewerten(int code) const;
  String  eintragAlsJson(const Eintrag &e, uint32_t jetzt) const;
  void    serverStatusNachfuehren(const Antwort &a);

  static const char *zustandName(Zustand z);
  static const char *ereignisName(Ereignis e);
  static bool faellig(uint32_t jetzt, uint32_t ziel) { return (int32_t)(jetzt - ziel) >= 0; }

  Belegungserkennung &_belegung;

  // Ringpuffer (geschuetzt durch _mutex)
  Eintrag  _puffer[Konfig::WEB_PUFFER_EINTRAEGE];
  size_t   _kopf = 0;
  size_t   _anzahl = 0;
  uint32_t _seqNaechste = 1;
  volatile uint32_t _verworfen = 0;

  // Anfragen mit Antwort (geschuetzt durch _mutex)
  ListenStatus     _listeStatus = ListenStatus::KEINE;
  Benutzer         _liste[Konfig::MAX_BENUTZER];
  size_t           _listeAnzahl = 0;
  ZuordnungAuftrag  _auftrag[Konfig::ANZAHL_PLAETZE];
  ZuordnungErgebnis _ergebnis[Konfig::ANZAHL_PLAETZE];

  // Nur die Webclient-Task schreibt diese Werte
  uint32_t _naechsteMessung_ms = 0;
  uint32_t _naechsterVersuch_ms = 0;
  uint32_t _wlanVersuch_ms = 0;
  uint8_t  _fehlversuche = 0;
  volatile bool _serverOk = false;
  volatile bool _keyAbgelehnt = false;
  bool     _warnungUngeprueftAusgegeben = false;

  mutable SemaphoreHandle_t _mutex = nullptr;
  TaskHandle_t _task = nullptr;
};
