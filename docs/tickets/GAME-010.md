# GAME-010 — Système de collectibles (pièces, 7 cartes, ~50 gags) avec état persistant

- **Status:** TODO
- **Priority:** P2
- **Module:** Game
- **Depends on:** REGR-001
- **Files:** `src/Game.cpp:171-172` (`_collectibleManager` + `SpawnOnPath`), `src/Game.cpp:719-720` (`Update` désactivé), `src/Game.cpp:897-898` (`Draw`), `src/Game.cpp:936-944` (HUD pièces), `src/Game/CollectibleManager.h`

## Contexte
`CollectibleManager` existe et est instancié (`Game.cpp:171`), spawn ses pièces
(`SpawnOnPath`, `Game.cpp:172`) et les dessine (`Game.cpp:898`). Mais sa boucle de collecte
`_collectibleManager->Update(...)` est dans le bloc gameplay commenté (`Game.cpp:719-720`),
donc rien n'est collecté. Le HUD n'affiche que `Coins: collected/total` (`Game.cpp:936-944`).
Aucune catégorie (cartes, gags) ni persistance n'existe.

## Approche
1. **Bloqué par REGR-001** : la collecte ne fonctionnera qu'une fois `Game.cpp:719-720`
   réactivé. Ne pas dupliquer l'appel `Update` ailleurs.
2. Étendre `CollectibleManager` (`src/Game/CollectibleManager.h`) avec un type de collectible
   (Coin, Card[0..6], Gag[0..~49]) et des compteurs par catégorie + un `std::set` d'IDs collectés.
3. Spawner les 7 cartes et ~50 gags à des positions fixes (table d'IDs/positions) en plus des
   pièces de `SpawnOnPath`.
4. Étendre le HUD (`Game.cpp:936-944`) pour afficher `Cards: x/7` et `Gags: x/50` sous `Coins`.
5. Brancher la persistance via GAME-011 (sérialiser le set d'IDs collectés) ; en attendant,
   exposer un getter de l'état collecté.

## Critères d'acceptation
- [ ] Les collectibles ont des catégories distinctes : pièces, 7 cartes, ~50 gags.
- [ ] La collecte au contact fonctionne (dépend de REGR-001 réactivé).
- [ ] Le HUD affiche les compteurs par catégorie.
- [ ] L'ensemble des IDs collectés est exposé pour sérialisation (GAME-011).
- [ ] Le jeu compile et reste jouable.
