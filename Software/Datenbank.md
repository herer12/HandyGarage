# Datenbank   
Stellplatz   

| stellplatzId      | UUID | Prim Key      |
| ----------------- | ---- | ------------- |
| handyGarageNumber | int  | joint unique  |
| stellplatzNumber  | int  | joint unique  |
  
User  

| userId    | UUID       | Prim Key |
| --------- | ---------- | -------- |
| firstName | String(15) |          |
| lastName  | String(30) |          |
  
Timestamp  

| timestampUUID  | UUID | Prim Key |  |
| ------------------ | ---------- | ------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| handyAngestecktZu | Timestamp |  | Ab wann das Handy steckt |
| zugeordneterUser  | UUID | Foreign Key(User.userId) |  |
| angeschlossenBei | UUID  | Foreign Key(Stellplatz.stellplatzId) |  |
| handyAngestecktBis | Timestamp  |  | Bis wann das Handy steckt wenn in den letzten 15 Minuten keine neue Zeitmeldung kam dann wird hier die letzte Zeit gemeldet  |
  
