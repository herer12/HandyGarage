#include "Webclient.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>      // Bibliothek "ArduinoJson" Version 7.x (Library Manager)
#include <memory>

#include "Zugang.h"           // WLAN, Server-Adresse, API-Key (Vorlage: Zugang.beispiel.h)

namespace {
constexpr uint8_t kPlaetze = Konfig::ANZAHL_PLAETZE;
bool platzGueltig(uint8_t platz) { return platz >= 1 && platz <= kPlaetze; }
bool nutztHttps() { return strncmp(Zugang::SERVER_BASIS, "https://", 8) == 0; }
}  // namespace

// ----------------------------------------------------------------------------------------
//  Aufbau
// ----------------------------------------------------------------------------------------
Webclient::Webclient(Belegungserkennung &belegung) : _belegung(belegung) {}

bool Webclient::starten() {
  if (_task) return true;

  _mutex = xSemaphoreCreateMutex();
  if (!_mutex) return false;

  WiFi.persistent(false);               // Zugangsdaten nicht in den Flash schreiben
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(Zugang::WLAN_SSID, Zugang::WLAN_PASSWORT);
  _wlanVersuch_ms = millis();

  // Erste Meldung kurz nach dem Start, danach alle WEB_SENDE_INTERVALL_MS
  _naechsteMessung_ms = millis() + 10000;

  const BaseType_t ok = xTaskCreatePinnedToCore(taskStart, "webclient", Konfig::WEB_TASK_STACK_BYTES, this,
                                                Konfig::WEB_TASK_PRIORITAET, &_task, Konfig::WEB_TASK_KERN);
  return ok == pdPASS;
}

bool Webclient::wlanAbwarten(uint32_t timeout_ms) {
  const uint32_t start = millis();
  while (!wlanVerbunden() && millis() - start < timeout_ms) delay(100);
  return wlanVerbunden();
}

bool Webclient::serverPruefen() {
  if (!wlanVerbunden()) return false;
  const Antwort a = anfrage("GET", Konfig::PFAD_GESUNDHEIT, nullptr, false);
  serverStatusNachfuehren(a);
  return a.code == 200;
}

bool Webclient::wlanVerbunden() const { return WiFi.status() == WL_CONNECTED; }

size_t Webclient::gepuffert() const {
  if (!_mutex) return 0;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const size_t n = _anzahl;
  xSemaphoreGive(_mutex);
  return n;
}


void Webclient::ereignisMelden(Ereignis e, const Meldung &m) {
  if (e == Ereignis::KEINES || !platzGueltig(m.platz) || !_mutex) return;

  Eintrag eintrag;
  eintrag.art = EintragArt::MESSUNG;
  eintrag.platz = m.platz;
  eintrag.ereignis = e;
  eintrag.zustand = m.zustand;
  eintrag.spannung_V = m.werte.spannung_V;
  eintrag.strom_mA = m.werte.strom_mA;
  eintrag.leistung_W = m.werte.leistung_W;
  eintrag.steckdauer_s = m.steckdauer_s;
  eintrag.zeit_ms = millis();
  einreihen(eintrag);
}

void Webclient::abmelden(uint8_t platz) {
  if (!platzGueltig(platz) || !_mutex) return;

  Eintrag eintrag;
  eintrag.art = EintragArt::ABMELDEN;
  eintrag.platz = platz;
  eintrag.zeit_ms = millis();
  einreihen(eintrag);
}

void Webclient::benutzerListeAnfordern() {
  if (!_mutex) return;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _listeStatus = ListenStatus::WARTET;
  xSemaphoreGive(_mutex);
}

ListenStatus Webclient::benutzerListeStatus() const {
  if (!_mutex) return ListenStatus::KEINE;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const ListenStatus s = _listeStatus;
  xSemaphoreGive(_mutex);
  return s;
}

size_t Webclient::benutzerListe(Benutzer *ziel, size_t max) const {
  if (!_mutex || !ziel) return 0;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const size_t n = (_listeAnzahl < max) ? _listeAnzahl : max;
  for (size_t i = 0; i < n; i++) ziel[i] = _liste[i];
  xSemaphoreGive(_mutex);
  return n;
}

void Webclient::zuordnen(uint8_t platz, const char *userId) {
  if (!platzGueltig(platz) || !_mutex) return;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ZuordnungAuftrag &a = _auftrag[platz - 1];
  a.offen = true;
  if (userId) strlcpy(a.userId, userId, sizeof(a.userId)); else a.userId[0] = '\0';
  _ergebnis[platz - 1] = ZuordnungErgebnis();
  _ergebnis[platz - 1].status = ZuordnungStatus::WARTET;
  xSemaphoreGive(_mutex);
}

