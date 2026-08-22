# SCRIPT-PARSE — `find_first_of("//")` supprimait toute commande `.con` ayant un chemin en argument

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (silencieux, et il coupait une partie du contenu des missions)
- **Module:** Scripting
- **Depends on:** —
- **Blocks:** SCRIPT-A, SCRIPT-B, AI-RACE, tout test en véhicule
- **Files:** `src/Scripting/Commands.h` (`RunLine`)

## Cause racine

`RunLine` retirait les commentaires de fin de ligne ainsi :

```cpp
std::size_t commentPos = line.find_first_of("//");
```

`find_first_of` cherche **n'importe quel caractère** de l'ensemble, donc le **premier `/`**, pas
la séquence `//`. Sur une ligne comme :

```
AddStageVehicle("snake_v","m1_snake_carstart","race","Missions/level01/M1race.con","snake");
```

la ligne était tronquée au premier `/` — à l'intérieur d'une chaîne entre guillemets :

```
AddStageVehicle("snake_v","m1_snake_carstart","race","Missions
```

`rfind(");")` échouait alors, `RunLine` renvoyait `false`, et **la commande était ignorée sans
le moindre message**. Toute commande dont un argument contient un `/` était donc perdue :

- `AddStageVehicle(..., "Missions/level01/M1race.con", ...)` → aucun véhicule de mission
  n'était jamais créé ;
- `SetPresentationBitmap("art/frontend/dynaload/images/mis01_01.p3d")` → jamais exécutée.

Cela expliquait pourquoi la course n'avait aucune voiture atteignable, symptôme que j'avais
d'abord attribué aux locators.

## Correctif

Balayage explicite de la séquence `//`, en ignorant celles situées **dans** une chaîne entre
guillemets.

## Effets de bord corrigés en même temps

- **`Core/Log.h` : `stdout` bufferisé.** `Log::Info`/`Debug` écrivent via `fmt::print` sur
  `stdout`, entièrement bufferisé dès que la sortie n'est pas un terminal. En redirigeant vers
  un fichier, tout ce qui restait dans le tampon était perdu si le processus était tué —
  précisément quand le journal sert. C'est ce qui m'a fait diagnostiquer à l'aveugle pendant
  plusieurs itérations. Ajout d'un `fflush(stdout)` par ligne (`stderr` est déjà non
  bufferisé).
- **Téléport de debug hors de portée.** `debug_teleport_to_vehicle` plaçait le joueur à
  `pos + (0, 2, 5)`, soit **5,39** unités, alors que l'embarquement exige `< 5`. Il ne
  pouvait donc jamais être suivi d'un « interact » réussi. Ramené à `(2.5, 1, 0)`.
- **Un seul `E` faisait entrer puis sortir.** La branche de sortie est gardée par
  `_inVehicle`, qui vient d'être mis à `true` par la branche d'entrée : la même intention
  était consommée deux fois dans la frame. L'entrée remet maintenant `intent.interact` à
  `false`.

## Vérification

Journal, chemin complet : `presentation bitmap = 'art/frontend/dynaload/images/mis01_01.p3d'`,
`add stage vehicle 'snake_v' at 'm1_snake_carstart' (80.6, 3.4, -640.2)`,
`Game: entered vehicle 'snake_v'` — **sans** `exited vehicle` derrière.

À l'écran : indications passées à « Up: Drive | LShift: Boost | H: Horn | E: Exit », barre
libellée « CAR », compteur de vitesse affiché. Le compteur de `UI-HUD` est donc désormais
validé par le chemin réel et non plus par un forçage.

## Reste ouvert

Une fois embarqué, le véhicule traverse le terrain (caméra à Y = -1,9 sous le décor). La
correction `CHARCTRL-FALL` ne concernait que la capsule du personnage ; les corps de véhicule
ont leur propre problème de collision. À ouvrir séparément.
