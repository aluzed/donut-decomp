# LEVEL-MISSIONP3D — Les locators de mission n'étaient jamais chargés

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (toute résolution de locator de mission échouait)
- **Module:** Level / Scripting / P3D
- **Depends on:** —
- **Blocks:** AI-RACE, SCRIPT-A, SCRIPT-D
- **Files:** `src/P3D/P3D.generated.{h,cpp}` (`Locator2`), `src/Level.cpp`, `src/Scripting/ScriptEngine.cpp`, `dev/codegen/p3d.json`

## Contexte

Depuis le début de la session, chaque commande de mission qui nomme un locator échouait en
silence et retombait sur une position de repli :

```
ScriptEngine: locator 'm1_snake_carstart' not found, spawning 'snake_v' near player
```

`AddStageWaypoint("m1_AI_path1")` ne résolvait rien non plus, ce qui bloquait `AI-RACE`.

## Deux causes, toutes deux confirmées sur les assets réels

### 1. Les P3D de mission ne sont jamais chargés

Les locators de mission ne sont pas dans `L1_TERRA.p3d` mais dans des fichiers que rien
n'ouvrait :

- `art/missions/level01/level.p3d` — **124 `Locator2`**, dont `level1_homer_start` et
  `level1_carstart` qu'utilise `SetMissionResetPlayerOutCar` ;
- `art/missions/level01/m1.p3d` — **11 `Locator2`**, dont `m1_AI_path1`, `m1_AI_path4`,
  `m1_AI_path1b` (chacun avec son `TriggerVolume` et sa `LocatorMatrix`) et `race_finish`.

`SelectMission` charge désormais les deux.

### 2. `Locator2` ne lisait pas sa propre position

Le layout réel, reversé et vérifié sur **1045 occurrences, exact à 100 %** :

```
name        : chaîne préfixée par sa longueur
type        : u32
dataSize    : u32              nombre de u32 de données spécifiques au type
data        : u32[dataSize]
position    : vec3
numTriggers : u32              == nombre de chunks TriggerVolume enfants (vérifié 1045/1045)
```

La classe s'arrêtait après `dataSize` et sautait directement aux triggers enfants. `Level`
en tirait donc la position depuis `triggers[0]->GetTransform()` — et **jetait purement et
simplement tout locator sans trigger**. Or c'est le cas de la plupart : `level1_carstart`,
`level1_homer_start`, `race_finish`… `p3d.json` est corrigé en conséquence.

## Vérification

À l'exécution, après lancement de la mission :

```
Level: locator 'level1_homer_start' at (229.0, 3.6, -177.0)
Level: locator 'm1_AI_path1'        at (38.1, 3.3, -211.3)
Level: locator 'm1_AI_path4'        at (10.5, 0.5, -623.8)
Level: locator 'm1_AI_path1b'       at (154.7, 3.3, -271.0)
Level: locator 'race_finish'        at (10.5, 0.5, -623.8)
ScriptEngine: player placed (out of car) at 'level1_homer_start' (229.0, 3.6, -177.0)
```

`level1_homer_start` à (229, 3.6, -177) correspond de près à la position de caméra codée en
dur au démarrage `(228, 5, -174)` — recoupement indépendant du décodage.

Les avertissements « locator not found » tombent de « tous » à **2** : `m1_snake_carstart`,
qui n'existe sous ce nom **nulle part** dans les assets extraits, reste introuvable. À
élucider avec `SCRIPT-A` (nom différent dans les données ? locator d'un autre niveau ?).

## Suite

`AI-RACE` peut maintenant construire son circuit à partir des vrais nœuds
`m1_AI_path*` et de `race_finish`, au lieu de la boucle de circulation prise au hasard.
Attention : ce sont trois points isolés, pas une polyligne — il faudra vraisemblablement les
relier via le réseau de `Path` du niveau.
