# AI-RACE — IA d'adversaire de course avec rubber-banding

- **Status:** PARTIAL (2026-08-23) — l'adversaire court : 67 waypoints sur 126 en 120 s, 0 chute ; il n'a pas le temps de boucler les 1621 m du circuit
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

## Deuxième passe (même journée) : deux causes écartées

**Circuit trop grossier — corrigé.** L'adversaire suivait les 6 checkpoints du joueur,
répartis sur tout le tour : aucune trajectoire routière là-dedans. `ScriptEngine` construit
désormais un `_racePath` distinct en rééchantillonnant le tracé du niveau à **12 m**
d'intervalle — 23 waypoints pour un chemin de 19 points sur `L1_TERRA`. Les checkpoints du
joueur restent inchangés.

**Accélérateur mal piloté — corrigé.** J'utilisais `Steering::ArrivalSpeed`, qui ramène la
vitesse à zéro à l'approche de la cible. C'est juste pour une destination et faux pour un
circuit : l'adversaire s'arrêtait à *chaque* waypoint qu'il devait simplement traverser.
L'accélérateur dépend maintenant de l'angle de braquage, avec un plancher (`kThrottleMin`)
sans lequel la voiture n'a pas assez de couple pour vaincre sa propre friction.

## Ce qui ne marche toujours pas

**L'adversaire reste bloqué au même endroit, pleins gaz.** Après les deux correctifs ci-dessus
il atteint le waypoint 2 puis s'immobilise à **(109, 2, -559)** — position reproductible
d'une exécution à l'autre, vitesse 0, accélérateur au plancher `0,55 × 0,85`. Ce n'est donc
plus un problème de commande mais de **collision** : la voiture est coincée contre la
géométrie à cet endroit précis.

## Le vrai blocage : il n'y a pas de circuit dans les données

> **Mise à jour (même journée)** : `m1_AI_path1` a été retrouvé. Ce n'est pas un chunk `Path`
> mais un `Locator2` du P3D de la mission, que rien ne chargeait — voir
> [LEVEL-MISSIONP3D](LEVEL-MISSIONP3D.md). Les nœuds `m1_AI_path1`, `m1_AI_path1b`,
> `m1_AI_path4` et `race_finish` sont désormais résolus. Ce sont **trois points isolés**, pas
> une polyligne : il reste à les relier en trajectoire via le réseau de `Path` du niveau.
>
> **Suite (même journée)** : le routage est implémenté — `AddStageWaypoint` résout son
> locator, et `CloseMission` compose un itinéraire départ → waypoints → `race_finish` en le
> faisant passer par `PathGraph::FindRoute` (A\*). Mais **le graphe routier est disjoint**
> en 110 composantes, une par boucle de circulation (cf. [AI-PATH](AI-PATH.md)) : aucun
> itinéraire inter-quartiers n'existe, et le circuit retombe donc à ses seuls points
> d'extrémité. Relier les boucles — vraisemblablement via les chunks `Intersection` — est le
> prérequis suivant.

Recensement des chemins de `L1_TERRA` (instrumenté, 2026-08-22) :

```
110 paths in level
  path 0:  9 pts, len=116m, first=(348,3.4,-168) last=(348,3.4,-168)
  path 1: 11 pts, len=142m, first=(289,4.4,-168) last=(289,4.4,-168)
  path 2:  7 pts, len=164m, first=(216,3.4,-167) last=(216,3.4,-167)
  ...
```

**110 chemins, chacun une petite boucle fermée** (premier point = dernier, 5 à 11 points,
56 à 164 m). Ce sont des boucles de circulation autour de pâtés de maisons — le réseau que
`TrafficManager` exploite. **Aucun n'est un circuit de course.**

Or `AddObjective("race")` choisit « le chemin le plus long du niveau » (19 points) et le
baptise circuit. C'est arbitraire : on obtient une boucle de quartier au hasard, pas le tracé
que la mission décrit.

Et le tracé que la mission décrit, `AddStageWaypoint("m1_AI_path1")`, **n'est pas
résolvable** : le chunk P3D `Path` ne porte que `GetNumPoints()` et `GetPoints()` — **pas de
nom** (`src/P3D/P3D.generated.h:2110`). Rien dans le moteur ne peut associer la chaîne
`"m1_AI_path1"` à un chemin. Ce n'est pas non plus un locator (aucun `m1_AI_path*` dans la
liste des locators du niveau).

