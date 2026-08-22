# VEH-SINK — Le véhicule ne bougeait pas et la caméra passait sous le décor

- **Status:** DONE (2026-08-22)
- **Priority:** P1
- **Module:** Physics / Vehicle
- **Depends on:** —
- **Blocks:** AI-RACE, SCRIPT-D, toute mission de course jouable
- **Files:** `src/Vehicle.cpp` (`CreatePhysicsBody`), `src/Physics/WorldPhysics.cpp`, `src/Game.cpp` (caméra véhicule), `src/Scripting/ScriptEngine.cpp` (`SelectMission`)

## Contexte

Découvert après `SCRIPT-PARSE` (2026-08-22), qui a rendu le véhicule de mission embarquable
pour la première fois. Une fois dedans, la caméra passe sous le décor (Y ≈ -1,9) et le
compteur reste à 0 : la voiture traverse le terrain.

## Ce qui a été corrigé (nécessaire mais pas suffisant)

**Le décor statique était ajouté en `btCollisionObject` nu.** `btDefaultVehicleRaycaster` —
le raycaster de roue derrière `btRaycastVehicle` — rejette tout impact dont l'objet ne
s'*upcaste* pas en `btRigidBody` :

```cpp
const btRigidBody* body = btRigidBody::upcast(rayCallback.m_collisionObject);
if (body && body->hasContactResponse()) { ...  }
return 0;
```

Les roues ne trouvaient donc jamais le sol : ni suspension, ni adhérence. Le personnage, lui,
n'était pas concerné : il interroge le monde directement par `convexSweepTest`.

`WorldPhysics::addStaticBody()` crée désormais les 4103 objets de collision (trimeshes
`Intersect`, OBB, sphères, cylindres, barrières) en `btRigidBody` de masse nulle. Vérifié :
avant embarquement la voiture **repose** sur le sol (Y stable à 2,98, corps endormi, vitesse
nulle) au lieu de tomber indéfiniment ; et la marche du personnage ne régresse pas
(60 s, 0 « fell off the map »).

## Correction du diagnostic

**Le titre initial était faux : le véhicule ne s'enfonçait pas.** J'avais lu la position de la
*caméra* (Y ≈ -1,9) comme celle de la voiture. Mesuré ensuite image par image, le châssis
restait à Y = 2,98, immobile et endormi. Trois vrais problèmes se cumulaient :

1. **Décor statique en `btCollisionObject` nu** (corrigé plus haut) — les roues ne trouvaient
   pas le sol, donc ni suspension ni adhérence.
2. **Le châssis s'endormait.** `btRaycastVehicle` pilote son corps par `applyEngineForce` /
   `setBrake`, qui **n'activent pas** le rigid body. Une fois Bullet l'ayant endormi
   (`ISLAND_SLEEPING`), la voiture ignorait purement et simplement l'accélérateur : elle
   restait garée moteur à fond. `setActivationState(DISABLE_DEACTIVATION)` — ce que tout
   véhicule Bullet doit faire.
3. **Caméra pivotée par l'orientation complète du châssis.**
   `camTarget + vehRot * Vector3(0, 3, -12)` : le châssis étant un corps physique qui tangue
   et roule, l'offset partait sous le terrain. La rotation de visée, elle, ne prenait déjà que
   le lacet. L'offset est désormais aligné dessus, et le point visé est relevé selon le haut
   *monde* et non celui de la caisse.

Corrigé aussi : `SelectMission` vidait `_missionVehicles` sans détruire la physique, laissant
un corps et une action orphelins dans le monde. Une voiture du premier chargement tombait
ainsi indéfiniment (Y = -440 relevé).

## Vérification

- Accélération : le compteur monte à **58 km/h**, l'arc et l'aiguille suivent.
- 55 s de conduite avec virages : la voiture parcourt Springfield (X 80 → 150), **0**
  « vehicle fell off the map », **0** « player fell off the map », aucun crash.
- Caméra au-dessus du sol pendant toute la conduite (Y de 5,5 à 8,4), vue de conduite normale.

## Reste à faire (hors périmètre)

- Aucun checkpoint franchi pendant le test : je conduisais sans savoir où ils sont, le HUD
  n'affiche pas encore de guidage. À reprendre avec `AI-RACE` / `SCRIPT-D`.
- Le modèle du véhicule n'est pas visible depuis la caméra de conduite.

## Pistes envisagées à l'époque (conservées pour mémoire)

Toutes fausses ou sans effet :

- `maxSuspensionTravelCm = 500` (5 m !) dans `CreatePhysicsBody` — une compression aussi
  large peut laisser le châssis passer sous la surface.
- Les points d'ancrage des roues sont à `Y = 0` en local, soit le centre du châssis, avec un
  rayon de rayon utile de `suspensionRestLength + wheelRadius = 0,6` alors que la demi-hauteur
  de la boîte est 0,4. La marge sous la caisse n'est que de 0,2.
- Le personnage n'est pas retiré du monde physique à l'embarquement : sa capsule peut pousser
  le châssis.
- CCD non activé sur le châssis (`setCcdMotionThreshold` / `setCcdSweptSphereRadius`).

## Piège de mesure à connaître

`btRaycastVehicle::updateWheelTransformsWS()` remet `m_raycastInfo.m_isInContact` à `false`
en début d'appel. Comme `Vehicle::Update()` appelle `updateWheelTransform()` à chaque frame de
rendu (~650 Hz) alors que l'action physique ne tourne qu'à 60 Hz, **lire `m_isInContact`
depuis `Vehicle::Update` renvoie presque toujours faux**, même quand les roues touchent
réellement. J'ai perdu deux itérations dessus : ne pas conclure « aucune roue au sol » à
partir de cette lecture.

## Critères d'acceptation

- [x] Rouler 60 s ne fait pas passer le véhicule sous le terrain (55 s testées, 0 chute).
- [x] Le compteur de vitesse affiche une valeur non nulle en accélérant (58 km/h).
- [ ] Franchir un checkpoint incrémente `CP n/6` dans le HUD — les repères existent désormais (cf. [UI-RACE-GUIDE](UI-RACE-GUIDE.md)), mais le franchissement lui-même reste à confirmer à la main.
