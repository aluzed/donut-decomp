# AI-PATH — Contrôleur de suivi de chemin réutilisable

- **Status:** PARTIAL (2026-08-23) — contrôleur extrait et réutilisé ; le graphe est connexe et suit les routes ; `TrafficManager` garde ses copies et 95 liens restent trop longs (voir ci-dessous)
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

## Diagnostic du circuit à 4 points (2026-08-23)

Journalisation par tronçon, comme prévu. Le verdict était immédiat :

```
race node 0 (107.6, 1.1, -558.5) -> noeud 641  ( 0.0 m, composante 0)
race node 1 ( 38.1, 3.3, -211.3) -> noeud 504  ( 9.6 m, composante 6)   <-- îlot
race node 2 ( 10.5, 0.5, -623.8) -> noeud 1052 (10.0 m, composante 0)
leg 0->1 : 354 m à vol d'oiseau, 0 nœud     leg 1->2 : 414 m, 0 nœud
```

Ni `FindNearestNode` ni l'A\* n'étaient en cause : le nœud 504 tombait dans l'une des
14 composantes résiduelles. Aucune des deux hypothèses notées ici n'était la bonne — c'était
la connexité elle-même, à 91 % et non 100 %.

### Le graphe est désormais connexe à 100 %

`bridgeComponents()` (Boruvka : chaque composante se relie à la plus proche, paire la plus
courte d'abord, jusqu'à n'en garder qu'une). 4 ponts suffisent, dont 3 de moins de 27 m —
les trous que le niveau laisse réellement entre deux pâtés. `GetComponent(node)` expose
l'étiquette pour que le prochain échec de route dise *pourquoi*.

### Les `Road` ne sont pas des segments droits

Relier les 44 intersections deux à deux donnait des tronçons de 118 m tirés au cordeau — la
voiture de course a pris le premier et l'a fini dans le mur d'un immeuble. La vraie
géométrie était sous les `Road`, inexploitée :

- **966 chunks `RoadSegment`**, enfants des `Road` : `name, data, mat4, mat4`. La
  translation de `transform` donne la position monde du tronçon. Layout vérifié
  exactement : 17 (LP string, longueur incluant le nul) + 17 + 128 = **162 octets**,
  la taille observée sur tous.
- **937 chunks `RoadDataSegment`** (`name, u32, lanes, u32, 3× vec3`) : 17 + 12 + 36 =
  **65 octets**, également exact. Non exploités pour l'instant — géométrie de voie locale.

`Level` collecte la chaîne de chaque `Road`, `PathGraph` la déroule
`jonction → tronçon → … → jonction`. Les 99 `Road` en portent toutes.

| | nœuds | composantes | plus grande | pas moyen du circuit |
|---|---|---|---|---|
| Avant | 1086 | 15 | 91 % | 51 m |
| Après | 2052 | **1** | **100 %** | **15 m** |

### L'ordre des chunks n'est pas l'ordre de la route

Les `RoadSegment` ne sont pas rangés le long de la route : les prendre dans l'ordre du
fichier donnait **288 liens de plus de 25 m, dont un de 239 m**, et un circuit qui
zigzaguait d'un trottoir à l'autre. `PathGraph` les enchaîne maintenant au plus proche
depuis la jonction de départ — l'ordre dans lequel une voiture les parcourt.

### Un `RoadSegment` se place par son coin, pas par son centre

`RoadDataSegment` donne trois coins d'une **dalle de bitume**, le quatrième étant à l'origine
locale. La translation du `transform` d'un `RoadSegment` est donc un **coin** de la dalle :
la prendre pour position de nœud posait la ligne médiane le long d'un caniveau et faisait
zigzaguer les dalles successives. Le nœud est maintenant au centroïde du quadrilatère,
`(p0 + p1 + p2) / 4` — le centroïde et non `p1/2`, car seules 495 des 937 dalles sont des
parallélogrammes ; les 442 autres sont les trapèzes qui composent les virages.

> Attention : ces matrices P3D portent leur translation sur la **dernière ligne**
> (cf. `Matrix4x4::Translation()`), alors que `Matrix4x4::operator*(Vector3)` la lit sur la
> dernière colonne. La transformation est écrite à la main dans `Level.cpp`.

### Les boucles `Path` ne servent plus à router

