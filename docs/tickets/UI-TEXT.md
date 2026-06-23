# UI-TEXT — Rendu des FrontendMultiText (texte frontend)

- **Status:** TODO
- **Priority:** P1
- **Module:** UI
- **Depends on:** P3D-005
- **Files:** `src/FrontendProject.cpp:43-103` (Add*/Draw), `src/FrontendProject.cpp:127-135` (boucle layers), `src/P3D/P3D.generated.h:1626-1640` (`FrontendMultiText`), `src/Render/SpriteBatch.cpp:60` (`DrawText`), `src/Render/Font.h`

## Contexte
Les chunks `FrontendMultiText` sont parsés (`src/P3D/P3D.generated.cpp:1545`) et lus dans les layers, mais jamais rendus : `FrontendProject` ne gère que `FrontendMultiSprite` (`AddMultiSprite`, `src/FrontendProject.cpp:43`). Le texte des écrans frontend (titres, labels) est donc invisible. `SpriteBatch::DrawText` existe déjà (`src/Render/SpriteBatch.cpp:60`).

## Approche
- Ajouter un `struct Text { const Font* font; std::string value; int x,y; Vector4 color; }` et un `std::vector<Text> _texts` dans `FrontendProject`.
- Implémenter `AddMultiText(const P3D::FrontendMultiText&, resX, resY)` calquée sur `AddMultiSprite` (`src/FrontendProject.cpp:43-96`) : reprendre la conversion Y (`resY - PositionY`) et la logique d'alignement X/Y via `GetAlignX/GetAlignY`, `GetDimensionX/Y`.
- Résoudre le contenu texte : récupérer la chaîne via le text bible (clé fournie par P3D-005) et la police via `ResourceManager::GetFont` (le `TextureFont` est déjà chargé `src/FrontendProject.cpp:145-150`).
- Dans la boucle `LoadP3D` (`src/FrontendProject.cpp:127-135`), appeler `AddMultiText` pour `layer->GetMultiTexts()` et `group->GetMultiTexts()` (et l'ajouter à `AddGroup`, `src/FrontendProject.cpp:98`).
- Dans `Draw` (`src/FrontendProject.cpp:156`), itérer `_texts` et appeler `_spriteBatch.DrawText(...)` avant le `Flush`.

## Critères d'acceptation
- [ ] `FrontendMultiText` rendu avec position et alignement corrects (cohérents avec les sprites).
- [ ] Le texte utilise la `TextureFont` chargée depuis le P3D frontend.
- [ ] Le contenu provient du text bible (pas de chaîne hardcodée).
- [ ] Les groupes imbriqués propagent leurs textes (`AddGroup`).
