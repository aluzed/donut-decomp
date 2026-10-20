# GAME-011 — Sauvegarde/chargement complet (position, véhicule, argent, progression, collectibles)

- **Status:** DONE (2026-10-20: one save, `donut_game.dat`)
- **Priority:** P2
- **Module:** Game
- **Depends on:** GAME-010
- **Files:** `src/Game.cpp:757-760` (`ResetBestTime`), `src/Scripting/ScriptEngine.h` (best-time persisté), `src/Game.cpp:327-342` (`SetPlayerPosition`/`GetPlayerPosition`), `src/Game.cpp:138-139` (`_inVehicle`/`_activeVehicle`), `src/Game/CollectibleManager.h`

## Contexte
Aujourd'hui seul le meilleur temps de course est persisté (via `ScriptEngine`, voir
`ResetBestTime` `Game.cpp:759` et `GetBestTime` utilisé `Game.cpp:1137`). Position du
joueur, état véhicule (`_inVehicle`/`_activeVehicle`, `Game.h:138-139`), argent et progression
de mission ne sont jamais sauvegardés. Aucun format de save unifié n'existe.

## Approche
1. Créer un `SaveGame` (struct + (dé)sérialisation, ex. JSON ou binaire simple) contenant :
   position joueur (`GetPlayerPosition`, `Game.cpp:337`), état véhicule (`_inVehicle`,
   nom du véhicule actif), argent (à ajouter comme membre `_money` dans `Game.h`),
   progression mission (checkpoint courant / best-time depuis `ScriptEngine`),
   et l'ensemble des collectibles collectés (depuis GAME-010 / `CollectibleManager`).
2. Ajouter `Game::SaveGame(path)` / `Game::LoadGame(path)` ; appeler Save à la complétion de
   mission (`Game.cpp:587-604`) et Load au démarrage si un fichier existe.
3. Au Load : `SetPlayerPosition(...)` (`Game.cpp:327`), restaurer argent et collectibles, et
   réinjecter le best-time dans `ScriptEngine`.
4. Conserver la compat avec le best-time existant (ne pas le casser).

## Critères d'acceptation
- [ ] Un fichier de sauvegarde contient position, état véhicule, argent, progression mission et collectibles.
- [ ] `SaveGame`/`LoadGame` existent et sont appelés (save à la fin de mission, load au boot).
- [ ] Après relancement, position, argent et collectibles sont restaurés.
- [ ] Le best-time existant continue de fonctionner.
- [ ] Le jeu compile et reste jouable.

## One save (2026-10-20)

`SaveGame` (`src/Game/SaveGame.{h,cpp}`) keeps the game in one file, `donut_game.dat`:
values by key and sets of entries by name, a tab-separated line each after a
`donut-save 1` header, written whole to a file beside it then moved into place (a write
cut short leaves the last save).

| Kept | As |
|---|---|
| the story reached | `progress`: "<mission> <level>", as donut_progress.dat had it |
| the coins | `coins` |
| a mission's best time | `time:L4:m1sd` |
| the cards, gags, wasp cameras found | sets `card`, `gag`, `wasp`, their ids as before |
| the bonus missions and street races won | set `won`: "L1:bm1", "L2:sr3" (new) |
| a level's cars and clothes bought | sets `car:1`, `skin:1` (new: they were lost with the level) |
| the car and the outfit in use there | `current-car:1`, `worn-skin:1` (new) |

`Game` loads it first thing, before the level reads its cards, gags and wasps; every
writer that had its own file (`saveCoins`, `saveStoryProgress`, `saveGags`,
`CollectibleManager::saveCards`, `WaspManager`, `ScriptEngine::saveBestTimes`) now sets
its part and saves. A level loaded gets its purchases back (`restorePurchases`), the
player in the outfit last worn (`BuySkin`, owned, free); a purchase or a car chosen is
saved (`savePurchases`). An unattended run (`--autopilot`) saves none of it, as before.

Without `donut_game.dat`, the separate files of before (`donut_progress.dat`,
`donut_coins.dat`, `donut_cards.dat`, `donut_gags.dat`, `donut_wasps.dat`,
`donut_save.dat`) are read, and written into it at the first save; they are left as they
were. Checked: a save of 60 coins, level 7's first mission and two best times read from
the old files, written out as the new one, the old ones unchanged (their checksums); a
`donut_game.dat` given a bonus mission and two street races won, a car and an outfit
bought on level 1: the scrapbook's stats count them, level 1's bonus mission (This Old
Shanty) shows in the missions' gallery, and level 1 opens with Homer in that outfit
(h_fat) and the car (the Plow King) at the car start.

Not kept: the player's position. "Resume Game" goes on with the story's next mission,
whose script places the player, as the game does. The scrapbook's stats now count the
bonus missions, street races, clothes and vehicles (GAME-003).