ZuordnungErgebnis Webclient::zuordnung(uint8_t platz) const {
  if (!platzGueltig(platz) || !_mutex) return ZuordnungErgebnis();
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const ZuordnungErgebnis e = _ergebnis[platz - 1];
  xSemaphoreGive(_mutex);
  return e;
}

// ----------------------------------------------------------------------------------------
//  Ringpuffer
// ----------------------------------------------------------------------------------------
void Webclient::einreihen(const Eintrag &e) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_anzahl == Konfig::WEB_PUFFER_EINTRAEGE) {       // voll: aeltesten opfern
    _kopf = (_kopf + 1) % Konfig::WEB_PUFFER_EINTRAEGE;
    _anzahl--;
    _verworfen = _verworfen + 1;
  }
  const size_t pos = (_kopf + _anzahl) % Konfig::WEB_PUFFER_EINTRAEGE;
  _puffer[pos] = e;
  _puffer[pos].seq = _seqNaechste++;
  _anzahl++;
  xSemaphoreGive(_mutex);
}

bool Webclient::vorderstenHolen(Eintrag &e) const {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const bool da = _anzahl > 0;
  if (da) e = _puffer[_kopf];
  xSemaphoreGive(_mutex);
  return da;
}

// Entfernt den vordersten Eintrag nur, wenn es noch derselbe ist (er koennte inzwischen
// wegen vollem Puffer verdraengt worden sein).
void Webclient::vorderstenEntfernen(uint32_t seq) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_anzahl > 0 && _puffer[_kopf].seq == seq) {
    _kopf = (_kopf + 1) % Konfig::WEB_PUFFER_EINTRAEGE;
    _anzahl--;
  }
  xSemaphoreGive(_mutex);
}

// ----------------------------------------------------------------------------------------
//  Task
// ----------------------------------------------------------------------------------------
void Webclient::taskStart(void *self) {
  static_cast<Webclient *>(self)->taskSchleife();
  vTaskDelete(nullptr);
}

