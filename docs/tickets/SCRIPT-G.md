# SCRIPT-G — Commandes de trafic / piétons

- **Status:** DONE (2026-10-21)
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** REGR-001
- **Files:** `src/Scripting/GameCommands.cpp:51-54,132,198-204,287,420,433`, `src/Scripting/ScriptEngine.cpp:223-226` (UsePedGroup)

## Contexte
`UsePedGroup` est câblé (log seulement, `ScriptEngine.cpp:223`). Toutes les commandes de définition de groupes de trafic et de piétons sont des stubs vides : aucun véhicule de trafic ni piéton n'est réellement spawné selon les définitions de mission.

## Approche
- Ajouter dans `ScriptEngine` des structures pour les groupes de trafic et de piétons (motif builder « Create.../Add.../Close... »), puis un spawner consommant ces groupes.
- Câbler les stubs (`GameCommands.cpp`) :
  - trafic : `CreateTrafficGroup` (`:198`), `AddTrafficModel` (`:199`), `CloseTrafficGroup` (`:200`), `SetMaxTraffic` (`:132`), `SetRespawnRate` (`:433`), `NoTrafficForStage` (`:287`), `ClearTrafficForStage` (`:420`),
  - piétons : `CreatePedGroup` (`:201`), `AddPed` (`:202`), `ClosePedGroup` (`:203`), `UsePedGroup` (déjà câblé `:51`).
- Réutiliser le système de spawn/régression d'acteurs livré par REGR-001 pour instancier réellement les modèles.

## Done (2026-10-21)

What the scripts use, and how:

| command | scripts | now |
|---|---|---|
| `SetMaxTraffic(n)` | 101 (1 to 5) | `ScriptEngine::SetMaxTraffic`; `TrafficManager::SetMaxCars` parks the cars over the limit, bodies out of the world, until it rises. Until a script says, the 12 cars as before |
| `CreatePedGroup`/`AddPed`/`ClosePedGroup` | 16 (`leveli.mfk`, demos) | read by `PedestrianManager::LoadModels`: each group's people, a name as many times as its count |
| `UsePedGroup(n)` | 136 | `PedestrianManager::UseGroup`: that group's bodies built, the people about cleared to come back as its own. Level 1 has 7 groups with extracted models |
| `CreateTrafficGroup`/`AddTrafficModel`/`CloseTrafficGroup` | 16 | read by `TrafficManager::LoadModels` from `leveli.mfk` (one group, 0, in every level): the commands stay empty |
| `NoTrafficForStage` | 21 | already wired |
| `ClearTrafficForStage`, `SetRespawnRate` | none | left empty |

Seen in `val.sh`: "2 of 12 cars driving" (level 1's `SetMaxTraffic(2)`), groups 0 to 6 used.

## Critères d'acceptation
- [x] The commands run without an unimplemented-command warning (none of these ever warned: they were empty).
- [x] A traffic group defined in the level script drives (read from the script, as before).
- [x] `SetMaxTraffic` caps the cars driving.
