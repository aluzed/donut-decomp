# SCRIPT-F — Commandes Hit & Run / police

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Un `.con` représentatif Hit & Run / police s'exécute avec 0 avertissement « unimplemented command ».
- [ ] `EnableHitAndRun`/`DisableHitAndRun` activent/désactivent réellement le suivi de la jauge dans `ChaseManager`.
- [ ] `SetChaseSpawnRate`/`SetNumChaseCars` influencent le nombre de poursuivants spawnés.
