# SCRIPT-D — Commandes de course (laps & paramètres IA de rattrapage)

- **Status:** PARTIAL (2026-10-18)
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** AI-RACE
- **Files:** `src/Scripting/GameCommands.cpp:142,261,265,282,421`, `src/Scripting/ScriptEngine.cpp:271-300` (UpdateAI), `_totalLaps` dans `ScriptEngine`

## Contexte
La logique de course de `ScriptEngine` gère déjà checkpoints, laps et une IA rivale simple (`UpdateAI`, `AdvanceCheckpoint`), mais les commandes qui paramètrent le nombre de tours et le comportement de rattrapage de l'IA sont des stubs vides. Le réglage de difficulté défini par les missions est ignoré.

## Approche
- `SetRaceLaps` (`GameCommands.cpp:142`) : câbler vers `ScriptEngine` pour affecter `_totalLaps` (actuellement utilisé en dur dans `AdvanceCheckpoint`).
- `SetVehicleAIParams` (`:261`), `SetStageAIRaceCatchupParams` (`:282`), `SetStageAITargetCatchupParams` (`:265`), `SetStageAIEvadeCatchupParams` (`:421`) : stocker les paramètres de rattrapage (vitesse min/max, bonus/malus de distance) dans `ScriptEngine` et les consommer dans `UpdateAI` (`ScriptEngine.cpp:288-294`) au lieu des constantes codées en dur (6.0 / 20.0 / +3 / -2).
- S'appuyer sur le système IA de course livré par AI-RACE pour brancher ces paramètres.

## Done (2026-10-18)

The legacy `UpdateAI` constants this ticket names (6.0 / 20.0 / +3 / -2) are gone: AI-RACE
replaced that AI with `RaceOpponent`, which drives a real `Vehicle`. The commands are
wired to it instead.

- **`SetRaceLaps(n)`** sets `_totalLaps`. It is reset to 2 by each `AddObjective`, since
  the game's scripts call it inside the objective (`sr1i.mfk`: 5 laps, `sr2i.mfk`: 3).
- **`SetStageAIRaceCatchupParams(vehicle, distance, ahead, level, behind)`** replaces the
  opponent's built-in per-lap rubber band. The deficit in laps is turned into metres with
  the circuit's length, and the boost is interpolated from `ahead` (player `distance` metres
  behind) through `level` to `behind`. The game passes `(80, 0.6, 1.0, 2.0)`. These
  parameter names are **an interpretation**; the data carries none. With an idle player,
  the M1race opponent held x0.60 and took 121.9s for a lap it drives in 109.3s.
- **The opponent can win.** When it completes all the laps first, the mission fails
  ("'snake_v' finished all 1 laps first"). Before this, only the stage timer could end a
  race in defeat.

Left as stubs: `SetVehicleAIParams(vehicle, -51, -50)`, whose two integers have no
meaning I could establish; the target and evade catch-up variants; and the `position`
condition, which needs more than one opponent to mean anything.

## The game's own street race runs (2026-10-18)

`--mission files/extracted/Scripts/missions/level01/sr2i.mfk` runs sr2 as the game ships it:
three AI cars (`honor_v`, `minivanA`, `schoolbu`), eight stage waypoints, three laps. What
it took:

- **Several opponents.** `ScriptEngine` held one `RaceOpponent`. The game's scripts
  declare the cars *before* the race objective and its waypoints, so it was never even
  created. Race cars are now recorded at `AddStageVehicle`, and their drivers are created
  with the circuit, in `CloseMission`. Catch-up parameters are kept by car name until then.
  Any opponent completing every lap first loses the race.
- **The circuit is the waypoints.** The car's start and `race_finish` are added as nodes
  only when the mission gives fewer than two waypoints (M1race's case). On sr2 the finish
  sits on the far carriageway, and threading it in put two hairpins into every lap.
- **Locator nodes stand on the road.** `sr2_waypoint3` hangs 2.5m in the air.
- **The probe ignores cars.** `FindGroundHeight` and `CastRay` take `staticOnly`; the
  circuit probe had taken a parked car's roof for the road.
- **Laps count from the grid.** Each opponent's circuit is re-indexed to start at the
  point nearest the car. Joining at the nearest point but counting from waypoint 0 scored
  the 9s from the grid to the first waypoint as a lap.
- **A failed turn-round counts as a wedge.** Otherwise the bus backed up and stalled at
  the same kerb until the 12s no-progress rescue fired.

Measured over 150s with an idle player (every car on its x0.6/x0.9 "ahead" catch-up):

| car | script | first lap |
|---|---|---|
| `honor_v` | 1200kg, gas 6.0, 125 km/h | 91.7s, one three-point turn at waypoint 3 |
| `minivanA` | 2000kg, gas 4.0, 125 km/h | 92.3s, clean |
| `schoolbu` | 2500kg, gas 3.5, 120 km/h | 115.8s, rescued twice |

The bus is still rescued twice per lap: once hung on a kerb at (-186.5, 4.0, -101.3) where
the route drops off a raised section, and once at the U-turn at waypoint 7, where the
circuit doubles back onto the other carriageway. The game's physics gives every car the
same 0.9 x 0.4 x 2.2m chassis box, which a bus outgrows.

Then, the same day, the player's side:

- **Cars face their locators.** A car-start locator's first data word is its heading, a
  float in radians about +Y (sr2's grid reads 1.571, facing +X down the starting street).
  Every car used to spawn facing +Z, across the street. Facing the right way took the
  first laps to **84.4s / 88.0s / 109.2s**.
- **`PlacePlayerCar("current", locator)`** builds the player's car at the locator.
  "current" is the family sedan, since there is no garage yet. **`PutMFPlayerInCar`**
  seats the player in it.
- **`AddCollectible` inside a race objective** makes the player's checkpoints, in order.
  sr2 gets its own seven (`sr2_waypoint1..6`, `sr2_waypoint8`) in place of six points
  spread over the level's longest path.

Not done: the countdown and the `position` condition.

## Critères d'acceptation
- [ ] Un `.con` de course représentatif s'exécute avec 0 avertissement « unimplemented command ».
- [x] `SetRaceLaps` change effectivement le nombre de tours requis pour terminer (player and opponent alike).
- [x] Les bornes de vitesse/rattrapage IA proviennent des commandes de catchup quand la mission en donne (`RaceOpponent::SetCatchup`); the built-in band remains the default.
