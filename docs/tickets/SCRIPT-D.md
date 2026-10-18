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

## Critères d'acceptation
- [ ] Un `.con` de course représentatif s'exécute avec 0 avertissement « unimplemented command ».
- [x] `SetRaceLaps` change effectivement le nombre de tours requis pour terminer (player and opponent alike).
- [x] Les bornes de vitesse/rattrapage IA proviennent des commandes de catchup quand la mission en donne (`RaceOpponent::SetCatchup`); the built-in band remains the default.
