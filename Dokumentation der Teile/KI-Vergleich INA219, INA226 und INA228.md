# Vergleich: INA219, INA226 und INA228

Die Sensoren INA219, INA226 und INA228 messen Strom, Spannung und Leistung über einen Shunt-Widerstand und kommunizieren über I²C.

## Übersicht

| Eigenschaft      | INA219   | INA226   | INA228   |
| ---------------- | -------- | -------- | -------- |
| Busspannung      | Bis 26 V | Bis 36 V | Bis 85 V |
| Auflösung        | 12 Bit   | 16 Bit   | 20 Bit   |
| Strom messen     | Ja       | Ja       | Ja       |
| Spannung messen  | Ja       | Ja       | Ja       |
| Leistung messen  | Ja       | Ja       | Ja       |
| Energie / Ladung | Nein     | Nein     | Ja       |
| Schnittstelle    | I²C      | I²C      | I²C      |

## Welcher Sensor ist geeignet?

### INA219

* Einfach und günstig
* Viele fertige Module verfügbar
* Gut für Arduino und ESP32
* Für einfache Strom- und Spannungsmessungen

### INA226

* Höhere Genauigkeit als INA219
* Bis 36 V
* Guter Allrounder
* Geeignet für Netzteile, Akkus und Embedded-Projekte

### INA228

* Sehr hohe Auflösung
* Bis 85 V
* Kann Energie und Ladung erfassen
* Geeignet für präzise Messungen und Batteriemonitoring

## Empfehlung

* **Einfache Projekte:** INA219
* **Genaue Messungen bis 36 V:** INA226
* **Hohe Präzision und bis 85 V:** INA228

> Der maximal messbare Strom hängt auch vom verwendeten Shunt-Widerstand ab.



Geschrieben von Chat-GPT am 14.09.2026