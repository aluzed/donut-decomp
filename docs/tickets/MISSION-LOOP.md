# MISSION-LOOP — Chaque `.con` terminait sa propre mission à la fin du parsing

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (bloquait toute la piste « contenu missions »)
- **Module:** Scripting
- **Depends on:** —
- **Blocks:** SCRIPT-A, SCRIPT-D, AI-RACE, et tout test en véhicule
- **Files:** `src/Scripting/ScriptEngine.cpp` (`CloseMission`, `CloseStage`, `CleanupMission`), `src/Game.cpp` (entrée en véhicule)

## Contexte

Découvert en validant `UI-HUD` (2026-08-22) : impossible d'atteindre un véhicule pour tester
le compteur de vitesse. Au lancement de la mission, l'écran affichait immédiatement
« STAGE COMPLETE! Time: 0.0s » puis « Next race in N… », et le cycle recommençait sans fin.

## Cause racine

Un `.con` **décrit** une mission ; il ne la joue pas. Sa structure est :

```
SelectMission("m1");   // ouvre la définition
AddStage(1);           // ouvre la définition d'une étape
    ...déclarations...
CloseStage();          // ferme la définition de l'étape
CloseMission();        // ferme la définition de la mission
```

`ScriptEngine::CloseMission()` interprétait ce marqueur de fin de bloc comme « la mission est
terminée » : il mettait `_missionActive = false` et appelait
`_game.SetState(GameState::MissionComplete)`. Donc **la dernière ligne de chaque script
terminait la mission que ce script venait de déclarer** — d'où le chrono à 0.0 s (l'étape
n'avait jamais tourné), l'écran de fin, le compte à rebours de 5 s, le re-lancement du
script, et la boucle.

Deuxième occurrence de la même confusion dans `Game.cpp` : **entrer dans un véhicule**
appelait `CloseObjective()` puis `ShowStageComplete()`. Monter en voiture démarre la course,
ça ne la termine pas.

## Correctif

- `CloseMission()` ne fait plus que journaliser la fin de la *définition* : la mission est
  alors chargée et en cours. La complétion revient au gameplay
  (`AdvanceCheckpoint` → `ShowStageComplete`).
- `CloseStage()` journalise « étape déclarée » plutôt que « étape terminée ».
- `CleanupMission()` reprend la remise à zéro de `_missionActive` / `_currentStage`, que
  `CloseMission()` faisait par effet de bord. Sans ça, le garde
  `if (_missionActive) return;` de `SelectMission` aurait refusé tout rechargement.
- Suppression de l'appel à `ShowStageComplete()` à l'entrée en véhicule.

## Vérification

- Mission lancée : « Objective: race », « Lap 1/2 CP 0/6 », chrono qui décompte
  120 → 106 → 59 s. Zéro occurrence de « stage complete » dans le journal.
- Deux redémarrages successifs (`M`) : la mission se recharge et le chrono repart à ~120 s.
- Un véhicule de mission est visible dans la scène après le redémarrage.

## Reste ouvert

Le véhicule de mission apparaît, mais ni le téléport de debug (`T`) ni `E` ne permettent d'y
monter — le contrôle de proximité (`< 5 m` de `_character`) n'est jamais satisfait. Piste :
`AddStageVehicle` place la voiture sur le locator `m1_snake_carstart` alors que
`SetMissionResetPlayerOutCar` place le joueur ailleurs. À traiter avec `SCRIPT-A`.
