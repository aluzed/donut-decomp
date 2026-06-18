# Script Format (.con)

## Confirmation

`.con` files are **plain text**, not bytecode. Confirmed by extraction from PC game ISOs.

## Syntax

```
CommandName(param1, param2, ...);
OtherCommand("string", 42);
```

- One command per line (or multiple separated by `;`)
- `//` comments supported
- Parameters: quoted strings, integers, floats
- Empty lines ignored

### Real Example (from `Missions/level01/M1race.con`)

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

## Directory Tree (~120 files)

Scripts are in `data3.cab` (CD2):

```
scripts/
├── Missions/
│   ├── level01/   # Level 1 (Evergreen Terrace)
│   │   ├── M1race.con, M3dest.con, M5evade.con, M6dump.con, M7race.con
│   │   └── sr2_1st.con ... sr3_4th.con  (Street Races)
│   ├── level02/   # Level 2
│   ├── level03/   # Level 3
│   ├── level04/   # Level 4
│   ├── level05/   # Level 5
│   ├── level06/   # Level 6
│   └── level07/   # Level 7
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

## Script Types

| Type | Description | Examples |
|------|-------------|----------|
| **Mx*.con** | Main missions (7 per level) | `M1race.con`, `M3dest.con`, `M5evade.con` |
| **sr*_*.con** | Street Races (bonus races) | `sr2_1st.con` (2nd character, 1st place) |
| **AI_*.con** | Vehicle AI behavior | `AI_1st.con` (1st place, easy race) |
| **Lxcop.con** | Police behavior | `L1cop.con` (level 1 pursuit) |
| **BM*.con** | Bonus missions | `BM1dest.con` |

## Mission Script Lifecycle

```
SelectMission("id")                    # Mission start
SetMissionResetPlayerInCar("locator")  # Reset position
SetDynaLoadData("zones")              # Zones to load

AddStage(0)                           # Stage 0
SetHUDIcon("icon")
AddObjective("type")
SetStageTime(seconds)
AddNPC("name", "locator")
AddStageVehicle("car", "locator", "behaviour", "AI.con")
CloseStage()                          # Stage end

AddStage(1)
...
CloseStage()

CloseMission()                        # Mission end
```

## Integration with Existing Parser

The `Commands::RunLine()` parser (in `src/Scripting/Commands.h`) is already compatible with this format:
- `RunScript(filename)` reads files line by line
- `RunLine(line)` parses `CommandName(params)` and calls `Run(name, params)`
- `Run(name, params)` dispatches to `GameCommands::CommandName(params...)`

**All that's needed is implementing the 250 `GameCommands` methods!**

## Execution Plan

1. **Extract .con files** from CAB → `assets/scripts/`
2. **Implement P0 commands** (~20 commands for a basic mission)
3. **Implement P1 commands** (~50 commands for vehicles, races)
4. **Implement P2 commands** (~100 commands for Hit & Run, AI, UI)
5. **Implement P3 commands** (rest for polish)

### P0 Commands (minimum playable mission)

`SelectMission`, `SetMissionResetPlayerInCar`, `SetDynaLoadData`,
`AddStage`, `SetHUDIcon`, `AddObjective`, `SetStageTime`,
`AddNPC`, `AddStageVehicle`, `PlacePlayerCar`,
`CloseStage`, `CloseMission`, `ShowStageComplete`,
`AddCharacter`, `InitLevelPlayerVehicle`, `EnableTutorialMode`,
`CreateChaseManager`, `SetPresentationBitmap`

→ These 18 commands are enough to run `M1race.con`!
