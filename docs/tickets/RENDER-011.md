# RENDER-011 — WorldSphere : billboards (lens flare, halo solaire)

- **Status:** TODO
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/WorldSphere.cpp:24-28`, `:130-141`, `:152-153`, `src/Render/BillboardBatch.cpp`

## Contexte
Le `WorldSphere` charge déjà ses `BillboardBatch` (`WorldSphere.cpp:24-28`) et les
dessine (`:152-153`), mais la liste TODO en fin de constructeur
(`WorldSphere.cpp:130-141`) signale que les éléments « Lens Flare », « Old Frame
Controller » et « Multi Controller » du chunk ne sont pas traités. Le halo
solaire / lens flare (billboards orientés caméra, screen-space) n'est donc pas
rendu.

## Approche
1. Identifier les chunks restants dans `P3D::WorldSphere` (grep
   `src/P3D/P3D.generated.h` : `LensFlare`, `MultiController`, `FrameController`).
   Ajouter leur chargement dans le constructeur `WorldSphere::WorldSphere`
   (`WorldSphere.cpp:13`).
2. Pour le lens flare : créer/étendre un `BillboardBatch` orienté caméra. Vérifier
   `BillboardBatch.cpp` et `assets/shaders/billboard_batch.vert` pour le mode de
   facing (le vert doit billboarder face caméra à partir du `viewProj`).
3. Brancher le rendu dans `WorldSphere::Draw` (`WorldSphere.cpp:143-156`), après
   les batches existants, en désactivant le test de profondeur / depth-write pour
   le halo (additif). Exposer position solaire (joint d'animation ou direction
   lumière).
4. Si `MultiController`/`FrameController` pilotent l'intensité/sélection de frame
   du flare, les câbler sur `_animTime` (cf. `WorldSphere::Update`
   `WorldSphere.cpp:158-166`).

## Critères d'acceptation
- [ ] Un halo/lens flare s'affiche à la position du soleil et suit la caméra.
- [ ] Les billboards restent face caméra quel que soit l'angle de vue.
- [ ] Pas de régression sur les batches de billboards déjà rendus.
