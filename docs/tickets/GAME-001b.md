# GAME-001b — Extraire GameCamera (orbite/suivi/freecam, ~Game.cpp:403-554)

- **Status:** TODO
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.cpp:403-414` (orbit yaw/pitch souris), `src/Game.cpp:529-565` (caméra véhicule + orbite perso), `src/Game.cpp:578-585` (shake), `src/Game.h:141-143` (`_smoothCamPos`/`_camYaw`/`_camPitch`), `src/Game.h:100` (`_camera`), `src/Game.h:134` (`_shakeAmount`)

## Contexte
La logique caméra est dispersée dans `Game::Run` : accumulation orbit `_camYaw`/`_camPitch`
depuis la souris avec clamp (`Game.cpp:410-414`), caméra de suivi véhicule lissée
(`Game.cpp:531-546`), caméra orbite tierce-personne du personnage (`Game.cpp:547-565`), et
shake (`Game.cpp:578-585`). Les membres `_smoothCamPos`/`_camYaw`/`_camPitch` (`Game.h:141-143`),
`_camera` (`Game.h:100`) et `_shakeAmount` (`Game.h:134`) appartiennent à `Game`.

## Approche
1. Créer `src/Game/GameCamera.{h,cpp}` possédant le `FreeCamera` (`_camera`, `Game.h:100`) et
   l'état `_smoothCamPos`/`_camYaw`/`_camPitch` (`Game.h:141-143`).
2. Y déplacer : l'accumulation orbit souris + clamp (`Game.cpp:408-414`), le mode suivi
   véhicule (`Game.cpp:531-546`), le mode orbite personnage (`Game.cpp:547-565`), et le shake
   (`Game.cpp:578-585`, exposer `AddShake` actuellement `Game.h:84`).
3. API type `GameCamera::Update(deltaTime, camTargetPos, camTargetRot, inVehicle, mouseDX, mouseDY)`
   et getters `GetViewMatrix`/`GetProjectionMatrix`/`GetPosition` pour `Game.cpp:839-843`.
4. Supprimer tous les membres `_cam*`, `_smoothCamPos`, `_shakeAmount`, `_camera` de `Game.h`
   (déplacés dans `GameCamera`) ; `Game` ne garde qu'un `std::unique_ptr<GameCamera>`.

## Critères d'acceptation
- [ ] Aucune variable `_cam*` (`_camYaw`/`_camPitch`/`_smoothCamPos`/`_camera`/`_shakeAmount`) ne reste dans `Game`.
- [ ] Le comportement caméra est identique : orbite souris tierce-personne, suivi véhicule lissé, clamp pitch, shake.
- [ ] L'overlay debug caméra (`Game.cpp:803-816`) lit toujours la position/orientation via `GameCamera`.
- [ ] Le jeu compile et la caméra se comporte à l'identique.
