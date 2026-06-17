# Tickets Render

## RENDER-001 — Implémenter SkinAnimation::Track::Evaluate
**Priorité :** P0 (bloquant)
**Fichier :** `src/Render/SkinAnimation.cpp:20`
**Description :** TOUT le corps de `Track::Evaluate()` est commenté. Retourne `Matrix4x4::Identity`. Sans ça, aucune animation squelettique ne fonctionne.
**Correctif :** 
1. Implémenter la boucle d'évaluation : trouver les keyframes, interpoler translation (lerp) et rotation (slerp/quat lerp), composer la matrice
2. `TranslationKey::Lerp` et `RotationKey::Lerp` sont aussi des stubs (retournent `b`)
3. Utiliser `glm::mix` pour translation, `glm::slerp` pour rotation (ou implémenter manuellement)

---

## RENDER-002 — Réparer LineRenderer::DrawBox (Matrix4x4)
**Priorité :** P1
**Fichier :** `src/Render/LineRenderer.cpp`
**Description :** Les 12 appels à `DrawLine` pour les arêtes de la boîte sont commentés.
**Correctif :** Décommenter et vérifier que les vertices sont corrects.

---

## RENDER-003 — Réparer LineRenderer::DrawBox (Quaternion + Euler)
**Priorité :** P1
**Fichier :** `src/Render/LineRenderer.cpp`
**Description :** 
- `DrawBox(Quaternion)` utilise `Matrix4x4::Identity` au lieu de la rotation passée
- `DrawBox(Euler)` est entièrement commenté
**Correctif :** Convertir le quaternion en matrice de rotation, composer avec la position.

---

## RENDER-004 — Réactiver SkinModel::UpdateAnimation
**Priorité :** P1
**Fichier :** `src/Render/SkinModel.h` 
**Description :** `UpdateAnimation` est déclaré mais commenté. Le skinning GPU fonctionne (les bone matrices sont uploadées) mais l'update d'animation n'est pas branché sur SkinModel.
**Correctif :** Connecter `Skeleton::UpdatePose()` et l'upload des matrices vers `TextureBuffer`.

---

## RENDER-005 — Implémenter le skinning dans le vertex shader
**Priorité :** P1
**Fichier :** Shaders (à vérifier dans `Game.cpp` où `_skinShader` est créé)
**Description :** Vérifier que le vertex shader pour le skinning lit correctement les bone indices/weights et applique les matrices depuis le `TextureBuffer`. Le pipeline existe (Character::Draw bind le boneBuffer) mais à valider.

---

## RENDER-006 — Implémenter CompositeModel_AnimObjectWrapper (shaders/textures)
**Priorité :** P1
**Fichier :** `src/Render/CompositeModel.h:22`
**Description :** Le constructeur laisse `_shaders` et `_textures` vides. Les `AnimObjectWrapper` sont utilisés pour les objets animés du monde (feux de circulation, distributeurs...).
**Correctif :** Parcourir le wrapper P3D pour extraire shaders et textures.

---

## RENDER-007 — Implémenter CompositeDrawable (méthodes)
**Priorité :** P2
**Fichier :** `src/Render/CompositeModel.h:90`
**Description :** La classe `CompositeDrawable` est déclarée avec seulement un constructeur, pas de `Draw()`.

---

## RENDER-008 — LOD et culling spatial
**Priorité :** P2
**Description :** Ajouter un système de niveau de détail (LOD) basé sur les chunks Tree (P3D-007) pour les grands niveaux. Sans ça, les performances sur les niveaux complets seront mauvaises.
**Approche :** 
1. Implémenter un octree ou utiliser les Tree/TreeNode chunks
2. Frustum culling par zone
3. DynaLoad streaming (chargement/déchargement de zones)

---

## RENDER-009 — Post-processing pipeline
**Priorité :** P3
**Description :** Effets visuels du jeu original :
- Motion blur
- Glow/bloom
- Heat haze (chaleur sur la route)
- Depth of field (cutscenes)
- Iris wipe (transition)

---

## RENDER-010 — Support du fog
**Priorité :** P2
**Description :** Le brouillard est un paramètre P3D (champ `fog` dans BillboardQuadGroup, + paramètres globaux). Implémenter le fog dans les shaders world/instanced.

---

## RENDER-011 — Billboards dans WorldSphere
**Priorité :** P2
**Fichier :** `src/Render/WorldSphere.cpp`
**Description :** Le chargement des billboards est commenté (`//_billboardBatches.push_back...`). Les WorldSpheres peuvent contenir des billboards (lens flare, halo solaire...).

---

## RENDER-012 — Shadow mapping
**Priorité :** P3
**Description :** Les ombres dans le jeu original sont des shadow volumes ou shadow maps simples. Ajouter des ombres portées basiques (sun shadow map).

---

## RENDER-013 — ShaderManager / précompilation des shaders
**Priorité :** P3 (optimisation)
**Description :** Les shaders sont recompilés à chaque lancement depuis `world.vert/frag` etc. Ajouter un cache binaire OpenGL ou précompiler les variantes.

---

## RENDER-014 — Vérifier le support multi-UV channels
**Priorité :** P2
**Description :** Les primitives P3D peuvent avoir plusieurs canaux UV (`UVList` avec `channel`). Le jeu utilise le canal 0 pour la diffuse, canal 1 pour le lightmap. Vérifier le vertex layout.

---

## RENDER-015 — Lightmaps et éclairage statique
**Priorité :** P2
**Description :** Les niveaux du jeu ont des lightmaps pré-calculées. Les shaders doivent sampler une deuxième texture pour l'éclairage statique.