void Webclient::taskSchleife() {
  for (;;) {
    const uint32_t jetzt = millis();

    wlanPruefen(jetzt);

    if (faellig(jetzt, _naechsteMessung_ms)) {
      messungenEinreihen(jetzt);
      _naechsteMessung_ms = jetzt + Konfig::WEB_SENDE_INTERVALL_MS;
    }

    if (wlanVerbunden()) {
      // Wer gerade wartet (Benutzerliste, Zuordnung), kommt zuerst dran.
      if (!interaktivBearbeiten()) puffernSenden(jetzt);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void Webclient::wlanPruefen(uint32_t jetzt) {
  if (wlanVerbunden()) return;
  _serverOk = false;
  if (!faellig(jetzt, _wlanVersuch_ms + Konfig::WLAN_NEUVERSUCH_MS)) return;
  _wlanVersuch_ms = jetzt;
  Serial.println("Webclient: WLAN fehlt, neuer Versuch");
  WiFi.disconnect();
  WiFi.begin(Zugang::WLAN_SSID, Zugang::WLAN_PASSWORT);
}

// Alle 5 Minuten: der letzte bekannte Stand jedes Stellplatzes
void Webclient::messungenEinreihen(uint32_t /*jetzt*/) {
  for (uint8_t platz = 1; platz <= kPlaetze; platz++) {
    const Meldung m = _belegung.letzteMeldung(platz);
    if (m.zeit_ms == 0) continue;                      // noch keine Meldung von FE1

    Eintrag e;
    e.art = EintragArt::MESSUNG;
    e.platz = platz;
    e.ereignis = Ereignis::KEINES;
    e.zustand = m.zustand;
    e.spannung_V = m.werte.spannung_V;
    e.strom_mA = m.werte.strom_mA;
    e.leistung_W = m.werte.leistung_W;
    e.steckdauer_s = _belegung.steckdauer_s(platz);
    e.zeit_ms = m.zeit_ms;
    einreihen(e);
  }
}

// Sendet den vordersten Eintrag. Klappt es, geht es beim naechsten Durchlauf gleich weiter.
void Webclient::puffernSenden(uint32_t jetzt) {
  if (!faellig(jetzt, _naechsterVersuch_ms)) return;

  Eintrag e;
  if (!vorderstenHolen(e)) return;

  const String body = eintragAlsJson(e, jetzt);
  const bool abmeldung = (e.art == EintragArt::ABMELDEN);
  const Antwort a = anfrage(abmeldung ? Konfig::METHODE_ABMELDEN : Konfig::METHODE_MESSUNG,
                            abmeldung ? Konfig::PFAD_ABMELDEN : Konfig::PFAD_MESSUNG, &body, true);
  serverStatusNachfuehren(a);

  switch (bewerten(a.code)) {
    case Urteil::ERFOLG:
      vorderstenEntfernen(e.seq);
      _fehlversuche = 0;
      _naechsterVersuch_ms = jetzt;                    // gleich den naechsten
      break;

    case Urteil::VERWERFEN:
      Serial.printf("Webclient: Eintrag verworfen, Server meldet %d\n", a.code);
      vorderstenEntfernen(e.seq);
      _fehlversuche = 0;
      _naechsterVersuch_ms = jetzt;
      break;

    case Urteil::WIEDERHOLEN: {
      uint32_t warten_ms;
      if (a.code == 429) {
        const uint32_t s = a.retryAfter_s ? a.retryAfter_s : Konfig::RETRY_AFTER_STANDARD_S;
        warten_ms = s * 1000UL;
      } else {
        const uint8_t schritt = (_fehlversuche < 8) ? _fehlversuche : 8;
        warten_ms = Konfig::BACKOFF_START_MS << schritt;
        if (warten_ms > Konfig::BACKOFF_MAX_MS) warten_ms = Konfig::BACKOFF_MAX_MS;
        if (_fehlversuche < 255) _fehlversuche++;
      }
      Serial.printf("Webclient: Code %d, naechster Versuch in %lu s (%u gepuffert)\n", a.code,
                    (unsigned long)(warten_ms / 1000), (unsigned)gepuffert());
      _naechsterVersuch_ms = jetzt + warten_ms;
      break;
    }
  }
}

// Gibt true zurueck, wenn eine Anfrage bearbeitet wurde (dann kein Puffer-Senden in diesem Durchlauf).
bool Webclient::interaktivBearbeiten() {
  // --- Benutzerliste -----------------------------------------------------------------
  xSemaphoreTake(_mutex, portMAX_DELAY);
  const bool listeOffen = (_listeStatus == ListenStatus::WARTET);
  xSemaphoreGive(_mutex);

  if (listeOffen) {
    const Antwort a = anfrage(Konfig::METHODE_BENUTZERLISTE, Konfig::PFAD_BENUTZERLISTE, nullptr, true);
    serverStatusNachfuehren(a);

    Benutzer neu[Konfig::MAX_BENUTZER];
    size_t n = 0;
    bool ok = false;

    if (a.code == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, a.text) == DeserializationError::Ok && doc.is<JsonArray>()) {
        for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
          if (n >= Konfig::MAX_BENUTZER) break;
          const char *id = o["userId"] | "";
          if (!id[0]) continue;                          // ohne Id nicht zuordenbar
          strlcpy(neu[n].id, id, sizeof(neu[n].id));
          strlcpy(neu[n].vorname, o["firstName"] | "", sizeof(neu[n].vorname));
          strlcpy(neu[n].nachname, o["lastName"] | "", sizeof(neu[n].nachname));
          n++;
        }
        ok = true;
      }
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (ok) {
      for (size_t i = 0; i < n; i++) _liste[i] = neu[i];
      _listeAnzahl = n;
    }
    _listeStatus = ok ? ListenStatus::FERTIG : ListenStatus::FEHLER;
    xSemaphoreGive(_mutex);
    return true;
  }

  // --- Zuordnung ---------------------------------------------------------------------
  for (uint8_t platz = 1; platz <= kPlaetze; platz++) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const ZuordnungAuftrag auftrag = _auftrag[platz - 1];
    xSemaphoreGive(_mutex);
    if (!auftrag.offen) continue;

    JsonDocument doc;
    doc["handyGarageNumber"] = Konfig::GARAGE_NUMMER;
    doc["stellplatzNumber"] = platz;
    if (auftrag.userId[0]) doc["userId"] = auftrag.userId;   // fehlt = Gast
    String body;
    serializeJson(doc, body);

    const Antwort a = anfrage(Konfig::METHODE_ZUORDNEN, Konfig::PFAD_ZUORDNEN, &body, true);
    serverStatusNachfuehren(a);

    ZuordnungErgebnis erg;
    if (a.code == 200) {
      erg.status = ZuordnungStatus::ZUGEORDNET;
    } else if (a.code == 300) {
      erg.status = ZuordnungStatus::VERGEBEN;
      JsonDocument antwort;                            // Info zum belegten Platz, falls vorhanden
      if (deserializeJson(antwort, a.text) == DeserializationError::Ok) {
        const int g = antwort["handyGarageNumber"] | 0;
        const int s = antwort["stellplatzNumber"] | 0;
        if (g > 0 && s > 0) snprintf(erg.info, sizeof(erg.info), "Garage %d, Platz %d", g, s);
      }
    } else {
      erg.status = ZuordnungStatus::FEHLER;            // FE2 nimmt dann den Gast
    }

    xSemaphoreTake(_mutex, portMAX_DELAY);
    _auftrag[platz - 1].offen = false;
    _ergebnis[platz - 1] = erg;
    xSemaphoreGive(_mutex);
    return true;
  }

  return false;
}

