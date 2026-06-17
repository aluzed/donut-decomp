# Format de script (.con)

## Confirmation

Les fichiers `.con` sont en **texte clair**, pas du bytecode. Confirmé par extraction des ISOs du jeu PC.

## Syntaxe

```
CommandName(param1, param2, ...);
AutreCommande("string", 42);
```

- Une commande par ligne (ou plusieurs séparées par `;`)
- Commentaires `//` supportés
- Paramètres : strings entre guillemets, entiers, flottants
- Lignes vides ignorées

### Exemple réel (extrait de `Missions/level01/M1race.con`)

```
SelectMission("m1");
SetMissionResetPlayerInCar("m1_carstart");
SetDynaLoadData("l1z7.p3d;l1r6.p3d;l1r7.p3d;");

AddStage(0);
SetHUDIcon("monkey");
AddObjective("race");
SetStageTime(120);
AddNPC("snake", "m1_snake");
AddStageVehicle("snake_v", "m1_snake_carstart", "race", "EasyRace/AI_1st.con", "snake");
CloseStage();

CloseMission();
```

## Arborescence (~120 fichiers)

Les scripts sont dans `data3.cab` (CD2) :

```
scripts/
├── Missions/
│   ├── level01/   # Niveau 1 (Evergreen Terrace)
│   │   ├── M1race.con, M3dest.con, M5evade.con, M6dump.con, M7race.con
│   │   └── sr2_1st.con ... sr3_4th.con  (Street Races)
│   ├── level02/   # Niveau 2
│   ├── level03/   # Niveau 3
│   ├── level04/   # Niveau 4
│   ├── level05/   # Niveau 5
│   ├── level06/   # Niveau 6
│   └── level07/   # Niveau 7
├── EasyRace/
│   └── AI_5th.con
├── HardRace/
│   └── AI_1st.con, AI_2nd.con
├── Pursuit/
│   └── Easycop.con, Hardcop.con, L1cop.con ... L7cop.con
├── Chase/
│   └── EasyChas.con
├── L6Race/
│   └── AI_1st.con ... AI_4th.con
├── Destroy/
│   └── MedDest.con
└── bonus/
    └── bg*.con  (bonus missions)
```

## Types de scripts

| Type | Description | Exemples |
|------|-------------|----------|
| **Mx*.con** | Missions principales (7 par niveau) | `M1race.con`, `M3dest.con`, `M5evade.con` |
| **sr*_*.con** | Street Races (courses bonus) | `sr2_1st.con` (2ᵉ perso, 1ʳᵉ place) |
| **AI_*.con** | Comportement IA véhicules | `AI_1st.con` (1ʳᵉ place, easy race) |
| **Lxcop.con** | Comportement police | `L1cop.con` (poursuite niveau 1) |
| **BM*.con** | Bonus missions | `BM1dest.con` |

## Cycle de vie d'un script de mission

```
SelectMission("id")                    # Début mission
SetMissionResetPlayerInCar("locator")  # Position reset
SetDynaLoadData("zones")              # Zones à charger

AddStage(0)                           # Étape 0
SetHUDIcon("icon")
AddObjective("type")
SetStageTime(seconds)
AddNPC("name", "locator")
AddStageVehicle("car", "locator", "behaviour", "AI.con")
CloseStage()                          # Fin étape

AddStage(1)
...
CloseStage()

CloseMission()                        # Fin mission
```

## Intégration avec le parser existant

Le parser `Commands::RunLine()` (dans `src/Scripting/Commands.h`) est déjà compatible avec ce format :
- `RunScript(filename)` lit les fichiers ligne par ligne
- `RunLine(line)` parse `CommandName(params)` et appelle `Run(name, params)`
- `Run(name, params)` dispatche vers `GameCommands::CommandName(params...)`

**Il suffit d'implémenter les 250 méthodes de `GameCommands` !**

## Plan d'exécution

1. **Extraire les .con** du CAB → `assets/scripts/`
2. **Implémenter les commandes** P0 (~20 commandes pour une mission basique)
3. **Implémenter les commandes** P1 (~50 commandes pour véhicules, courses)
4. **Implémenter les commandes** P2 (~100 commandes pour Hit & Run, IA, UI)
5. **Implémenter les commandes** P3 (reste pour polish)

### Commandes P0 (minimum mission jouable)

`SelectMission`, `SetMissionResetPlayerInCar`, `SetDynaLoadData`,
`AddStage`, `SetHUDIcon`, `AddObjective`, `SetStageTime`,
`AddNPC`, `AddStageVehicle`, `PlacePlayerCar`,
`CloseStage`, `CloseMission`, `ShowStageComplete`,
`AddCharacter`, `InitLevelPlayerVehicle`, `EnableTutorialMode`,
`CreateChaseManager`, `SetPresentationBitmap`

→ Ces 18 commandes suffisent pour jouer `M1race.con` !
