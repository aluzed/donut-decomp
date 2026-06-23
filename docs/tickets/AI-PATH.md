# AI-PATH — Contrôleur de suivi de chemin réutilisable

- **Status:** TODO
- **Priority:** P2
- **Module:** AI
- **Depends on:** —
- **Files:** `src/AI/TrafficManager.cpp:16-39`, `src/AI/TrafficManager.cpp:82-119`, `src/AI/PathGraph.h`, `src/AI/PathGraph.cpp`, `src/AI/` (nouveau `PathFollower.*`)

## Contexte
La logique de suivi de chemin (sélection du nœud suivant, `seekSteer`, `arrivalSpeed`, avance le long du graphe) est aujourd'hui embarquée dans `TrafficManager` (helpers `TrafficManager.cpp:16-39`, boucle `Update` `TrafficManager.cpp:82-119`). `PathGraph` (`PathGraph.h`) expose déjà `GetNextNode`/`FindNearestNode`/`GetRandomNode`. Il faut extraire un contrôleur réutilisable pour le partager avec AI-RACE et d'autres agents.

## Approche
1. Créer `PathFollower` (`src/AI/`) encapsulant un état d'agent : `currentNode`, `targetNode`, `position`, `rotation`, paramètres (`maxSpeed`, `maxSteer`, `slowDist`).
2. Y déplacer `seekSteer` et `arrivalSpeed` (`TrafficManager.cpp:16-39`) et le pas de progression de `Update` (`TrafficManager.cpp:82-119`) ; exposer une méthode `Step(const PathGraph&, double dt)` retournant `{steer, speed}` ou mettant à jour pos/rot.
3. Refactorer `TrafficManager::Update` pour déléguer à `PathFollower` par voiture (comportement inchangé).
4. Vérifier que `AI-RACE` peut réutiliser `PathFollower` pour produire les entrées d'un `Vehicle`.

## Critères d'acceptation
- [ ] Un `PathFollower` autonome existe et ne dépend que de `PathGraph` + état d'agent.
- [ ] `TrafficManager` délègue son pilotage à `PathFollower` sans changement de comportement visible.
- [ ] `seekSteer`/`arrivalSpeed` ne sont plus dupliqués dans `TrafficManager.cpp`.
- [ ] Le contrôleur est réutilisable par un autre agent (compile/usage depuis AI-RACE).
