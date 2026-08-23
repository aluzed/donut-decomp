# AI-RACE — IA d'adversaire de course avec rubber-banding

- **Status:** PARTIAL (2026-08-23) — l'adversaire **boucle un tour** : 122 waypoints sur 126 puis passage au tour 1, 39 km/h de moyenne. Reste : trois endroits du circuit ne sont pas roulables et sont contournés d'office
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

Il ne boucle donc toujours pas un tour, mais pour une raison différente : **il est trop lent
pour 1621 m en 120 s**, et aucune IA ne le serait.

### Pourquoi il est trop lent : la force moteur est un nombre magique

`ApplyInput` calculait `throttle * gasScale * 1000 * boost`. Mille newtons pour un châssis de
1200 kg font **0,83 m/s²** : contre sa propre traînée, la voiture plafonne à ~56 km/h quelle
que soit la longueur de la ligne droite, et `_topSpeedKmh` (180 par défaut) n'était jamais
atteint — ni même consulté. Cela vaut pour **toutes** les voitures, celle du joueur comprise,
ce qui explique les « 58 km/h » relevés sur VEH-SINK.

La force dérive maintenant de la masse du châssis (`masse × kEngineAcceleration`), et
`_topSpeedKmh` plafonne le gaz. Mais `kEngineAcceleration` reste à **0,83**, c'est-à-dire
exactement l'ancien comportement, parce que l'augmenter n'est pas gratuit — mesuré :

| `kEngineAcceleration` | waypoint atteint | dégagements |
|---|---|---|
| 0,83 (= ancien 1000 N) | 66-70 / 126 | 4-6 |
| 3,0 | **18** / 126 | 13 |
| 3,0 + anticipation proportionnelle à la vitesse | **18** / 126 | 13 |

Avec plus de couple, l'adversaire arrive dans les virages plus vite qu'il ne peut ralentir et
s'encastre. Il faut donc traiter la tenue de virage **avant** le moteur : freinage plus tôt,
trajectoire de corde, et détection d'obstacle. L'anticipation est désormais un temps (3 s) et
non une distance fixe, ce qui est correct mais n'a pas suffi à lui seul.

Reste aussi, côté tracé, les liens jonction↔dalle longs — cf. **AI-PATH**.

## Pistes secondaires

- Aucune détection d'obstacle : la voiture vise le waypoint en ligne droite.
- `SetMass` n'écrit que le membre `_mass` ; le châssis Bullet reste à 1200 kg, donc la force
  moteur dérivée de la masse ne bouge pas. À brancher avec SCRIPT-B, avec `setMassProps`.


## Le tour est bouclé (2026-08-23, deuxième session)

| | avant | après |
|---|---|---|
| waypoint le plus loin | 66 / 126 | **122 / 126, puis tour 1** |
| vitesse moyenne | 23 km/h | **39 km/h** |
| dégagements en marche arrière | 6 | **3** |
| sortie du processus | segfault | **code 0** |

Reproduit à l'identique sur trois exécutions consécutives : 122/126, tour 1, 3 dégagements.

### Ce qui bloquait la mesure, et non l'IA

Deux défauts rendaient tout chiffre faux, et il fallait les traiter d'abord.

**`Game::~Game` détruisait le monde physique avant le personnage.** Les membres sont détruits
dans l'ordre inverse de leur déclaration, ce qui plaçait `_worldPhysics` avant `_character` :
`~CharacterController` appelait `removeAction(this)` à travers un monde déjà libéré et le
processus mourait **à chaque sortie propre**. Personne ne l'avait vu, parce qu'un plantage à
la sortie ressemble à une fermeture normale.

**Et il emportait la fin du journal.** Redirigé vers un fichier, `stdout` est bufferisé par
blocs : le segfault perdait tout ce qui n'avait pas encore été écrit. Des exécutions entières
revenaient tronquées — une course où l'adversaire atteignait le waypoint 122 se lisait
« bloqué au waypoint 5 après cinq secondes ». Les mesures prises avant ce correctif sont à
relire avec cette réserve, y compris le « 18 waypoints à 3,0 m/s² » de la session précédente.
`stdout` et `stderr` sont désormais en tampon de ligne, et l'ordre de destruction est
explicite.

### Harnais de mesure

`--autostart` (pas de splash, entrée directe en mission) et `--quit-after <s>` (fermeture au
bout de N secondes de boucle). Une mission de 120 s se mesure donc sans personne devant le
clavier :

```
build/bin/donut.exe --log-level debug --autostart --quit-after 140 > run.log
```

### Les quatre corrections de pilotage

1. **Profil de vitesse en deux temps** — remplace la somme des angles sur 35 m, qui ne disait
   rien du *moment* où freiner : la voiture arrivait au virage en ayant déjà besoin d'être
   lente. Chaque virage a maintenant la vitesse que son rayon et l'adhérence permettent,
   `v = sqrt(a_lat · r)` (rayon = cercle circonscrit à trois waypoints), et chacune est
   ramenée jusqu'à la voiture par la distance de freinage,
   `v = sqrt(v_virage² + 2 · a_frein · s)`. La plus basse sur les 100 m d'anticipation
   l'emporte. Le freinage démarre donc exactement aussi tôt que le virage l'exige — et plus
   tôt de lui-même quand la voiture va plus vite, ce qui est la propriété qui manquait.

