# RENDER-011 — WorldSphere : billboards (lens flare, halo solaire)

- **Status:** DONE (2026-10-21: the lens flare drawn)
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

## The lens flare (2026-10-21)

Nine world spheres have one (`LensFlare`, 0x3F0000D, "sunshine": levels 1 and 3's
terrains, the bonus game's tracks...). It is four billboard groups -- glares 640 to 3300
units across, of the sun's colours -- on the joints of its own skeleton ("sunshine"), as
its composite drawable places them, and a small shape to test the sun's visibility. All
four sit on the sun (their distance is 0), level 1's at (1903, 838, 2714): 14 degrees
up. Where a joint stands is its bind pose (the inverse of `restGlobalInverse`): its
`finalGlobal` at rest is the skinning's, and put the sun below the ground.

Each glare is seen as the camera looks toward the sun: fully within its display info's
source range (0.52 rad), fading out over its edge range (0.07 to 0.17 rad). It is drawn
over the world (after everything the level draws), not tested against its depth; how
much of the sun shows is asked of the GPU instead -- the smallest glare drawn unseen,
tested against the world's depth and on its own (two `GL_SAMPLES_PASSED` queries), the
answer read a frame later, never waited for -- and the glares dimmed by it: hidden
behind a house, the sun gives none.

The sky's own animation already plays (its skeleton posed by its animation); the old
frame and multi controllers named in the ticket are that.
