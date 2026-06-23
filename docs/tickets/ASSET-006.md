# ASSET-006 — Charger les vraies textures et shaders depuis les P3D des CAB

- **Status:** TODO
- **Priority:** P1
- **Module:** Tools
- **Depends on:** ASSET-004
- **Files:** `src/FrontendProject.cpp:138-151` (`Sprite`/`TextureFont`), `src/ResourceManager.h`, `src/Render/Texture.h`

## Contexte
Les textures du jeu sont stockées dans les chunks des P3D (Sprite, Texture/Image, Shader). `FrontendProject::LoadP3D` sait déjà charger les chunks `Sprite` et `TextureFont` (`src/FrontendProject.cpp:138-151`), mais le reste du pipeline utilise encore des `.png` externes (`src/FrontendProject.cpp:45`).

## Approche
- Étendre le `ResourceManager` pour charger les chunks Texture/Image P3D (décoder le format d'image interne) et les exposer comme `Texture`, en s'appuyant sur `LoadTexture(*sprite)` existant (`src/FrontendProject.cpp:141`).
- Faire pointer `AddMultiSprite` vers les textures issues du P3D plutôt que `GetTexture(name + ".png")` (`src/FrontendProject.cpp:45`).
- Charger les chunks Shader des P3D et mapper leurs paramètres sur les shaders donut (`src/Game.h:111` `_meshShader`, `:124` `_skinShaderProgram`).
- Valider sur un niveau et le frontend : textures correctes, pas de magenta/placeholder.

## Critères d'acceptation
- [ ] Textures chargées directement depuis les chunks P3D (plus de `.png` externes pour le frontend).
- [ ] Définitions de shader lues depuis les P3D et appliquées.
- [ ] Un niveau et le frontend s'affichent avec leurs textures réelles.
