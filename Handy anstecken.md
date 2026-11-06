# Handy anstecken   
5sek später fragen welcher User man ist.   
Benutzer sieht Liste mit allen Usern man kann einen User auswählen oder ganz unten gibt es als Gast fortfahren.   
  
1. Der Benutzer wählt einen User. Man muss den User bestätigen und dann wird eine User Bestätigung an den Server gesendet.  
    1. Der Server liefert eine 200 ok: heißt der Stellplatz ist jetzt dem User zugeordnet nun können Zeit Aufnahmen gemacht werden   
    2. Der Server liefert 300 vergeben: Es wird eine User ist bereits einem Stellplatz zugeordnet mit Infos welchem Platz zurückgeben. Das muss der User bestätigen. Nach dreißig Sekunden wird der Benutzer als Gast angemeldet. Wenn der User bestätigt hat dann muss er den ganzen Prozess wiederholen.  
2. Der Benutzer wählt Gast: Passiert das gleiche wie bei einem normalen User nur das jede handygarage für jeden Stellplatz einen eigenen Gast hat wodurch praktisch keine 300 kommen soll.   
3. Der Benutzer wählt nichts aus: nach 30 Sekunden ihm wir der Gast zugeordnet   
4. Ein weiteres Handy wird während dem ganzen Prozess angesteckt: Erst wenn das anmelden eines Benutzer fertig ist kommt der nächste dran die Timer werden für Benutzer 2 pausiert   
Wenn der Server in irgendeinem Prozess nicht mehr erreichbar ist dann speichert der esp die Daten mit den Gast Benutzern.   
Der Benutzer anmelden kann immer wiederholt werden.  
Wenn ein Handy nicht mehr steckt wenn innerhalb von 5 Sekunden dann wird der gleiche User genommen   
