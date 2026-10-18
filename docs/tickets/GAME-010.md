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

A card is drawn, turning, with the one card geometry in `cards.p3d` (`card11_geoShapeh`),
whose shaders, textures and geometries are loaded at start-up; the rest of that file is the
pick-up effect (billboards, particles), not played. Without the file, a blue frame marks the
card. Seen on screen (drawn for the check on the lawn in front of Homer): the Buzz Cola card,
textured, turning, about Homer's height and leaning on its axis as authored. `card11` itself
stands 3m up, inside Bart's treehouse.

Gags are still to do.

## Wrenches and phone booths (2026-10-18)

The level's action locators (type 9) carry three strings: the object, its joint or switch,
and the action -- `SummonVehiclePhone`, `Wrench`, `Doorbell`, `PlayAnim`, `ToggleOnOff`...
Only the first was read. `Level::Trigger::action` now holds the third.

- **Wrenches** (`Wrench`, 14 on level 1): drawn with `wrench.p3d`'s `wrench7Shape`,
  turning a metre above the road. Driven over (within 4m), one repairs the car to full
  and is gone until the coins are laid out again. Checked: a car at 40 put on level 1's
  first wrench came back at 100.
- **Phone booths** (`SummonVehiclePhone`): the action button at a booth, on foot, brings
  the player's car (the one the mission gave them; there is no garage yet) to the nearest
  road, repaired, with the player at the wheel. Checked at R1P2 on level 1 m2.

A card found shows its title for 4s, from the phrase table: `CARD_TITLE_nn`, numbered per
level from (level - 1) x 8, the eighth of each level a bonus card. `card11` is "HOME MADE
FOOTBALL"; the seventh of a level adds "LEVEL CARDS COLLECTED!".

**Doorbells** (`Doorbell` action locators, `[DB_quimby | quimby_switch6 | Doorbell]`): the
action button at one, on foot, rings, and the resident named in the first string answers in
one of their recorded `w_doorbell` lines. Checked at Mayor Quimby's (5 lines).

**Gags** (`level.mfk`'s `GagBegin` ... `GagEnd`, 33 on level 1, 12 on level 2): each has a
sound (`GagSetSound`), a trigger (`GagSetTrigger("touch", x, y, z, radius)`, walked or
driven into, or `("action", locator, radius)`, the action button on foot) and often a coin
(`GagSetCoins`). Triggered, a gag plays one of its sounds (`interactive_props\gag_jasp_01`
to `_03`, `nis\gag_s1`: the English set) and pays its coins the first time in the session.
A touch gag plays again on the way back in, no more than every 5s. Checked: the player put
on each of level 1's 33 in turn, all but one found their sound (`gag_loot`, which the data
does not have). Each gag's model (`art/nis/gags/gag_bbq.p3d`) stands where `GagSetPosition`
puts it, in its rest pose: seen, the barbecue by the Simpsons' door. Not done: the
model's animation (its `GagController`) played when the gag is triggered;
`GagSetInterior`, `GagSetPersist`, `GagSetCycle` and the movies; keeping
the gags found from one session to the next.

A gag found for the first time shows the count for 3s, "Gags 3/12" (the phrase table's
`GAGS`), and "LEVEL GAGS COMPLETE!" (`GAGS_COMPLETE`) with the last. Seen in Moe's.
A gag with `GagSetCameraShake` (the power plant's meltdown) shakes the view when it plays,
at full strength: what its three numbers measure is not known.