// ----------------------------------------------------------------------------------------
//  HTTP
// ----------------------------------------------------------------------------------------
Webclient::Antwort Webclient::anfrage(const char *methode, const char *pfad, const String *body, bool mitKey) {
  Antwort a;
  if (!wlanVerbunden()) return a;                      // code = -1

  // Verbindung: HTTPS (Cloudflare Tunnel) oder HTTP (Test im LAN)
  std::unique_ptr<WiFiClient> client;
  if (nutztHttps()) {
    WiFiClientSecure *sicher = new WiFiClientSecure();
    if (Zugang::SERVER_ROOT_CA) {
      sicher->setCACert(Zugang::SERVER_ROOT_CA);
    } else {
      if (!_warnungUngeprueftAusgegeben) {
        Serial.println("Webclient: WARNUNG, Server-Zertifikat wird nicht geprueft (SERVER_ROOT_CA fehlt)");
        _warnungUngeprueftAusgegeben = true;
      }
      sicher->setInsecure();
    }
    client.reset(sicher);
  } else {
    client.reset(new WiFiClient());
  }

  const String url = String(Zugang::SERVER_BASIS) + pfad;
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(Konfig::HTTP_VERBINDUNG_TIMEOUT_MS);
  http.setTimeout(Konfig::HTTP_ANTWORT_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);   // 300 ist eine echte Antwort, kein Umweg
  const char *gesammelt[] = {"Retry-After"};
  http.collectHeaders(gesammelt, 1);

  if (!http.begin(*client, url)) return a;

  http.addHeader("Accept", "application/json");
  if (mitKey) http.addHeader("X-API-Key", Zugang::API_KEY);   // Header, nie in der URL

  int code;
  if (body) {
    http.addHeader("Content-Type", "application/json");
    code = http.sendRequest(methode, (uint8_t *)body->c_str(), body->length());
  } else {
    code = http.sendRequest(methode);
  }
  a.code = code;

  if (code > 0) {
    if (http.hasHeader("Retry-After")) a.retryAfter_s = (uint32_t)http.header("Retry-After").toInt();
    const int groesse = http.getSize();                // -1 = unbekannt (chunked)
    if (groesse < 0 || (size_t)groesse <= Konfig::MAX_ANTWORT_BYTES) a.text = http.getString();
  }
  http.end();
  return a;
}

Webclient::Urteil Webclient::bewerten(int code) const {
  if (code >= 200 && code < 300) return Urteil::ERFOLG;
  if (code == 400 || code == 415) return Urteil::VERWERFEN;   // Eintrag selbst ist unbrauchbar
  return Urteil::WIEDERHOLEN;      // 401, 404, 405, 429, 5xx, kein Netz: Daten behalten
}

void Webclient::serverStatusNachfuehren(const Antwort &a) {
  _serverOk = (a.code > 0 && a.code < 500);
  _keyAbgelehnt = (a.code == 401);
  if (a.code == 401) Serial.println("Webclient: Server lehnt den API-Key ab (401)");
}

// ----------------------------------------------------------------------------------------
//  JSON (VORSCHLAG, bis die Endpunkte festgelegt sind: nur hier anpassen)
//  Feldnamen wie in der Datenbank (handyGarageNumber, stellplatzNumber).
//  "vor_s" = wie lange das Ereignis zurueckliegt, der Server rechnet daraus die echte Zeit.
// ----------------------------------------------------------------------------------------
String Webclient::eintragAlsJson(const Eintrag &e, uint32_t jetzt) const {
  JsonDocument doc;
  doc["handyGarageNumber"] = Konfig::GARAGE_NUMMER;
  doc["stellplatzNumber"] = e.platz;
  doc["vor_s"] = (jetzt - e.zeit_ms) / 1000UL;

  if (e.art == EintragArt::MESSUNG) {
    doc["zustand"] = zustandName(e.zustand);
    doc["ereignis"] = ereignisName(e.ereignis);
    doc["spannung_V"] = serialized(String(e.spannung_V, 2));
    doc["strom_mA"] = serialized(String(e.strom_mA, 1));
    doc["leistung_W"] = serialized(String(e.leistung_W, 2));
    doc["steckdauer_s"] = e.steckdauer_s;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

const char *Webclient::zustandName(Zustand z) {
  switch (z) {
    case Zustand::FREI:   return "frei";
    case Zustand::BELEGT: return "belegt";
    default:              return "fehler";
  }
}

const char *Webclient::ereignisName(Ereignis e) {
  switch (e) {
    case Ereignis::ANGESTECKT:    return "angesteckt";
    case Ereignis::ABGEZOGEN:     return "abgezogen";
    case Ereignis::SENSOR_AUS:    return "sensor_aus";
    case Ereignis::SENSOR_WIEDER: return "sensor_wieder";
    default:                      return "keines";
  }
}
