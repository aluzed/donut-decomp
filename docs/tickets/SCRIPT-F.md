# SCRIPT-F — Commandes Hit & Run / police

- **Status:** DONE (2026-10-21)
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp:183,184,286,295,406-411`, `src/Scripting/ScriptEngine.cpp:408-412` (CreateChaseManager), `src/AI/ChaseManager.h`

## Contexte
`CreateChaseManager` est déjà câblé et crée un `ChaseManager` fonctionnel (`ScriptEngine.cpp:408-412`). En revanche, les commandes qui paramètrent le mode Hit & Run et le taux de spawn des poursuivants sont des stubs vides, donc la jauge de poursuite et la difficulté restent non configurées.

## Approche
- Exposer dans `ChaseManager` les réglages nécessaires (taux de spawn, nombre de poursuivants, décroissance de la jauge) et un setter `SetMeter`.
- Câbler les stubs via `ScriptEngine` -> `_chaseManager` :
  - activation : `EnableHitAndRun` (`GameCommands.cpp:406`), `DisableHitAndRun` (`:286`), `ResetHitAndRun` (`:410`), `SetHitNRun` (`:295`),
  - jauge/décroissance : `SetHitAndRunMeter` (`:407`), `SetHitAndRunDecay` (`:183`), `SetHitAndRunDecayInterior` (`:411`),
  - poursuivants : `SetChaseSpawnRate` (`:408`), `SetNumChaseCars` (`:184`), `KillAllChaseAI` (`:409`).
- La logique de détection « busted » existe déjà (`ScriptEngine::Update`, `ChaseManager::IsBusted`) ; ces commandes doivent uniquement la paramétrer.

## Done (2026-10-21)

| command | scripts | now |
|---|---|---|
| `SetHitAndRunDecay`, `SetNumChaseCars` | 16 (`leveli.mfk`, demos) | read by `HitAndRun` from the level script (HIT-AND-RUN); the commands stay empty |
| `DisableHitAndRun` | 23 | already wired |
| `SetHitNRun()` | 4 calls, all in `e3m1i.mfk` (the E3 demo's mission) | fills the meter: the police at once |
| `SetHitAndRunMeter`, `SetChaseSpawnRate` | only commented out | left empty |
| `EnableHitAndRun`, `KillAllChaseAI`, `ResetHitAndRun`, `SetHitAndRunDecayInterior` | none | left empty |

## Critères d'acceptation
- [x] The level and mission scripts run with no unimplemented-command warning.
- [x] `DisableHitAndRun` switches the meter and the police off for its stage.
      `EnableHitAndRun` is in no script and stays empty.
- [x] The number of chasing cars comes from the level script's `SetNumChaseCars`.
      `SetChaseSpawnRate` is only ever commented out and stays empty.
