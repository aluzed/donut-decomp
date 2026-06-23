# SCRIPT-E — Commandes gags / collectibles

- **Status:** TODO
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** GAME-010, REGR-001
- **Files:** `src/Scripting/GameCommands.cpp:147-165,290-321,402-414`

## Contexte
Tout le système de gags et de collectibles (~51 commandes : `Gag*`, `AddCollectible`, bindings, cartes à collectionner) est un ensemble de stubs vides `{}`. Les gags définis par les missions ne sont ni placés, ni déclenchés, ni récompensés.

## Approche
- Créer dans `ScriptEngine` un builder de gag accumulant l'état entre `GagBegin`/`GagEnd` (motif « begin/setters/end »), puis enregistrer un objet Gag dans une collection persistante.
- Câbler les commandes `Gag*` (`GameCommands.cpp:147-165,304-312,402-403`) :
  - cycle de vie : `GagBegin`, `GagEnd`, `ClearGagBindings`,
  - placement : `GagSetPosition` (2 surcharges), `GagSetInterior`, `GagSetCycle`, `GagSetRandom`, `GagSetWeight`, `GagSetLoadDistances`, `GagSetSoundLoadDistances`,
  - déclencheurs/effets : `GagSetTrigger` (2 surcharges), `GagSetSound`, `GagSetSparkle`, `GagSetPersist`, `GagSetCoins`, `GagSetAnimCollision`, `GagSetIntro`, `GagSetOutro`, `GagSetCameraShake`, `GagPlayFMV`,
  - cartes/films : `GagCheckCollCards`, `GagCheckMovie`.
- Câbler les collectibles : `AddCollectible` (`:93`), `SetTotalGags` (`:36`), `SetCollectibleEffect` (`:112`), `AddGagBinding` (`:290`), `BindCollectibleTo` (`:313`), `AddCollectibleStateProp` (`:320`), `AttachStatePropCollectible` (`:414`), `SetCoinDrawable` (`:243`), `SetPickupTarget` (`:321`).
- S'appuyer sur la collecte/trigger livrée par GAME-010 et la régression d'objets de REGR-001 pour le rendu et la détection.

## Critères d'acceptation
- [ ] Un `.con` représentatif de gags/collectibles s'exécute avec 0 avertissement « unimplemented command ».
- [ ] Un gag complet (begin -> position -> trigger -> coins -> end) est enregistré et déclenchable en jeu.
- [ ] Aucune des ~51 commandes gags/collectibles ne reste un `{}` vide.