C'est donc là que se situe le vrai travail, et il précède toute amélioration du pilotage :

1. Trouver comment le format nomme les chemins — probablement un chunk voisin (`Intersection`,
   `LocatorMatrix`, `Spline`, cf. `P3D-008`) ou un `Locator2` de type chemin, à reverser.
2. Faire porter un nom à `Level::Path` et permettre `AddStageWaypoint` de le résoudre.
3. Seulement ensuite : évitement d'obstacle et manœuvre de dégagement.

Observation complémentaire : téléporté à côté de la voiture bloquée, on la voit **sur la
route et à moitié enfoncée dedans**, pas coincée contre un bâtiment. À creuser avec le point
1 — un tracé de circulation posé plus bas que la surface roulante expliquerait les deux.

## Ce qui bloquait réellement (2026-08-23)

Le blocage à (109, 2, -559) n'avait pas une cause mais cinq, empilées. Chacune masquait la
suivante.

### 1. `BulletCast<Quaternion>` décalait toutes les composantes

```cpp
return Quaternion(q.w(), q.x(), q.y(), q.z());   // ctor = (x, y, z, w)
```

Toute rotation relue de Bullet était donc une autre rotation. Le sens inverse
(`BulletCast<btQuaternion>`) était correct, donc rien ne se compensait. `Steering::Seek`
calculait le braquage contre un cap sans rapport avec l'orientation réelle de la voiture :
placée face à son waypoint, elle partait à braquage plein dans la direction opposée. La
caméra de suivi et le modèle du véhicule lisaient la même valeur.

### 2. La voiture naissait dans une barrière

`m1_snake_carstart` n'existe pas dans les assets extraits, et le repli la posait sur
`_racePath.front()` — un point qui tombe dans `DONUTFENCE` (96..128, -569..-535). Elle
poussait dans la clôture à 0 km/h pendant toute la course.

Sonde décisive : `m_wheelsSuspensionForce` par roue. Elle valait ~11 900 N au total pour
1 200 kg, donc les roues *touchaient* — la voiture avait de l'adhérence et n'avançait pas,
ce qui a désigné un contact de châssis, pas un problème de suspension.

> Piège : `m_raycastInfo.m_isInContact` ne peut pas être lu de l'extérieur.
> `btRaycastVehicle::updateWheelTransformsWS` le remet à `false`, et `Vehicle::Update`
> l'appelle à chaque frame — il lit donc toujours 0 roue au sol. C'est la force de
> suspension qui survit au tick.

Le circuit est maintenant ancré sur les seuls locators que la mission fournit réellement
(`m1_AI_path1`, `race_finish`) quand le carstart manque, et la voiture est posée sur le
premier point du tracé, face au second. `Vehicle::CreatePhysicsBody` cherche aussi le sol
par raycast au lieu de partir d'un `+1.5 m` aveugle.

### 3. Le waypoint n'était validé qu'au rayon

Rayon de 8 m, et la voiture est passée à 14 m du waypoint 1 : jamais capturé, donc elle a
continué tout droit jusqu'à sortir de la carte. `PathFollower::Advance` valide désormais
aussi le franchissement du plan passant par le waypoint, perpendiculaire au tronçon d'arrivée
— dépasser coûte une trajectoire large, plus la course entière.

### 4. Aucune récupération

Trois filets, dans cet ordre :

- **hors-tracé** : au-delà de 40 m du waypoint visé, on se raccroche au point le plus proche
  du circuit, au lieu de traverser les jardins en ligne droite ;
- **chute** : 25 m sous son propre waypoint = la voiture est passée sous le monde
  (elle atteignait Y = -45 000) → remise sur le circuit ;
- **coincée** : moins de 4 m parcourus en 3 s → marche arrière 1,5 s, et au bout de
  3 tentatives, remise sur le circuit.

L'ancien test de blocage (vitesse < 2 km/h **et** pas < 2 cm sur la frame) ne se déclenchait
pratiquement jamais : une voiture qui rabote un mur à 1-3 km/h le réarmait sans cesse.

### 5. Pas de contrôle de vitesse

Le gaz était piloté par l'angle de braquage, ce qui ne réagit qu'une fois *dans* le virage.
`cornerSpeedKmh()` regarde 35 m de tracé en avant, additionne les changements de cap et vise
entre 60 km/h en ligne droite et 22 km/h en épingle, freins compris.

