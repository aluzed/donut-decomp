# AI-PATH — Contrôleur de suivi de chemin réutilisable

- **Status:** PARTIAL (2026-08-22) — contrôleur extrait et réutilisé ; `TrafficManager` garde ses copies, et le graphe routier est disjoint (voir ci-dessous)
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

## Réalisation (2026-08-22)

- `src/AI/PathFollower.*` : `Steering::Seek` et `Steering::ArrivalSpeed` extraites des copies
  file-static de `TrafficManager.cpp`, plus un suivi de boucle de waypoints (index, tours,
  `Progress()` monotone).
- `PathGraph::FindRoute(from, to)` : **A\*** sur le graphe complet. `GetNextNode` existant est
  un glouton « meilleur d'abord » qui se piège dans les minima locaux ; il est conservé pour
  le trafic, `FindRoute` le remplace pour tout ce qui doit vraiment aller d'un point à un
  autre.

## Constat bloquant : le graphe routier est disjoint

Mesuré sur `L1_TERRA.p3d` en reproduisant la construction de `PathGraph` :

```
noeuds                : 1042
composantes connexes  : 110      (exactement une par chunk Path)
tailles des 10 plus grandes : 41, 26, 24, 24, 23, 22, 22, 22, 19, 19
plus grande composante = 4 % des noeuds
```

Chaque chunk `Path` est une boucle fermée autour d'un pâté de maisons, et **rien ne relie
deux boucles**. Le voisinage construit par `PathGraph` (meilleur voisin à moins de 30 m,
plus les voisins séquentiels à moins de 50 m) ne franchit jamais l'écart entre deux boucles.

Conséquence : `FindRoute` fonctionne, mais ne peut jamais router entre deux quartiers — le
circuit de course composé pour `AI-RACE` retombe à ses seuls points d'extrémité. C'est aussi
pourquoi les voitures du trafic ne font que tourner autour de leur propre bloc.

## Résolu : le vrai réseau routier était ailleurs

Les boucles `Path` ne sont pas le réseau routier. Le niveau en contient un second, distinct,
que rien n'exploitait :

- **60 chunks `Intersection`** — layout conforme à `p3d.json` (`name, position, radius,
  trafficBehaviour`), vérifié : 0 octet résiduel sur les 60 ;
- **99 chunks `Road`** — layout également conforme (`name, u32, startIntersection,
  endIntersection, maxCars, 4×u8`), et **les 99 résolvent leurs deux extrémités** vers une
  intersection connue.

Pris seul, le graphe des intersections forme **une seule composante connexe**.

`Level` collecte désormais les deux, et `PathGraph` les intègre : un nœud par intersection,
une arête par `Road`, puis chaque nœud de boucle raccroché à la jonction la plus proche
(`kJunctionSnapDistance`).

| | composantes | plus grande |
|---|---|---|
| Avant | 110 | 4 % des nœuds |
| Après (accrochage 40 m) | 29 | 80 % |
| Après (accrochage 70 m) | **15** | **91 %** |

`PathGraph` journalise maintenant cette connexité au démarrage : c'est le chiffre qui décide
si une IA peut traverser la ville, et il était jusqu'ici invisible.

## Reste

Le circuit de course composé par `ScriptEngine::buildRaceCircuit` ne fait toujours que
4 points malgré un graphe à 91 % connexe : `FindRoute` renvoie des itinéraires très courts
entre les nœuds de course. À diagnostiquer — soit `FindNearestNode` accroche les extrémités
sur une petite composante résiduelle, soit l'A\* s'arrête trop tôt. Journaliser la taille de
chaque tronçon est le prochain pas.

## Critères d'acceptation
- [x] Un `PathFollower` autonome existe et ne dépend que de `PathGraph` + état d'agent.
- [ ] `TrafficManager` délègue son pilotage à `PathFollower` — **non fait** : ses `seekSteer`/`arrivalSpeed` file-static sont intacts, pour ne pas toucher au comportement du trafic sans pouvoir le vérifier.
- [ ] `seekSteer`/`arrivalSpeed` ne sont plus dupliqués dans `TrafficManager.cpp` — dépend du point précédent.
- [x] Le contrôleur est réutilisable par un autre agent — `RaceOpponent` l'utilise (AI-RACE).
