# Tickets P3D

> Les chunks P3D sont définis via `dev/codegen/p3d.json` → générés en C++ par `donut-codegen.exe`
> Ajouter un chunk = modifier p3d.json + relancer codegen + implémenter la classe

## P3D-001 — Ajouter les chunks Particle System (8 types)
**Priorité :** P1
**Chunks :** `ParticleSystemFactory` (0x15800), `ParticleSystem` (0x15801), `BaseEmitter` (0x15805), `SpriteEmitter` (0x15806), `ParticleAnimation` (0x15808), `EmitterAnimation` (0x15809), `GeneratorAnimation` (0x1580A), `ParticleInstancingInfo` (0x1580B)

**Description :** Le système de particules est utilisé pour la fumée, le feu, les débris de voiture, les effets de nitro. Le format est partiellement documenté par d'autres projets (OpenC1, Lucas' Simpsons tools).

**Approche :**
1. Reverser les structures binaires depuis `SIMPSONS.EXE` (Ghidra) ou depuis des samples P3D
2. Ajouter les définitions dans `dev/codegen/p3d.json`
3. Régénérer `P3D.generated.h/.cpp`
4. Implémenter le rendu dans `Render/`

---

## P3D-002 — Ajouter les chunks Light (4 types)
**Priorité :** P2
**Chunks :** `Light` (0x13000), `LightDirection` (0x13001), `LightPosition` (0x13002), `LightShadow` (0x13004), `LightGroup` (0x2380)

**Description :** Éclairage statique/dynamique des niveaux. Le jeu original a des lightmaps pré-calculées et des lumières dynamiques (phares de voiture).

---

## P3D-003 — Ajouter les chunks Expression/Facial Animation
**Priorité :** P3
**Chunks :** `Expression` (0x21000), `ExpressionGroup` (0x21001), `ExpressionMixer` (0x21002), `ExpressionOffsets` (0x10018)

**Description :** Animations faciales des personnages (dialogues). Basse priorité car nécessite le skinning facial en premier.

---

## P3D-004 — Ajouter les chunks Animation Controller (8 types)
**Priorité :** P2
**Chunks :** `FrameController` (0x121200), `FrameController2` (0x121201), `MultiController2` (0x121202), `VertexAnimKeyFrame` (0x121304), `VectorOffsetList` (0x121301), `Float1Channel` (0x121100), `Float2Channel` (0x121101), `Vector1Channel` (0x121102)

**Description :** Canaux d'animation supplémentaires (float, int, bool, entity) non encore supportés. Utilisés pour les contrôleurs avancés (vitesse de roue, animation de phares...).

---

## P3D-005 — Ajouter les chunks Frontend manquants
**Priorité :** P2
**Chunks :** `FrontendStringHardCoded` (0x1800C), `FrontendTextBible` (0x1800D), `FrontendLanguage` (0x1800E), `FrontendObjectResource` (0x18101), `FrontendTextStyleResource` (0x18104), `FrontendTextBibleResource` (0x18105)

**Description :** Ressources de texte localisé et bibles de traduction. Nécessaire pour les menus complets.

---

## P3D-006 — Ajouter les chunks Entity manquants
**Priorité :** P1
**Chunks :** `InstancedStaticEntity` (0x3F00009), `Anim` (0x3F0000C), `AnimDynamicPhysicsWrapper` (0x3F0000F), `AnimCollision` (0x3F00008)

**Description :** Variantes d'entités non encore supportées. InstancedStaticEntity est utilisé pour les props répétés (arbres, lampadaires...).

---

## P3D-007 — Ajouter les chunks Tree (spatial)
**Priorité :** P2
**Chunks :** `Tree` (0x3F00004), `TreeNode` (0x3F00005), `TreeNode2` (0x3F00006)

**Description :** Structure spatiale (probablement un octree/kd-tree) pour le culling et les requêtes spatiales. Le jeu original l'utilise pour le chargement dynamique des zones (DynaLoad).

---

## P3D-008 — Ajouter les chunks Route/Level manquants
**Priorité :** P1
**Chunks :** `Spline` (0x3000007), `LocatorMatrix` (0x300000C), `SurfaceTypeList` (0x300000E)

