# SCRIPT-D — Commandes de course (laps & paramètres IA de rattrapage)

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Un `.con` de course représentatif s'exécute avec 0 avertissement « unimplemented command ».
- [ ] `SetRaceLaps` change effectivement le nombre de tours requis pour terminer.
- [ ] Les bornes de vitesse/rattrapage IA dans `UpdateAI` proviennent des commandes de catchup et non de constantes codées en dur.
