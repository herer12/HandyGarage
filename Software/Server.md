# Server Modul Einheit

## 1. Überblick
Der Server liegt auf meinem Home Server und ist aus dem Internet erreichbar (Webseite und lesende API), speichert Daten in PostgreSQL und ist so getrennt, dass ein Sicherheitsproblem nicht auf den Rest des Systems übergreift.

**Technik:** Debian 13, rootless Docker (Benutzer `websrv`), nginx, Java 21 mit Spring Boot 4.1.0 und Maven, PostgreSQL 17, Cloudflare Quick Tunnel (`cloudflared`).

**Datenfluss:** Internet → Cloudflare → `cloudflared` (nur ausgehende Verbindung) → nginx (HTML und Reverse Proxy) → Java-API → PostgreSQL.

## 2. Architektur


### 2.1 Docker-Netze

| Netz                      | Teilnehmer         | Internet              |
|---------------------------|--------------------|-----------------------|
| `egress`                  | nur cloudflared    | ja (einziger Ausgang) |
| `tunnel` (172.30.10.0/24) | cloudflared, nginx | nein (intern)         |
| `app`                     | nginx, API         | nein                  |
| `data`                    | API, PostgreSQL    | nein                  |


### 2.2 Docker Container

| Dienst        | Image                                       | Besonderheiten                                     |
|---------------|---------------------------------------------|----------------------------------------------------|
| `db`          | `postgres:17-alpine`                        | Volume `pgdata`, Passwort aus Secret `db_password` |
| `api`         | `webserver-api:0.1.0` (eigener Build)       | Benutzer 10001, Port 8081 nur im Container         |
| `web`         | `nginxinc/nginx-unprivileged:1.30.5-alpine` | Port 8080 nur im Container                         |
| `cloudflared` | `cloudflare/cloudflared:2026.10.0`          | Quick Tunnel auf `http://web:8080`                 |


### 2.3 Zugriffsstufen

| Stufe             | Pfad                   | Erreichbar über                                                    | Schutz                                                                                |
|-------------------|------------------------|--------------------------------------------------------------------|---------------------------------------------------------------------------------------|
| Öffentliche Seite | `/`                    | Tunnel                                                             | nur lesend (GET, HEAD)                                                                |
| Öffentliche Daten | `/api/site/messages`   | Tunnel                                                             | ohne Key, nur Text und Zeit, Rate-Limit                                               |
| Externe API       | `/api/public/...`      | nur im Docker-Netz (nginx sperrt sie nach außen)                   | API-Key pro System, Rate-Limit                                                        |
| Handygarage-API   | `/api/handygarage/...` | noch festzulegen (geplant)                                         | eigene Handygarage-Keys mit eigenen, strengeren Regeln muss noch implementiert werden |
| Interne API       | `/api/internal/...`    | nur Tailscale, nie über den Tunnel (geplant, noch nicht vorhanden) | API-Key                                                                               |


### 2.4 Host

- Debian 13 (trixie), Hostname `ServerPCHaupt`, 7,7 GiB RAM, 908 GB Platte.
- Firewall `ufw`: eingehend alles gesperrt. SSH nur über `tailscale0`, UDP 41641 für Tailscale.
- SSH nur mit Ed25519-Key (Passphrase), nur Benutzer `erik`, `MaxAuthTries 3`.
- Tailscale-IP des Servers: `100.123.47.33`.

## 3. Betrieb im Alltag

**Zugang:** `ssh erik@100.123.47.33` (nur über Tailscale). Dann `sudo machinectl shell websrv@` und `cd ~/webserver`.

| Aufgabe                 | Befehl (in `~/webserver` als `websrv`)                                                                                                |
|-------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| Zustand                 | `docker compose ps`                                                                                                                   |
| Logs                    | `docker compose logs web --tail 20` (auch `api`, `db`, `cloudflared`)                                                                 |
| Starten / Stoppen       | `docker compose up -d` / `docker compose down` (bei `down` gibt es kein `-d`)                                                         |
| Tunnel offline / online | `docker compose stop cloudflared` / `docker compose up -d cloudflared`                                                                |
| Aktuelle Tunnel-Adresse | `docker compose logs cloudflared \| grep -oE 'https://[a-z0-9-]+\\.trycloudflare\\.com' \| tail -n 1`                                 |
| Datenbank-Konsole       | `docker compose exec db psql -U app -d appdb`                                                                                         |
| Einträge ansehen        | `docker compose exec -T db psql -U app -d appdb -c "SELECT id, source_system, left(body, 40), created_at FROM messages ORDER BY id;"` |
| Tabelle leeren          | `docker compose exec -T db psql -U app -d appdb -c "TRUNCATE messages RESTART IDENTITY;"`                                             |
| Namen der API-Keys      | `cut -d= -f1 secrets/api_keys`                                                                                                        |
| Backup sofort           | `./backup/backup.sh`                                                                                                                  |
| Nächster Backup-Lauf    | `systemctl --user list-timers backup.timer --no-pager`                                                                                |