### 6. Le braquage partait du mauvais côté

`Steering::Seek` calculait

```cpp
const float cross = forward.X * desired.Z - forward.Z * desired.X;
```

soit **l'inverse** de ce qu'attend `btRaycastVehicle::setSteeringValue`, qui fait tourner les
roues avant autour de +Y : `forward` (0,0,1) part vers `(sin, 0, cos)`, donc un braquage
positif va vers +X. L'adversaire braquait donc *à l'opposé* de chaque waypoint : l'erreur de
cap s'amplifiait au lieu de se refermer, et la voiture partait en spirale hors de la route
jusqu'à la première clôture. Le bug de quaternion (§1) rendait le cap aléatoire et masquait
complètement celui-ci ; il n'est devenu visible qu'une fois §1 corrigé.

### 7. Aucun véhicule ne pouvait faire marche arrière

`ApplyInput(0.0f, -steer, 1.0f, 1.0f)` — le « dégagement » — passe un gaz nul et un frein
plein. C'est un freinage, pas une marche arrière : une voiture déjà à l'arrêt ne bouge pas
d'un centimètre. `Vehicle::ApplyInput` n'avait tout simplement pas de marche arrière, le gaz
étant supposé positif. Il accepte désormais un gaz négatif, et **le joueur en profite
aussi** : « reculer » freinait tant que la voiture roulait, puis ne faisait plus rien. Il
freine maintenant tant qu'elle avance et recule une fois arrêtée.

### 8. Le rattrapage hors-tracé revenait en arrière

Le waypoint le plus proche d'une voiture qui vient de couper un virage est **celui qu'elle
vient de dépasser**. `SnapToNearest` le reprenait, le test de franchissement de plan la
faisait ré-avancer à la frame suivante, et le cycle repartait — des centaines de
« rejoining at 4 » d'affilée sans jamais rien changer. Le rattrapage ne regarde plus que
vers l'avant (`SnapToNearestAhead`, fenêtre de 12 waypoints), et son seuil est la distance
*au tronçon* et non au waypoint : à 40 m il se déclenchait au début de chaque tronçon long,
puisque le plus long du circuit fait 46 m.

### État observé

Un tour de mission complet (120 s), traces toutes les 5 s :

| | avant | après |
|---|---|---|
| waypoint le plus loin | 2 / 112 | **67 / 126** |
| chutes hors du monde | permanentes | **0** |
| remises d'office sur le circuit | — | **0** |
| dégagements en marche arrière | 15 | 4 |
| vitesse | 0-6 km/h | 25-53 km/h |

L'adversaire part de (38, -211), traverse la ville et finit à (21.7, -625) — la moitié des
1621 m du circuit. **La mission s'arrête sur l'expiration du chrono de 120 s, plus parce que
la voiture est bloquée.**

Il ne boucle donc toujours pas un tour, mais pour une raison différente : il est trop lent
pour 1621 m en 120 s, et il perd du temps aux endroits où le tracé passe trop près du décor.
La suite est sur **AI-PATH** (95 liens de plus de 25 m, tracé jusqu'à 46 m entre deux
points).

## Pistes secondaires

- Aucune détection d'obstacle : la voiture vise le waypoint en ligne droite.
- `Vehicle::ApplyInput` ignore `_topSpeedKmh` : la force moteur est la même à toute vitesse,
  seule la traînée limite. À brancher avec SCRIPT-B.

## Critères d'acceptation
- [x] Un `RaceOpponent` suit le circuit en pilotant un `Vehicle` via `ApplyInput`.
- [x] Le rubber-banding accélère l'adversaire distancé et le ralentit quand il est en tête, dans une plage bornée (observé à 0,85 en tête).
- [ ] L'adversaire termine un tour complet sans rester bloqué — **pas encore** : 67 waypoints sur 126 dans les 120 s du chrono, la mission s'arrêtant sur le temps et non sur un blocage.
- [x] L'adversaire ne sort plus du monde et ne reste plus bloqué indéfiniment (hors-tracé, chute et blocage sont tous rattrapés ; 0 chute et 0 remise d'office sur un tour complet).
- [x] L'adversaire roule à une vitesse crédible (25-53 km/h) en freinant pour les virages.
- [ ] SCRIPT-D peut instancier, démarrer et arrêter l'adversaire — `ScriptEngine` le crée et le détruit, mais les commandes `SCRIPT-D` elles-mêmes restent des stubs.
