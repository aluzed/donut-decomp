# RENDER-006 — AnimObjectWrapper : remplir `_shaders` / `_textures`

- **Status:** TODO
- **Priority:** P1
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/CompositeModel.h:37-43`, `src/Render/CompositeModel.cpp:48-83`

## Contexte
`CompositeModel_AnimObjectWrapper` expose `GetShaders()`/`GetTextures()` mais
renvoie les vecteurs membres `_shaders` / `_textures` qui restent **vides** : le
constructeur ne les peuple jamais (contrairement à `CompositeModel_Chunk` qui
collecte shaders et textures depuis ses chunks enfants). Conséquence : un modèle
chargé via `AnimObjectWrapper` n'enregistre aucun shader/texture dans le
`ResourceManager` (boucles `CompositeModel.cpp:81-83`), et les props rendus
tombent sur des shaders manquants.

## Approche
1. Dans `CompositeModel.h:41-43`, vérifier ce que `P3D::AnimObjectWrapper` expose
   réellement (a-t-il `GetShaders()` / `GetTextures()` comme il a déjà
   `GetCompositeDrawables()`, `GetSkeletons()`, `GetGeometries()` utilisés en
   l.30-36 ?). Grep dans `src/P3D/P3D.generated.h` pour `AnimObjectWrapper`.
2. Si l'accesseur existe : faire renvoyer directement
   `_animObjectWrapper.GetShaders()` / `GetTextures()` (comme les autres getters),
   et **supprimer** les membres `_shaders`/`_textures` morts.
3. Sinon : remplir `_shaders`/`_textures` dans un constructeur (transformer
   `CompositeModel_AnimObjectWrapper` pour copier/charger les chunks Shader/Texture
   du wrapper, sur le modèle de `CompositeModel_Chunk` `CompositeModel.cpp:33-42`).
4. Vérifier que `CompositeModel::CompositeModel` (`CompositeModel.cpp:81-83`)
   enregistre bien les ressources retournées via
   `ResourceManager::LoadShader/LoadTexture`.

## Critères d'acceptation
- [ ] `GetShaders()` / `GetTextures()` d'un `AnimObjectWrapper` retournent des
      vecteurs non vides quand le P3D contient des chunks Shader/Texture.
- [ ] Aucun warning « shader manquant » lors du rendu d'un modèle chargé via
      `AnimObjectWrapper`.
- [ ] Plus de membres `_shaders`/`_textures` inutilisés (ou bien réellement
      peuplés).
