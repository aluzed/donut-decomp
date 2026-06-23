# AI-RACE — IA d'adversaire de course avec rubber-banding

- **Status:** TODO
- **Priority:** P2
- **Module:** AI
- **Depends on:** AI-PATH
- **Files:** `src/AI/` (nouveau `RaceOpponent.*`), `src/AI/TrafficManager.cpp:84-119` (réf. seekSteer/arrivalSpeed), `src/AI/PathGraph.h`, `src/Vehicle.h`

## Contexte
Aucune IA d'adversaire de course n'existe dans le codebase (seuls `TrafficManager`, `PedestrianManager`, `ChaseManager` existent). Ce ticket pilote SCRIPT-D : il faut une IA capable de suivre un circuit, de conduire un `Vehicle` et de s'adapter au joueur (rubber-banding).

## Approche
1. Créer `RaceOpponent` (`src/AI/`) qui suit une route définie (chemin de waypoints du circuit, via le contrôleur de AI-PATH).
2. Réutiliser la logique de pilotage `seekSteer`/`arrivalSpeed` (`TrafficManager.cpp:16-39`) pour le braquage et la vitesse cible, mais appliquer la sortie à un vrai `Vehicle` via `Vehicle::ApplyInput(throttle, steer, brake, boost)` plutôt qu'à une intégration cinématique.
3. Implémenter le **rubber-banding** : mesurer l'écart de progression adversaire↔joueur le long du circuit et moduler le `boost`/vitesse cible (accélère quand distancé, ralentit quand en tête) dans une plage bornée.
4. Brancher spawn et `Update(dt)` côté `Game` quand une course est active (déclenché par SCRIPT-D).

## Critères d'acceptation
- [ ] Un `RaceOpponent` suit le circuit en pilotant un `Vehicle` via `ApplyInput`.
- [ ] Le rubber-banding accélère l'adversaire distancé et le ralentit quand il est en tête, dans une plage bornée.
- [ ] L'adversaire termine un tour complet sans rester bloqué.
- [ ] SCRIPT-D peut instancier, démarrer et arrêter l'adversaire.