**Notfall: Modul abschalten.** `docker compose stop cloudflared` nimmt die Seite sofort aus dem Internet.

## 4. Java: Regeln und Ist-Zustand

### 4.1 Rahmen und Abhängigkeiten

- Java 21 (LTS). Spring Boot 4.1.x, Maven. Verboten sind Snapshots, Milestones, Release Candidates, Versionsbereiche und `LATEST`.
- Laufzeit-Image `eclipse-temurin:21-jre-alpine` (Tag fest anpinnen, kein `latest`). Im Dockerfile steht aktuell der bewegliche Tag `21-jre-alpine`.
- So wenig Abhängigkeiten wie nötig. Neue brauchen einen Satz Begründung in der Doku, bevor sie in die `pom.xml` kommen. Versionen kommen aus der Spring-Boot-Stückliste, eigene Versionsangaben nur mit Begründung (aktuell nur der PostgreSQL-Treiber `42.7.14`). Nur Maven Central als Quelle.
- Vor jeder Übertragung nach bekannten Schwachstellen suchen (Vorschlag: OWASP Dependency-Check). Das ist bisher nicht eingerichtet.
- Sicherheitspatches von Spring Boot zeitnah übernehmen. Spring Boot 4.0 bekommt laut Richtlinie nur bis Ende 2026 Sicherheitsupdates.

### 4.2 Eingaben von außen

- Standard ist Ablehnen. Eingaben kommen in eigene unveränderliche Klassen (`record`) mit Prüfung. Jedes Textfeld hat eine maximale Länge, jede Zahl einen erlaubten Bereich.
- Unbekannte JSON-Felder werden abgelehnt (400). Das ist nicht getestet und im Code bisher nicht umgesetzt.
- Größe der Anfrage doppelt begrenzen: nginx (`client_max_body_size 1k`) und Feldlängen im Code.
- Keine Datei-Uploads, kein XML. Eingaben bestimmen nie einen Dateipfad, Befehl oder eine aufgerufene URL.

### 4.3 Datenbank

- Nur parametrisierte Abfragen (`?` mit Parametern). SQL nie durch Zusammenkleben von Strings bauen. Im Code ist das überall so umgesetzt.
- Passwort nur aus der Secret-Datei. Fehlermeldungen der Datenbank gehen nie an den Aufrufer.
- Änderungen am Datenbankaufbau laufen über versionierte Dateien im Repo, nicht per Hand.

### 4.4 Zugriffsschutz

- Alles gesperrt, bis es ausdrücklich erlaubt wird. Im Code übernimmt das der `ApiKeyFilter` als Allowlist: Ohne Key laufen nur `GET /api/site/messages` und `GET /health`, exakter Vergleich der URI. Alles andere braucht den Header `X-API-Key`.
- Ein Key pro anrufendem System, einzeln sperrbar (Zeile aus `secrets/api_keys` löschen). Der Key steht nie in einer URL, nie im Code, nie im Log. Verglichen wird mit `MessageDigest.isEqual` (konstante Zeit).
- Fehlender und falscher Key liefern dieselbe Antwort (401), damit der Aufrufer nichts erfährt.
- Keine Sitzungen, keine Cookies (zustandslos). Kein CORS mit Platzhalter `*`.
- Rate-Limit: 60 Anfragen pro Minute und Key, ohne gültigen Key 20 pro Minute und Adresse. Bei Überschreitung 429 mit `Retry-After: 60`. Könnte ich mir auch überlegen das zu begrenzen mit ngix

### 4.5 Fehler, Logs, Health

- Der Aufrufer bekommt nie einen Stacktrace, Klassennamen oder SQL-Meldungen. Eingestellt in `application.yml`: `include-stacktrace`, `include-message`, `include-binding-errors` jeweils `never`.
- Ins Log kommen nie Passwörter, API-Keys, Tokens oder ganze Anfragekörper. Eingaben vor dem Protokollieren von Zeilenumbrüchen und Steuerzeichen befreien (Log-Injection). Kein leerer `catch`-Block, kein `System.out.println` (stattdessen Logger).
- `/health` meldet nur `UP` oder `DOWN` ohne Details (HTTP 503 bei `DOWN`) und prüft mit `SELECT 1` die Datenbank.

### 4.6 Geheimnisse

