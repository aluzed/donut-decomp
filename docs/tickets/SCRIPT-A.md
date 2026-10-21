# SCRIPT-A — Compléter le jeu de commandes de mission P0 (course de base)

- **Status:** DONE (2026-10-21)
- **Priority:** P1
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp:37-182`, `src/Scripting/ScriptEngine.cpp:19-412`, `scripts/Missions/level01/M1race.con`

## Contexte
Le jeu de commandes minimal d'une mission de course (~18 commandes) est en grande partie câblé vers `ScriptEngine`, mais quelques commandes restent des stubs vides ou de simples logs, ce qui empêche `M1race.con` de tourner proprement de bout en bout. Il faut combler ces trous pour que le scénario P0 fonctionne sans avertissement.

## Approche
- Auditer chaque commande référencée par `scripts/Missions/level01/M1race.con` et vérifier qu'elle pointe vers une méthode réelle de `ScriptEngine` (et non un `{}`).
- Commandes déjà implémentées à conserver : `SelectMission`, `SetMissionResetPlayerInCar`, `SetDynaLoadData`, `AddStage`, `SetHUDIcon`, `AddObjective`, `SetStageTime`, `AddNPC`, `AddStageVehicle`, `CloseStage`, `CloseMission`, `ShowStageComplete`, `AddCharacter`, `InitLevelPlayerVehicle`, `PlacePlayerCar`, `CreateChaseManager`, `SetPresentationBitmap`, `EnableTutorialMode` (`GameCommands.cpp:37-182`).
- Pour les commandes encore stub utilisées par le `.con` de course (notamment `AddCondition`/`CloseCondition` `GameCommands.cpp:101-102`, `AddCollectible` `:93`, `SetStageMessageIndex` `:64`, `RESET_TO_HERE` `:85`), implémenter une logique minimale (stockage dans `ScriptEngine` + `Log::Info`) qui consomme correctement les paramètres.
- Ajouter dans `ScriptEngine` les setters/conteneurs manquants (conditions de stage, collectibles de mission) en suivant le motif existant de `_objectiveType`/`_checkpoints`.

## Critères d'acceptation
- [x] `M1race.con` runs with no unknown command (`command ... not found`) nor failed one; the seven levels' scripts likewise over `val.sh 240` (2026-10-21). Its one warning is the data's: `m1_snake_carstart` is in no file.
- [x] Every command listed calls `ScriptEngine`; `CloseCondition`, which closes what `AddCondition` opened, has nothing to do and says so.
- [x] The race ends through its stage's completion and the mission's cleanup (`stage complete! Time: 24.5s`, `mission cleaned up`), and starts again.
