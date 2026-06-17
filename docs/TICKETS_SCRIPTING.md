# Tickets Scripting — Mise à jour

**Découverte majeure :** Les scripts `.con` sont en **texte clair** (pas de bytecode). Le parser existant `Commands::RunLine()` est directement compatible.

Les scripts sont dans `data3.cab` (CD2), ~120 fichiers organisés en `Missions/level0X/`, `EasyRace/`, `HardRace/`, `Pursuit/`, etc.

---

## Phase 1 : Extraction et parser (P0)

### SCRIPT-001 — Extraire les .con depuis les ISOs
**Statut :** Format confirmé, chemins listés  
**À faire :**
- [ ] Extraire `data3.cab` du CD2 avec `cabextract`
- [ ] Copier l'arborescence `scripts/` dans `assets/scripts/`
- [ ] Script d'extraction automatique dans `tools/extract_assets.sh` (mis à jour)

### SCRIPT-002 — Tester le parser sur un vrai script
**À faire :**
- [ ] Charger `Missions/level01/M1race.con` via `Commands::RunScript()`
- [ ] Vérifier que la boucle de parsing lit toutes les lignes
- [ ] Logger les commandes non implémentées

---

## Phase 2 : Commandes P0 — Mission jouable (~18 commandes)

Ces commandes suffisent pour faire tourner `M1race.con` et avoir une mission race basique :

| # | Commande | Rôle |
|---|----------|------|
| 1 | `SelectMission(id)` | Init state machine mission |
| 2 | `SetMissionResetPlayerInCar(loc)` | Point de respawn |
| 3 | `SetDynaLoadData(zones)` | Charger les zones du niveau |
| 4 | `AddStage(num)` | Début d'étape de mission |
| 5 | `SetHUDIcon(name)` | Icône HUD |
| 6 | `AddObjective(type, ...)` | Objectif (race, destroy, collect...) |
| 7 | `SetStageTime(secs)` | Timer de l'étape |
| 8 | `AddNPC(name, loc)` | Ajouter un PNJ |
| 9 | `AddStageVehicle(car, loc, mode, aiScript, driver)` | Ajouter véhicule IA |
| 10 | `CloseStage()` | Fin d'étape |
| 11 | `CloseMission()` | Fin de mission |
| 12 | `ShowStageComplete()` | Afficher écran de complétion |
| 13 | `AddCharacter(name, loc)` | Ajouter personnage |
| 14 | `InitLevelPlayerVehicle(car, loc, ...)` | Initialiser véhicule joueur |
| 15 | `PlacePlayerCar(car, loc)` | Placer voiture joueur |
| 16 | `CreateChaseManager(cop, script, count)` | Créer poursuite police |
| 17 | `SetPresentationBitmap(tex)` | Image de présentation mission |
| 18 | `EnableTutorialMode(bool)` | Mode tutoriel |

### SCRIPT-003 — Implémenter les commandes P0
**Implémentation :**
- [ ] Créer `ScriptEngine` class qui gère l'état mission
- [ ] `SelectMission` → init MissionState
- [ ] `AddStage` / `CloseStage` → stage state machine
- [ ] `AddNPC` → spawn Character via Game
- [ ] `AddStageVehicle` → spawn Vehicle avec comportement IA
- [ ] Les autres → stocker état, appliquer au monde

---

## Phase 3 : Commandes P1 (~50 commandes)

### SCRIPT-004 — Commandes véhicule (~30)
Tuning physique : `SetMass`, `SetGasScale`, `SetTopSpeedKmh`, `SetTireGrip`, `SetSuspensionLimit`, etc.

### SCRIPT-005 — Commandes caméra
`SetAnimatedCameraName`, `SetConversationCam`, `SetCarStartCamera`, etc.

### SCRIPT-006 — Commandes course
`SetRaceLaps`, `SetStageAIRaceCatchupParams`, `SetVehicleAIParams`, etc.

---

## Phase 4 : Commandes P2 (~100 commandes)

### SCRIPT-007 — Gags (collectibles)
`GagBegin`, `GagSetPosition`, `GagSetTrigger`, `GagEnd`, etc. (~51 commandes)

### SCRIPT-008 — Hit & Run (police)
`EnableHitAndRun`, `SetHitAndRunDecay`, `SetChaseSpawnRate`, etc.

### SCRIPT-009 — Trafic / Piétons
`CreateTrafficGroup`, `CreatePedGroup`, `SetMaxTraffic`, etc.

### SCRIPT-010 — UI / HUD
`SetPresentationBitmap`, `ShowHUD`, `SetIrisWipe`, `SetFadeOut`, etc.

---

## Phase 5 : Trigger system

### SCRIPT-011 — Volumes de déclenchement
- [ ] Charger les `Locator2` et `TriggerVolume` depuis P3D
- [ ] Détecter entrée/sortie de zone
- [ ] `ActivateTrigger(name)`, `DeactivateTrigger(name)`

---

## Phase 6 : Polish (P3)

### SCRIPT-012 — Achats / Récompenses
`AddPurchaseCarReward`, `SetCoinFee`, `BindReward`, etc.

### SCRIPT-013 — Dialogues / FMV
`SetDialogueInfo`, `SetFMVInfo`, `GagPlayFMV`, etc.

### SCRIPT-014 — Bonus missions
`AddBonusMission`, `AddBonusObjective`, etc.

---

## Architecture proposée

```
class ScriptEngine {
    MissionState _mission;       // Mission en cours
    std::vector<Stage> _stages;  // Étapes
    Game& _game;                 // Référence au monde

    void RunFile(const std::string& path);
    void ExecuteCommand(const std::string& name, const std::string& params);
};

class MissionState {
    std::string id;
    std::string playerResetLocator;
    std::vector<std::string> zones;
    bool completed = false;
};

class Stage {
    int index;
    std::string hudIcon;
    std::string objectiveType;
    float timeLimit;
    std::vector<NPCRef> npcs;
    std::vector<VehicleRef> vehicles;
};
```

Le `ScriptEngine` remplace le `Commands::Run` statique actuel par un système avec état.