- Nie im Code, in Properties, im Repo, im Chat oder in einer ZIP. Das DB-Passwort und die API-Keys werden als Docker-Secret-Dateien unter `/run/secrets/` eingehängt und von Spring per `spring.config.import: configtree:/run/secrets/` gelesen. Fehlt eines, startet die API nicht.
- Die JAR muss auch dann unbedenklich sein, wenn jemand sie öffnet. Zum Testen auf dem Laptop eigene Wegwerf-Werte benutzen, nie die Werte vom Server.

### 4.7 Abweichungen des Ist-Zustands von den Richtlinien

| Richtlinie                                                                 | Ist-Zustand                                                                                        |
|----------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------|
| Spring Security von Anfang an                                              | Eigener Servlet-Filter (`ApiKeyFilter`), kein Spring Security in der `pom.xml`                     |
| Keys nur als SHA-256-Hash speichern                                        | Keys stehen im Klartext in `secrets/api_keys` (Format `name=schluessel`), Schutz durch Dateirechte |
| Bean Validation (`@Valid`, `@NotBlank`, `@Size`)                           | Handgeschriebene Prüfung in `PublicController.create` (1 bis 500 Zeichen, zugeschnitten)           |
| `JdbcClient` mit `.param(...)`                                             | `JdbcTemplate` mit `?`-Parametern (ebenfalls parametrisiert)                                       |
| Actuator auf eigenem Port                                                  | Kein Actuator, stattdessen eigener Endpunkt `/health`                                              |
| Aufteilung in Pakete `web`, `service`, `persistence`, `security`, `config` | Alles in einem Paket `de.homelab.api`                                                              |
| Eigener DB-Benutzer mit minimalen Rechten                                  | Die API nutzt den Datenbankbenutzer `app`                                                          |
| Maven Wrapper mit Prüfsumme, OWASP-Prüfung                                 | Nicht im Projekt gefunden                                                                          |
| Fehlertest „interner Fehler ohne Details“, Test auf unbekannte JSON-Felder | Fehlen (bekannte Lücken)                                                                           |

## 5 API

### 5.1 API-Endpunkte
Müssen noch genau definiert werden.


### 5.2 Header und Statuscodes

- Der Key steht im Header `X-API-Key`, nie in der URL (ein Key in der URL zählt nicht).
- Beim Schreiben ist `Content-Type: application/json` Pflicht.

| Code      | Bedeutung                                                                                                                                     |
|-----------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| 200 / 201 | Erfolg beim Lesen / beim Speichern                                                                                                            |
| 400       | Eingabe ungültig, z. B. leer, zu lang, kaputtes JSON. Antwort ohne interne Details, z. B. `{"error":"text muss 1 bis 500 Zeichen lang sein"}` |
| 401       | Key fehlt oder ist falsch (beides dieselbe Antwort `{"error":"unauthorized"}`). Auch jeder unbekannte Pfad ohne Key                           |
| 404       | Pfad unbekannt (mit Key). Über den Tunnel: alles unter `/api/` außer `/api/site/messages` sowie `/health`                                     |
| 405       | Falsche Methode. Über den Tunnel: jede Methode außer GET und HEAD                                                                             |
| 415       | Falscher Content-Type beim Schreiben                                                                                                          |
| 429       | Rate-Limit überschritten, `{"error":"rate limit"}` mit Header `Retry-After: 60`                                                               |
| 503       | `/health`: Datenbank nicht erreichbar                                                                                                         |

### 5.3 Limits

| Wo                          | Limit                                                                                         |
|-----------------------------|-----------------------------------------------------------------------------------------------|
| API, mit gültigem Key       | 60 Anfragen pro Minute und Key                                                                |
| API, ohne gültigen Key      | 20 Anfragen pro Minute und Absenderadresse (hinter nginx ist das immer die Adresse von nginx) |
| nginx, `/api/site/messages` | 30 Anfragen pro Minute und Besucher, Burst 10, dann 429. Bei IPv6 zählt das /64-Netz          |

### 5.4 API-Keys verwalten
Es soll eigene Keys für die Handygarage-API geben. Jede Handygarage hat einen eigenen Key.

## 6. Build, Tests und Ausrollen

### 6.1 Pflicht-Tests je Endpunkt (Richtlinie 11)

| Fall                                                              | Erwartung                           |
|-------------------------------------------------------------------|-------------------------------------|
| Gültige Anfrage mit gültigem Key                                  | Erfolg                              |
| Kein Key                                                          | 401                                 |
| Falscher Key                                                      | 401, gleiche Antwort wie ohne Key   |
| Ungültige Eingabe (leer, zu lang, falscher Typ, unbekanntes Feld) | 400, ohne interne Details           |
| Zu viele Anfragen                                                 | 429                                 |
| Nicht freigegebener Pfad                                          | Abgelehnt                           |
| Interner Fehler                                                   | Allgemeine Antwort, kein Stacktrace |