**Description :** Splines pour les chemins de caméra, matrices de locators, types de surface pour la physique (route, herbe, trottoir...).

---

## P3D-009 — Ajouter les chunks Collision manquants
**Priorité :** P1
**Chunks :** `CollisionWallVolume` (0x7010005), `CollisionEffect` (0x3000600), `Atc` (0x3000602)

---

## P3D-010 — Ajouter les chunks Static Prop
**Priorité :** P2
**Chunks :** `StaticPropData` (0x8020000), `StaticPropStateData` (0x8020001), `StaticPropVisibilityData` (0x8020002), `StaticPropFrameControllerData` (0x8020003), `StaticPropEventData` (0x8020004), `StaticPropCallbackData` (0x8020005)

**Description :** Props statiques avec états (destructibles, interactifs). Utilisés pour les distributeurs, poubelles, boîtes aux lettres...

---

## P3D-011 — Ajouter les chunks Geometry manquants
**Priorité :** P2
**Chunks :** `Mesh` (0x10000), `OffsetList` (0x1000E), `PackedNormalList` (0x10010), `VertexShader` (0x10011), `DrawShadow` (0x10017)

**Description :** Variantes de géométrie (Mesh vs PolySkin, normales compressées, vertex shader hardware PS2...).

---

## P3D-012 — Ajouter les chunks Export Info
**Priorité :** P3
**Chunks :** `ExportInfo` (0x7030), `ExportInfoNamedString` (0x7031), `ExportInfoNamedInteger` (0x7032)

**Description :** Métadonnées d'export 3ds Max → P3D. Non critique.

---

## P3D-013 — Implémenter P3DUtil::GetShaderTexture()
**Priorité :** P0 (bloquant)
**Fichier :** `src/P3D/P3DChunk.h:267` (déclaré), jamais défini
**Description :** Fonction utilitaire déclarée mais jamais implémentée. Sert à résoudre les textures des shaders. Si elle est utilisée, lien cassé.
**Correctif :** Implémenter ou supprimer la déclaration. Vérifier si `CompositeModel` ou `WorldSphere` en dépend.

---

## P3D-014 — Implémenter le parsing de CompositeDrawableSkin et CompositeDrawableEffect
**Priorité :** P2
**Fichier :** `src/P3D/P3D.generated.cpp` (constructeurs CompositeDrawableSkinList et CompositeDrawableEffectList)
**Description :** Les constructeurs lisent `numSkins`/`numEffects` mais ne parsent pas les chunks enfants. Compléter la boucle de parsing.

---

## P3D-015 — Documenter les champs `_todo` dans le code P3D
**Priorité :** P3 (recherche)
**Champs :** `StaticPhysics::_todo`, `InstancedStaticPhysics::_todo`, `DynamicPhysics::_todo`, `AnimDynamicPhysics::_todo`, `AnimObjectWrapper::_todo`, `SceneGraph::_todo`, `RoadDataSegment::_todo0/1`, `Road::_todo0/1/2/3`, `CollisionObjectAttribute::_todo1/2/3`
**Description :** Champs lus mais but inconnu. À reverser via Ghidra (chercher comment l'exe utilise ces valeurs).

---

## P3D-016 — Reverser le format P3DZ (compression propriétaire)
**Priorité :** P2
**Description :** Format compressé utilisé pour beaucoup de fichiers du jeu. Le header est `0x5A443350`. 
**Approche :** 
1. Extraire `SIMPSONS.EXE` (le vrai binaire, pas le launcher)
2. Chercher les références à `0x5A443350` dans Ghidra
3. Identifier l'algorithme de décompression
4. Implémenter dans `P3DFile` ou un `P3DZFile` séparé

---

## P3D-017 — Support des images non-PNG (format propriétaire ?)
**Priorité :** P3
**Fichier :** `src/P3D/P3DChunk.cpp` (ImageDecoder::Decode)
**Description :** `stb_image` ne décode que le PNG. Les images P3D peuvent avoir d'autres formats (palettisé, DXT...). Vérifier le champ `format` de Image.
