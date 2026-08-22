# VEH-SINK — Le véhicule s'enfonce sous le terrain une fois embarqué

- **Status:** TODO (une cause corrigée, symptôme toujours présent)
- **Priority:** P1
- **Module:** Physics / Vehicle
- **Depends on:** —
- **Blocks:** AI-RACE, SCRIPT-D, toute mission de course jouable
- **Files:** `src/Vehicle.cpp` (`CreatePhysicsBody`), `src/Physics/WorldPhysics.cpp`

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

## Ce qui reste

Après embarquement, le véhicule descend quand même sous le terrain. Pistes non explorées :

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

- [ ] Rouler 60 s ne fait pas passer le véhicule sous le terrain.
- [ ] Le compteur de vitesse affiche une valeur non nulle en accélérant.
- [ ] Franchir un checkpoint incrémente `CP n/6` dans le HUD.
