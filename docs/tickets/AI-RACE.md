# AI-RACE — IA d'adversaire de course avec rubber-banding

- **Status:** PARTIAL (2026-08-22) — l'adversaire pilote un vrai véhicule, mais se coince avant de boucler un tour
- **Priority:** P2
- **Module:** AI
- **Depends on:** AI-PATH (couvert : `src/AI/PathFollower.*`)
- **Files:** `src/AI/PathFollower.*` (nouveau), `src/AI/RaceOpponent.*` (nouveau), `src/Scripting/ScriptEngine.cpp`, `src/Game.h`

## Contexte
Aucune IA d'adversaire de course n'existe dans le codebase (seuls `TrafficManager`, `PedestrianManager`, `ChaseManager` existent). Ce ticket pilote SCRIPT-D : il faut une IA capable de suivre un circuit, de conduire un `Vehicle` et de s'adapter au joueur (rubber-banding).

## Approche
1. Créer `RaceOpponent` (`src/AI/`) qui suit une route définie (chemin de waypoints du circuit, via le contrôleur de AI-PATH).
2. Réutiliser la logique de pilotage `seekSteer`/`arrivalSpeed` (`TrafficManager.cpp:16-39`) pour le braquage et la vitesse cible, mais appliquer la sortie à un vrai `Vehicle` via `Vehicle::ApplyInput(throttle, steer, brake, boost)` plutôt qu'à une intégration cinématique.
3. Implémenter le **rubber-banding** : mesurer l'écart de progression adversaire↔joueur le long du circuit et moduler le `boost`/vitesse cible (accélère quand distancé, ralentit quand en tête) dans une plage bornée.
4. Brancher spawn et `Update(dt)` côté `Game` quand une course est active (déclenché par SCRIPT-D).

## Réalisation (2026-08-22)

- **`PathFollower`** (`src/AI/PathFollower.*`) : `Steering::Seek` / `Steering::ArrivalSpeed`
  extraites des copies file-static de `TrafficManager.cpp`, plus un suivi de boucle de
  waypoints (index, tours, `Progress()` monotone pour comparer deux concurrents).
- **`RaceOpponent`** (`src/AI/RaceOpponent.*`) : pilote un vrai `Vehicle` via
  `ApplyInput(throttle, steer, brake, boost)` — pas d'intégration cinématique. Rubber-banding
  borné à [0,7 ; 1,6] selon l'écart de progression avec le joueur.
- **`ScriptEngine`** crée l'adversaire pour un véhicule d'étape dont le `behaviour` vaut
  `"race"`, le détruit avec la mission, et **cesse de le piloter si le joueur monte dans sa
  voiture** (sinon deux entrées se disputent le même véhicule dans la même frame). L'ancienne
  IA cinématique (`_aiPosition` intégrée à la main) est remplacée ; les accesseurs hérités
  pointent désormais sur le véhicule réel.
- Un véhicule `"race"` dont le locator est introuvable démarre **sur son circuit** et non à
  côté du joueur : lâché près du joueur il naissait coincé contre un bâtiment.

## Ce qui ne marche pas encore

L'adversaire **se coince**. Mesuré sur 75 s : il capture le waypoint 0, roule jusqu'à
`(110, 2, -557)` en visant le waypoint 1, puis reste bloqué à 1 km/h braquage à fond.
La détection d'enlisement se déclenche (le nombre d'échantillons chute) mais la marche
arrière ne le libère pas.

Pistes pour la suite :

- Le circuit ne fait que **6 waypoints** issus des checkpoints du joueur — beaucoup trop
  espacés pour définir une trajectoire routière. Il faudrait suivre le `PathGraph` complet
  (`AddStageWaypoint("m1_AI_path1")` nomme un chemin qui n'est pas encore exploité).
- Aucune détection d'obstacle : la voiture vise le waypoint en ligne droite, bâtiments
  compris.
- La manœuvre de dégagement est naïve (marche arrière, braquage inversé) et ne réessaie pas
  d'angle différent.

## Critères d'acceptation
- [x] Un `RaceOpponent` suit le circuit en pilotant un `Vehicle` via `ApplyInput`.
- [x] Le rubber-banding accélère l'adversaire distancé et le ralentit quand il est en tête, dans une plage bornée (observé à 0,85 en tête).
- [ ] L'adversaire termine un tour complet sans rester bloqué — **échoue**, cf. ci-dessus.
- [ ] SCRIPT-D peut instancier, démarrer et arrêter l'adversaire — `ScriptEngine` le crée et le détruit, mais les commandes `SCRIPT-D` elles-mêmes restent des stubs.
