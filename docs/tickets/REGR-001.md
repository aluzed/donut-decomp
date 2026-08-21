# REGR-001 — Crash au déplacement : réactiver la boucle gameplay désactivée

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (bloquant)
- **Module:** Game / Régression
- **Depends on:** —
- **Blocks:** GAME-010, LEVEL-TRIG, PHYS-004, SCRIPT-E, SCRIPT-G
- **Files:** `src/Game.cpp:813` (bloc gameplay), `src/Game.cpp:643` (dégâts de collision)

## Contexte

Le commit `db9c8db` (« debug: disable collision damage + pedestrians/collectibles to
isolate crash ») a commenté toute la boucle gameplay par-frame pour isoler un crash qui
se produit au déplacement du personnage. Le bloc n'a **jamais été réactivé** : depuis, le
code de PedestrianManager, CollectibleManager, du système de triggers et des dégâts de
collision existe mais **ne s'exécute pas**.

Le bloc désactivé (`Game.cpp:695-723`) fait, quand `_gameState == InGame` :
- `_level->CheckTrigger(...)` → détection d'entrée en zone trigger ;
- spawn + `_pedestrianManager->Update(...)` toutes les 0.5 s ;
- `_collectibleManager->Update(...)`.

Le bloc dégâts (`Game.cpp:574`, « collision damage disabled for debugging ») est désactivé
séparément.

## Objectif

Diagnostiquer la cause du crash, la corriger, puis **décommenter** les deux blocs pour que
piétons / collectibles / triggers / dégâts tournent à nouveau, sans crash.

## Repro

1. Lancer `./build/bin/donut`, charger un niveau, passer en `InGame` (personnage à pied).
2. Décommenter temporairement `Game.cpp:695-723`, rebuild.
3. Appuyer sur avancer (flèche haut / W) et se déplacer.
4. Observer le crash (le commit pointe ce scénario précis).

## Pistes d'investigation

- Lancer sous `gdb` (`gdb --args ./build/bin/donut`, `run`, `bt` au crash) pour localiser la
  frame fautive — probablement dans `PedestrianManager::Spawn/Update`, `CollectibleManager::Update`
  ou `Level::CheckTrigger`.
- Hypothèses à écarter dans l'ordre :
  1. **Déréférencement nul** : un manager utilisé avant init, ou `_character`/`_level` null
     dans un état transitoire (vérifier les gardes `if (_pedestrianManager)` suffisent).
  2. **Itérateur invalidé** : spawn de piétons/collectibles pendant une itération de la même
     collection (le spawn et l'Update sont dans le même bloc 0.5 s).
  3. **Accès physique concurrent** : ajout d'un corps Bullet (`Spawn`) au mauvais moment du
     `stepSimulation` → modifier le monde physique hors de la fenêtre sûre.
  4. **Bounds/locator manquant** : `CheckTrigger` sur un niveau sans triggers parsés.
- Vérifier que `PedestrianManager::Spawn` n'est pas appelé chaque frame de façon non bornée
  (fuite de bodies) avant de réactiver.

## Étapes

1. Reproduire le crash et capturer la backtrace.
2. Identifier la cause racine (ne pas se contenter d'un garde null si l'invalidation
   d'itérateur ou l'accès physique est en cause).
3. Corriger à la racine.
4. Décommenter `Game.cpp:695-723` **et** réactiver le bloc dégâts `Game.cpp:574`.
5. Retirer les commentaires « disabled for debugging » devenus obsolètes.

## Cause racine (résolue)

Le crash n'a **jamais eu de cause dans le bloc désactivé**. `db9c8db` (18 juin) a commenté
ces blocs pour isoler un crash « appuyer sur flèche haut → crash » ; **le même jour**,
`9860a4f` (« fix(gameplay): character spawn camera + movement + step controller ») a corrigé
les quatre bugs qui produisaient exactement ce symptôme :

- `Character::_rotation` construit par `Quaternion()`, dont le constructeur laisse X/Y/Z/W
  **non initialisés** — la caméra de suivi calculait `rot * offset` sur des ordures et
  partait à ~1e6 unités au spawn ;
- `stepUp()` soulevait la capsule sans que `stepDown()` ne rende la hauteur gagnée ;
- `stepDown()` balayait depuis `_position` (pré-déplacement) au lieu de `_targetPosition` ;
- le raycast `onGround()` (0.2u) était plus court que la demi-hauteur de la capsule (~0.875u).

Le correctif a été livré mais **personne n'est revenu décommenter les blocs** : le
désactivage de debug a survécu à son motif pendant 22 commits.

## Vérification (2026-08-22, Windows / GCC 16 / UCRT)

Reproduction tentée sur le build actuel avec les blocs réactivés :

- **91 882 exécutions** du bloc gameplay sur ~90 s de marche + conduite : **aucun crash**.
- « player entered trigger zone » émis **34 fois** → les volumes de trigger répondent.
- Collectibles : 1/10 ramassé au contact.
- Piétons : 1 → 15 (plafond `_maxPedestrians`), positions qui évoluent d'une frame à l'autre.
- A/B : bloc **désactivé**, marche 25 s → le joueur traverse quand même le sol 21 fois.
  La chute est donc **indépendante** de ce ticket (→ voir `CHARCTRL-FALL`).

## Critères d'acceptation

- [x] Le bloc `Game.cpp:695-723` est décommenté (plus de `/* */`).
- [x] Les dégâts de collision (`Game.cpp:574`) sont réactivés — le corps de la boucle, supprimé par `db9c8db`, a été restauré depuis `db9c8db^`.
- [x] Se déplacer à pied **et** en véhicule pendant ≥ 60 s ne crashe pas (~90 s testées).
- [x] Des piétons apparaissent et se déplacent autour du joueur (vérifié en instrumentant le compteur et la position ; la confirmation *visuelle* reste bloquée par `UI-SPLASH`).
- [x] Les collectibles se mettent à jour (collecte fonctionnelle au contact).
- [x] L'entrée en zone trigger émet le log « entered trigger zone ».
- [x] La cause racine est notée en commentaire ou dans le message de commit (pas juste un
      garde null masquant le symptôme) — cf. § Cause racine et le commentaire à `Game.cpp:813`.
