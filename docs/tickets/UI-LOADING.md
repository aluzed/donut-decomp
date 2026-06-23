# UI-LOADING — Écran de chargement (barre de progression + astuce)

- **Status:** TODO
- **Priority:** P3
- **Module:** UI
- **Depends on:** UI-TEXT
- **Files:** `src/Game.cpp` (transitions de zone/niveau), `src/FrontendProject.cpp:156-166` (`Draw`), `src/Render/SpriteBatch.h:38` (`Draw9Slice`)

## Contexte
Le passage entre zones/niveaux est instantané, sans retour visuel. Le jeu original affiche un écran de chargement avec barre de progression et texte d'astuce.

## Approche
- Ajouter un `GameState::Loading` et un `class LoadingScreen` (sous `src/UI/`) avec `SetProgress(float)` et `Draw(SpriteBatch&)`.
- Barre de progression via `SpriteBatch::Draw9Slice` (`src/Render/SpriteBatch.h:38`) ; fond plein écran via une texture frontend (chargée par `FrontendProject`).
- Texte d'astuce via `SpriteBatch::DrawText` (UI-TEXT / text bible), tournant aléatoirement.
- Brancher l'avancement sur les étapes réelles du chargement de niveau (chargement P3D, physique, scripts) en émettant des incréments de progression.

## Critères d'acceptation
- [ ] Écran de chargement affiché pendant le chargement d'une zone/niveau.
- [ ] Barre de progression reflétant l'avancement réel (0→100%).
- [ ] Texte d'astuce affiché et varié.
- [ ] Retour automatique à `InGame` une fois le chargement terminé.
