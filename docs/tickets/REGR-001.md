# REGR-001 — Crash au déplacement : réactiver la boucle gameplay désactivée

- **Status:** TODO
- **Priority:** P0 (bloquant)
- **Module:** Game / Régression
- **Depends on:** —
- **Blocks:** GAME-010, LEVEL-TRIG, PHYS-004, SCRIPT-E, SCRIPT-G
- **Files:** `src/Game.cpp:695-723` (bloc `/* */`), `src/Game.cpp:574` (« collision damage disabled »)

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

## Critères d'acceptation

- [ ] Le bloc `Game.cpp:695-723` est décommenté (plus de `/* */`).
- [ ] Les dégâts de collision (`Game.cpp:574`) sont réactivés.
- [ ] Se déplacer à pied **et** en véhicule pendant ≥ 60 s ne crashe pas.
- [ ] Des piétons apparaissent et se déplacent autour du joueur.
- [ ] Les collectibles se mettent à jour (collecte fonctionnelle au contact).
- [ ] L'entrée en zone trigger émet le log « entered trigger zone ».
- [ ] La cause racine est notée en commentaire ou dans le message de commit (pas juste un
      garde null masquant le symptôme).
