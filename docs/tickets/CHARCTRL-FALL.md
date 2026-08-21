# CHARCTRL-FALL — Le personnage traverse le sol dès qu'il se déplace

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (bloquant : le jeu n'est pas jouable à pied)
- **Module:** Physics / CharacterController
- **Depends on:** —
- **Blocks:** toute vérification visuelle du gameplay à pied (UI-HUD, GAME-010, LEVEL-TRIG)
- **Files:** `src/Physics/WorldPhysics.cpp` (constructeur)

## Contexte

Découvert en validant `REGR-001` (2026-08-22, Windows / GCC 16 / UCRT).

À l'arrêt, le personnage tient correctement sur le terrain (Y stable à ~3.4). **Dès qu'il
avance**, il passe au travers du sol et tombe. Le garde `Game.cpp` (« player fell off the
map, respawning ») le remet à `_lastSafePos`, il retombe aussitôt, et la boucle se répète :

- 25 s de marche avant → **21 respawns** ;
- 90 s de marche + conduite → **100 respawns**.

Comme `_lastSafePos` n'est mis à jour que quand `onGround()` est vrai et qu'il ne l'est
jamais en chute, le joueur est renvoyé indéfiniment au même point.

## Ce qui est déjà écarté

Test A/B avec le bloc gameplay de `REGR-001` **désactivé** : la chute se produit à
l'identique (21 fois sur 25 s). Ce n'est donc **pas** dû aux piétons / collectibles /
triggers, mais bien au contrôleur de personnage ou à la collision du terrain.

## Pistes d'investigation

- `9860a4f` a retouché `stepUp`/`stepDown`/`onGround` (offset de montée rendu par
  `stepDown`, balayage depuis `_targetPosition`, raycast `onGround` allongé). Un de ces
  correctifs peut avoir déplacé le problème plutôt que de le résoudre.
- Vérifier que le balayage vers le bas de `stepDown()` teste bien contre le mesh de
  collision du terrain, et pas seulement contre les volumes P3D : le sol de `L1_TERRA.p3d`
  est-il ajouté au monde Bullet comme `btBvhTriangleMeshShape` ?
- Un `btBvhTriangleMeshShape` statique laisse passer les objets rapides : activer le CCD
  (`setCcdMotionThreshold` / `setCcdSweptSphereRadius`) sur la capsule, ou vérifier que le
  déplacement par frame ne dépasse pas l'épaisseur du triangle.
- Instrumenter `onGround()` pendant le déplacement : renvoie-t-il `false` dès la première
  frame de mouvement, ou seulement après que la chute a commencé ?

## Cause racine (résolue)

**Aucun `btGhostPairCallback` n'était installé sur le broadphase.**

Le personnage est un `btPairCachingGhostObject`. Bullet ne remplit le cache de paires d'un
ghost object *que* si `setInternalGhostPairCallback()` a été appelé sur le broadphase — c'est
la ligne d'initialisation qu'exige tout `btKinematicCharacterController`. Elle n'a jamais été
écrite dans `WorldPhysics::WorldPhysics`. Conséquences en chaîne :

- `recoverFromPenetration()` itérait sur **zéro** paire : le personnage ne se dépénétrait
  jamais du décor ;
- `stepForwardAndStrafe()`, qui répond aux collisions en lisant les manifolds du ghost,
  n'avait **rien à lire** : le déplacement horizontal n'avait donc *aucune* collision.

Le personnage entrait donc dans les murs en marchant. Une fois la capsule à l'intérieur d'un
maillage statique, le balayage descendant de `stepDown()` démarre en recouvrement et Bullet
ne rapporte pas de contact — plus rien ne le retenait, d'où la chute.

Ça explique aussi pourquoi il tenait debout à l'arrêt : `stepDown()` est une requête sur le
monde (`convexSweepTest`), pas sur le ghost, donc elle fonctionnait tant que la capsule
restait à l'extérieur du décor.

## Mesures

| Configuration | Parcours | « fell off the map » |
|---|---|---|
| Avant correctif | 25 s marche avant | 21 |
| Avant correctif | 60 s multi-directions | 56 |
| **Après correctif** | **60 s multi-directions** | **0** |
| Après correctif | 68 s marche 5 directions + véhicule | 2, *après* le téléport debug (`T`) — aucune pendant la marche |

Compteurs internes sur le même parcours de 60 s : balayage descendant `hit=3871 / miss=39`
(avant : il se figeait à `hit=1366` et `miss` grimpait sans fin).

## Pistes écartées

- `recoverFromPenetration` poussant le personnage vers le bas via le `fabs(dist) > 0.2`
  d'origine (au lieu du `dist < 0.0` de Bullet, commenté depuis 2019) : instrumenté, cette
  branche ne se déclenchait **jamais** — c'était une conséquence, pas la cause. La condition
  reste douteuse et mérite son propre ticket si un artefact de dépénétration apparaît.
- Réécrire `stepForwardAndStrafe` avec un vrai `convexSweepTest` (comme le
  `btKinematicCharacterController` amont) : testé, donne aussi 0 chute et réduit les `miss`
  du balayage descendant de 39 à 3, mais **n'est pas nécessaire** — le callback seul suffit.
  À garder en réserve comme durcissement.

## Critères d'acceptation

- [x] Marcher 60 s dans une direction quelconque n'émet **aucun** « fell off the map ».
- [x] `onGround()` reste vrai en marche sur terrain plat (le balayage descendant touche 3871 fois sur 3910).
- [x] Le personnage monte les trottoirs sans gagner ni perdre d'altitude parasite (Y stable à 2.36–2.47 après 60 s de marche).
- [x] La cause racine est identifiée (pas un simple relèvement du seuil de respawn) — cf. § Cause racine.