`FindRoute` ne traverse plus que les nœuds de route (jonctions + dalles). Les 1012 nœuds de
boucle `Path` sont des voies de circulation autour d'un pâté : ils coupent par les avant-cours
et les accotements, et l'A\* les prenait comme raccourcis parce qu'ils sont plus rapprochés
que les dalles — ce qui mettait la trajectoire de course dans un mur. Le trafic continue de
s'en servir via `GetNextNode`.

Le réseau routier seul est connexe (1 composante), donc rien n'est perdu : `bridgeComponents`
est appelé d'abord sur les seuls nœuds de route, puis sur le graphe entier.

| | nœuds | composantes | liens > 25 m | pas moyen | plus long tronçon |
|---|---|---|---|---|---|
| Boucles `Path` seules | 1086 | 110 → 15 | — | 51 m | — |
| + `Road` droites | 1086 | 15 | 288 (max 239 m) | 51 m | 118 m |
| + chaîne `RoadSegment` ordonnée | 2052 | 1 | 105 (max 152 m) | 15 m | 82 m |
| + centre de dalle, route seule | 2052 | **1** | **95 (max 90 m)** | **13 m** | **46 m** |

## Les 95 liens longs sont en grande partie légitimes

Hypothèse précédente — « une `Road` porte les dalles de plusieurs voies » — **infirmée**.
Ce que montre le relevé par catégorie :

- **53 sont des liens dalle-à-dalle**, et presque tous sur une ligne droite : une rue droite
  est pavée de quelques longues dalles plutôt que d'une multitude de petites. Trois dalles
  successives à x ≈ -121 sont espacées de 62 m, alignées ; la droite qui les relie reste au
  milieu de la chaussée. Rien à corriger.
- **42 aboutissent sur une jonction ou sur un nœud de boucle raccroché.** Les raccrochages ne
  servent plus au routage (`FindRoute` ne passe que par la route), donc seuls comptent les
  liens jonction↔dalle, quand le centre d'un carrefour est loin de la première dalle.

Ce que `lanes` désigne : la **largeur** de la rue, pas deux rangées de dalles dans le même
chunk. Chaque rue est en fait décrite par **deux chunks `Road`, un par sens** — `z1RoadNode4`
va de `r1IntersectionLocatorNode` à `r1IntersectionLocatorNode4` et `z1RoadNode3` fait
exactement l'inverse. Le graphe contient donc deux chaînes parallèles par rue, distantes
d'une largeur de voie.

L'ordre des dalles dans le chunk est régulier, simplement pas monotone : il part du milieu
de la rue vers une extrémité, puis repart du milieu vers l'autre (`z1RoadNode1` : Z = -107,
-116, … -147, puis -99, -88, … -60). L'enchaînement au plus proche depuis la jonction de
départ le remet d'aplomb correctement.

Notes de reverse au passage : `RoadDataSegment.todo1` vaut 1 sur les 937 occurrences (donc
constante, pas un champ utile), et `todo0` prend des valeurs de 0 à ~106, mais **ce n'est pas
un index par route** — seules 12 dalles portent `todo0 = 0` alors qu'il y a 99 routes.

## Reste

Les liens jonction↔dalle trop longs (au plus 90 m) sont le seul reste identifié côté graphe.
Mesurer leur longueur séparément des raccrochages de boucle est le prochain pas : si un
carrefour est loin de ses dalles, insérer la première dalle de chaque sens comme point de
passage obligatoire suffirait.

Au-delà, la perte de temps de l'adversaire ne vient plus principalement du tracé
(cf. AI-RACE : c'est la puissance moteur et la tenue de virage).

## Critères d'acceptation
- [x] Un `PathFollower` autonome existe et ne dépend que de `PathGraph` + état d'agent.
- [ ] `TrafficManager` délègue son pilotage à `PathFollower` — **non fait** : ses `seekSteer`/`arrivalSpeed` file-static sont intacts, pour ne pas toucher au comportement du trafic sans pouvoir le vérifier.
- [ ] `seekSteer`/`arrivalSpeed` ne sont plus dupliqués dans `TrafficManager.cpp` — dépend du point précédent.
- [x] Le contrôleur est réutilisable par un autre agent — `RaceOpponent` l'utilise (AI-RACE).
- [x] Le graphe est connexe (1 composante, 100 % des nœuds) et suit la forme des routes
      (pas moyen 13 m contre 51 m), et le réseau routier l'est aussi à lui seul.
- [ ] Aucun lien du réseau routier ne dépasse la largeur d'une rue — **95 dépassent 25 m**.