## 7. Datenbank, Backup und Restore

### 7.1 Backup

- Jede Nacht um 03:30 Uhr (plus bis zu 10 Minuten Zufallsverzögerung) entsteht ein verschlüsselter Dump `backups/appdb-<Zeitstempel>.dump.age`. `Persistent=true` holt einen verpassten Lauf nach dem Start nach.
- `backup/backup.sh` führt `pg_dump -Fc` im Datenbank-Container aus und leitet den Strom direkt in `age` (mit dem öffentlichen Schlüssel aus `recipient.txt`). Ein Klartext-Dump liegt nie auf der Platte. Backups älter als 7 Tage werden gelöscht. Dateirechte `600`.
- Umgesetzt als systemd-Benutzerdienst von `websrv` (`backup.service`, `backup.timer` unter `/home/websrv/.config/systemd/user/`). Prüfen mit `journalctl --user -u backup.service -n 10 --no-pager`, erwartet wird `Backup ok`.
- Der **private** Schlüssel (`AGE-SECRET-KEY-1...`) liegt nur im Passwortmanager und darf nie dauerhaft auf dem Server bleiben. Ohne ihn sind die Backups wertlos.
- **Bekanntes Risiko:** Die Backups liegen auf derselben Platte wie die Datenbank. Die zweite Kopie (1.5.4) ist zurückgestellt bis zum zweiten Server und wird nicht weiterverfolgt.

### 7.2 Restore (Kurzfassung des Restore-Tests)

Ein Backup zählt erst, wenn es sich in eine leere Datenbank laden lässt. Ablauf in Stichworten:

1. Als `erik` den privaten Schlüssel vorübergehend ohne Anzeige und ohne Shell-Historie nach `/home/websrv/webserver/backup-key.tmp` legen (Rechte 600, Besitzer `websrv`).
2. Als `websrv` in `~/webserver`:

```
f=$(ls -t backups/appdb-*.dump.age | head -1)
docker compose exec -T db createdb -U app restore_test
age -d -i backup-key.tmp "$f" | docker compose exec -T db pg_restore -U app -d restore_test --no-owner
docker compose exec -T db psql -U app -d restore_test -c "SELECT count(*) FROM messages;"
```

3. Aufräumen: `dropdb restore_test`, danach `shred -u backup-key.tmp` und mit `ls` prüfen, dass die Datei weg ist.

Im Ernstfall (echte Wiederherstellung) wird statt `restore_test` die Zieldatenbank `appdb` benutzt, nachdem die Dienste gestoppt und die Datenbank leer angelegt wurde. Das ist im Projekt nicht als eigener Ablauf getestet. Der Restore-Test selbst ist laut erik bestanden (09.10.2026), die Ausgaben liegen nicht vor.

## 8. Sicherheit und Secrets
### 8.1 Secrets

| Secret                 | Zweck                                                           | Ablage                                                  |
|------------------------|-----------------------------------------------------------------|---------------------------------------------------------|
| `db_password`          | Datenbank-Passwort (`openssl rand -hex 32`, ohne Zeilenumbruch) | `/home/websrv/webserver/secrets/`                       |
| `api_keys`             | API-Keys, eine Zeile `name=schluessel`                          | `/home/websrv/webserver/secrets/`                       |
| `tunnel_token`         | nur für den geplanten verwalteten Tunnel                        | `/home/websrv/webserver/secrets/` (noch nicht angelegt) |
| privater age-Schlüssel | Backups entschlüsseln                                           | nur Passwortmanager                                     |#


### 8.2 Bekannte Risiken

- Backups liegen nur auf derselben Platte (Plattenausfall oder Diebstahl: Daten und Backups weg).
- Läuft Tailscale auf dem Server nicht, ist er nur noch an der Konsole erreichbar.
- Der Quick Tunnel hat eine wechselnde Adresse und ist nicht verwaltbar. Wer das Cloudflare-Konto kontrolliert, kontrolliert später den Verkehr (2FA aktivieren). Gleiches gilt für das GitHub-Konto (Tailscale-Login hängt daran).
- Das Limit für Anfragen ohne Key gilt hinter nginx für alle gemeinsam (die API sieht nur nginx). Der Schutz pro Besucher kommt aus nginx.
- Die API-Zähler liegen im Speicher, ein Neustart setzt sie zurück.
- Bei Änderungen an SSH oder Firewall immer eine zweite Sitzung offen halten. LAN-Zugriff per SSH ist absichtlich gesperrt.
