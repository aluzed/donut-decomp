# SCRIPT-G — Commandes de trafic / piétons

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Un `.con` représentatif de trafic/piétons s'exécute avec 0 avertissement « unimplemented command ».
- [ ] Un groupe de trafic défini (`CreateTrafficGroup`/`AddTrafficModel`/`CloseTrafficGroup`) spawne effectivement des véhicules.
- [ ] `SetMaxTraffic` plafonne le nombre de véhicules de trafic actifs.
