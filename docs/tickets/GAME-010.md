# GAME-010 — Système de collectibles (pièces, 7 cartes, ~50 gags) avec état persistant

- **Status:** PARTIAL (2026-10-18: coins, cards)
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

## Coins (2026-10-18)

- **Where they lie.** The level's own coins: one per coin locator (type 14), in every file
  loaded. Level 1 has 177 of them. `Level::GetCoinPositions`; the scatter along the longest
  path stays as a fallback for a level without any.
- **Picking up** runs every frame, from the car when the player drives: once every half
  second, a car at speed went straight past them.
- **The purse.** Every coin picked up goes into it, and it is kept across sessions in
  `donut_coins.dat` (gitignored). The HUD shows the purse. The coins are laid out again on
  each mission load, as the game's come back when a zone reloads.
- **Spending.** A `coins` objective (`SetCoinFee`, gr1's 20-coin entry) is met as soon as
  the purse holds the fee, which is then taken; the objective line shows "12/20" until
  then. `--autopilot` runs neither earn nor pay: they are let in for free.

Verified: a purse of 25 pays gr1's fee and leaves 5; a purse of 3 holds the mission at its
first stage; walking onto a coin adds it to the purse and the file.

- **Their look.** Every level script names `coinShape_000` as the coin drawable; it is in
  `art/missions/level01/level.p3d`, which the game now loads with the level. The coins are
  drawn with it, spinning, with the level's world shader; the gold wire boxes remain only
  when the model is missing. A geometry from the resource manager must be committed before
  it is drawn, or `Mesh::Draw` binds a null vertex binding and crashes.

## Cards (2026-10-18)

Level 1's seven collector cards stand at the locators named `card` and two digits (level,
card): `card11` to `card17`, except that the one which should be `card14` is a second
`card13`. A card is therefore known by its name **and** its position.

Walking (or driving) within 3m of one finds it for good: found cards are written to
`donut_cards.dat` (gitignored), are not laid out again, and the HUD's card row counts them.
`--autopilot` finds nothing. Verified by standing on `card11`: found, 1/7, and still found
(and gone) on the next run.

A card is a billboard group in `cards.p3d` (`card11_bb1Shape`, `card_collect`...) which is
not drawn yet; a turning blue frame marks it meanwhile.

Gags are still to do.
