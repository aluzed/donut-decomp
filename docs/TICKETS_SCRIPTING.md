# Tickets — Scripting (Updated)

**Major discovery:** `.con` scripts are **plain text** (not bytecode). The existing `Commands::RunLine()` parser is directly compatible.

Scripts are in `data3.cab` (CD2), ~120 files organized in `Missions/level0X/`, `EasyRace/`, `HardRace/`, `Pursuit/`, etc.

---

## Phase 1: Extraction and Parser (P0)

### SCRIPT-001 — Extract .con files from ISOs
**Status:** Format confirmed, paths listed
- [ ] Extract `data3.cab` from CD2 with `cabextract`
- [ ] Copy `scripts/` tree to `assets/scripts/`
- [ ] Auto-extraction script in `tools/extract_assets.sh` (updated)

### SCRIPT-002 — Test parser on a real script
- [ ] Load `Missions/level01/M1race.con` via `Commands::RunScript()`
- [ ] Verify the parsing loop reads all lines
- [ ] Log unimplemented commands

---

## Phase 2: P0 Commands — Playable Mission (~18 commands)

These commands are enough to run `M1race.con` and have a basic race mission:

| # | Command | Role |
|---|--------|------|
| 1 | `SelectMission(id)` | Init mission state machine |
| 2 | `SetMissionResetPlayerInCar(loc)` | Respawn point |
| 3 | `SetDynaLoadData(zones)` | Load level zones |
| 4 | `AddStage(num)` | Stage start |
| 5 | `SetHUDIcon(name)` | HUD icon |
| 6 | `AddObjective(type, ...)` | Objective (race, destroy, collect...) |
| 7 | `SetStageTime(secs)` | Stage timer |
| 8 | `AddNPC(name, loc)` | Add NPC |
| 9 | `AddStageVehicle(car, loc, mode, aiScript, driver)` | Add AI vehicle |
| 10 | `CloseStage()` | Stage end |
| 11 | `CloseMission()` | Mission end |
| 12 | `ShowStageComplete()` | Show completion screen |
| 13 | `AddCharacter(name, loc)` | Add character |
| 14 | `InitLevelPlayerVehicle(car, loc, ...)` | Init player vehicle |
| 15 | `PlacePlayerCar(car, loc)` | Place player car |
| 16 | `CreateChaseManager(cop, script, count)` | Create police pursuit |
| 17 | `SetPresentationBitmap(tex)` | Mission briefing image |
| 18 | `EnableTutorialMode(bool)` | Tutorial mode |

### SCRIPT-003 — Implement P0 commands
- [ ] Create `ScriptEngine` class that manages mission state
- [ ] `SelectMission` → init MissionState
- [ ] `AddStage` / `CloseStage` → stage state machine
- [ ] `AddNPC` → spawn Character via Game
- [ ] `AddStageVehicle` → spawn Vehicle with AI behavior
- [ ] Others → store state, apply to world

---

## Phase 3: P1 Commands (~50 commands)

### SCRIPT-004 — Vehicle commands (~30)
Physics tuning: `SetMass`, `SetGasScale`, `SetTopSpeedKmh`, `SetTireGrip`, `SetSuspensionLimit`, etc.

### SCRIPT-005 — Camera commands
`SetAnimatedCameraName`, `SetConversationCam`, `SetCarStartCamera`, etc.

### SCRIPT-006 — Race commands
`SetRaceLaps`, `SetStageAIRaceCatchupParams`, `SetVehicleAIParams`, etc.

---

## Phase 4: P2 Commands (~100 commands)

### SCRIPT-007 — Gags (collectibles)
`GagBegin`, `GagSetPosition`, `GagSetTrigger`, `GagEnd`, etc. (~51 commands)

### SCRIPT-008 — Hit & Run (police)
`EnableHitAndRun`, `SetHitAndRunDecay`, `SetChaseSpawnRate`, etc.

### SCRIPT-009 — Traffic / Pedestrians
`CreateTrafficGroup`, `CreatePedGroup`, `SetMaxTraffic`, etc.

### SCRIPT-010 — UI / HUD
`SetPresentationBitmap`, `ShowHUD`, `SetIrisWipe`, `SetFadeOut`, etc.

---

## Phase 5: Trigger System

### SCRIPT-011 — Trigger volumes
- [ ] Load `Locator2` and `TriggerVolume` from P3D
- [ ] Detect zone entry/exit
- [ ] `ActivateTrigger(name)`, `DeactivateTrigger(name)`

---

## Phase 6: Polish (P3)

### SCRIPT-012 — Purchases / Rewards
`AddPurchaseCarReward`, `SetCoinFee`, `BindReward`, etc.

### SCRIPT-013 — Dialogue / FMV
`SetDialogueInfo`, `SetFMVInfo`, `GagPlayFMV`, etc.

### SCRIPT-014 — Bonus missions
`AddBonusMission`, `AddBonusObjective`, etc.

---

## Proposed Architecture

```cpp
class ScriptEngine {
    MissionState _mission;
    std::vector<Stage> _stages;
    Game& _game;

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

The `ScriptEngine` replaces the static `Commands::Run` with a stateful system.
