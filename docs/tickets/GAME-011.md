# GAME-011 — Sauvegarde/chargement complet (position, véhicule, argent, progression, collectibles)

- **Status:** TODO
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
