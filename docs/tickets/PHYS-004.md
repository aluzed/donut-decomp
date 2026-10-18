# PHYS-004 — Callbacks de collision (dégâts / ramassage / son)

- **Status:** PARTIAL (2026-10-18: impact damage)
- **Priority:** P1
- **Module:** Physics
- **Depends on:** REGR-001
- **Files:** `src/CharacterController.cpp:256-285`, `src/Physics/CollisionCallbacks.h`, `src/Game.cpp:574`, `src/Game.cpp:695-723`

## Contexte
Aujourd'hui seul un test de pénétration/manifold existe dans `recoverFromPenetration` (`CharacterController.cpp:256-285`) : il ne sert qu'à repousser le personnage, sans déclencher aucun événement gameplay. `CollisionCallbacks.h` fournit déjà une `btCollisionWorld::ContactResultCallback` avec une liste de `std::function`, et un `VehicleDamage::ApplyDamage`, mais rien ne les câble. Les dégâts de collision sont par ailleurs désactivés (`Game.cpp:574`).

## Approche
1. Instancier un `CollisionCallbacks` dans `WorldPhysics` (ou exposer un point d'enregistrement) et l'évaluer après `stepSimulation` via `contactTest`/`contactPairTest` sur les objets concernés (joueur, véhicule).
2. Enregistrer trois callbacks via `CollisionCallbacks::AddCallback` :
   - **dégâts** : sur impact véhicule, calculer l'impulsion depuis `btManifoldPoint` (vitesse relative × masse) et appeler `VehicleDamage::ApplyDamage`.
   - **ramassage** : si l'autre `btCollisionObject` est un collectible, notifier `CollectibleManager`.
   - **son** : déclencher un son d'impact au-delà d'un seuil d'impulsion.
3. Réutiliser le parcours manifold existant de `CharacterController.cpp:256-285` pour émettre l'événement piéton plutôt que de dupliquer la boucle.
4. Réactiver le bloc dégâts `Game.cpp:574` (cf. REGR-001) et brancher l'appel par-frame dans le bloc `Game.cpp:695-723`.

## Critères d'acceptation
- [ ] Un `CollisionCallbacks` est évalué chaque frame après `stepSimulation`.
- [ ] Une collision véhicule au-dessus d'un seuil applique des dégâts via `VehicleDamage::ApplyDamage`.
- [ ] Le contact avec un collectible déclenche le ramassage.
- [ ] Un son d'impact est joué au-delà du seuil d'impulsion.
- [ ] Aucune régression de la résolution de pénétration existante.

## Impact damage (2026-10-18)

Done without contact callbacks, in `Game::updateCarDamage`: the player's car loses health
when its **horizontal velocity changes by more than 4 m/s in one frame**, 2.5 points per
m/s beyond that (4 at first: a 70 km/h hit cost 64, and level 3 m3's destroy wrecked the
car four times over), with a camera shake and a thud. That catches every impact, a wall as much as
a car, while driving itself never comes near (full brakes take ~0.2 m/s off a frame).
Height is left out, so a jump's landing is free, and a car that moved more than 5m in a
frame was teleported, not hit.

It replaces the proximity checks, which only hurt the car while its centre was within 1.5m
of another car's: two cars bumper to bumper are 4m apart, so it never happened, and walls
were free. Health is also reset to 100 when a mission starts or is retried; it carried
over before, so a car worn down in one mission started the next close to failing its
`damage` condition.

Verified: sr2 driven at full throttle into the scenery, one impact, 9 m/s lost, -20; the
story run by `--autopilot`, m0 to m5, no impact at all.