2. **Poursuite pure** — la cible n'est plus le waypoint suivant, qui saute latéralement dès
   qu'il est capturé (la voiture sciait le volant), mais un point qui **glisse le long du
   tracé**, une seconde en avant. Il coupe le virage à la corde de lui-même. La distance de
   visée est plafonnée à `sqrt(8 · R · couloir)` — la corde dont la flèche reste dans une
   demi-largeur de rue — sans quoi la trajectoire coupe par le trottoir.

3. **Braquage dégressif avec la vitesse** (`Vehicle::ApplyInput`, donc **le joueur aussi**).
   Une demi-radian de braquage est juste au pas et fait partir la voiture en tête-à-queue à
   90 km/h : on retrouvait l'adversaire à l'arrêt, plein gaz, volant à fond, **face à
   l'envers**. C'est ce qui interdisait toute augmentation de la puissance moteur.

4. **Zone morte de l'accélérateur** — le gaz était remonté à son plancher de 0,55 même
   au-dessus de la vitesse visée. Entre « trop vite pour accélérer » et « assez vite pour
   freiner », la voiture gardait donc le pied dedans et emportait l'excédent dans le virage.
   Gaz coupé et freins relâchés est désormais un état possible.

### Le test de blocage mesurait la mauvaise chose

Il demandait si la voiture avait *parcouru du chemin*, pas si elle *avançait dans la course*.
Une voiture qui rebondit sur un mur recule et repart dedans toutes les trois secondes : huit
mètres au compteur, indiscernable de quelqu'un qui roule. L'adversaire a passé les vingt
dernières secondes de la course à faire exactement cela sans jamais être déclaré bloqué.

Le progrès, c'est atteindre des waypoints. Douze secondes sans en valider un et la voiture est
déplacée — **au-delà** de l'obstacle (30 m de dégagement) et non dessus, sans quoi elle
retombe dans le même trou. Les blocages se comptent aussi **par endroit** (deux blocages à
moins de 15 m l'un de l'autre sont le même) et non par fenêtres consécutives : chaque
dégagement réussi remettait le compteur à zéro, si bien que l'abandon n'arrivait jamais —
14 dégagements dans une course et pas une seule remise sur le circuit.

### Ce qui reste : le circuit n'est pas roulable partout

La sonde d'obstacle ajoutée ici (`WorldPhysics::CastRay`, tirée depuis le pare-chocs quand la
voiture se déclare bloquée) donne le point touché **et sa normale**, ce qui distingue enfin un
mur d'une pente — et d'un plafond. Trois défauts, tous de tracé ou de géométrie, aucun d'IA :

| endroit | waypoints | ce que dit la sonde |
|---|---|---|
| (231, 4.2, -336) | 18-19 | normale **(0,00, -0,98, 0,19)** : elle pointe **vers le bas**. La voiture est sous le tablier de la route, coincée contre son dessous à Y 4,5 alors qu'elle roule à Y 4,2. C'est l'explication de l'observation « sur la route et à moitié enfoncée dedans » notée plus haut |
| (~63, 3.2, -601) → (48, 1.6, -622) | 57-70 | mur franc, normale horizontale (0,72, 0,00, 0,70) |
| autour du waypoint 5 | 5 | pas d'obstacle : la voiture **tombe hors du monde** (y = -22), trois fois par tour |

L'adversaire les contourne — 2 sauts de waypoints par tour, plus les remises après chute — et
c'est ce qui permet enfin de mesurer le reste. Mais tant qu'ils sont là, le tour est bouclé
avec des tronçons sautés, et **la vraie correction est côté tracé** : voir AI-PATH.

### La force moteur n'est plus bloquée par l'IA, mais reste à 0,83

| `kEngineAcceleration` | waypoint atteint | tour bouclé |
|---|---|---|
| 0,83 | **122 / 126** | **oui** |
| 3,0 | 116 / 126 | non |

La raison invoquée jusqu'ici pour la garder basse (« à 3,0 l'IA tombe à 18 waypoints ») ne
tient plus : c'était le braquage constant qui envoyait la voiture en tête-à-queue. Mais 0,83
gagne toujours, parce que ce qui reste en travers est de la **géométrie**, et qu'y arriver plus
vite ne fait que taper plus fort. À relever une fois le circuit roulable — et en mesurant alors
la voiture du joueur, qui plafonne elle aussi à ~56 km/h.

## Critères d'acceptation
- [x] Un `RaceOpponent` suit le circuit en pilotant un `Vehicle` via `ApplyInput`.
- [x] Le rubber-banding accélère l'adversaire distancé et le ralentit quand il est en tête, dans une plage bornée (observé à 0,85 en tête).
- [x] L'adversaire termine un tour complet sans rester bloqué — **oui** : 122 waypoints sur 126 puis passage au tour 1, reproduit trois fois. Réserve : deux tronçons non roulables sont sautés d'office et trois chutes hors du monde sont rattrapées.
- [x] L'adversaire ne sort plus du monde et ne reste plus bloqué indéfiniment (hors-tracé, chute et blocage sont tous rattrapés ; 0 chute et 0 remise d'office sur un tour complet).
- [x] L'adversaire roule à une vitesse crédible (39 km/h de moyenne, jusqu'à 76) en freinant pour les virages, sur un profil de vitesse et non sur l'angle de braquage.
- [ ] SCRIPT-D peut instancier, démarrer et arrêter l'adversaire — `ScriptEngine` le crée et le détruit, mais les commandes `SCRIPT-D` elles-mêmes restent des stubs.
