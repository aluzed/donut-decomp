# UI-TEXT — Rendu des FrontendMultiText (texte frontend)

- **Status:** DONE (2026-10-20)
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

## Related, done (2026-10-18): the phrase table for mission objectives

The CD carries the text bible as a tab-separated export beside the compiled one,
`art/frontend/scrooby2/resource/txtbible/srr2.txt`, five languages per row (E F G I S, as its
own "Languages" line says). `UI/TextBible` loads one language of it (1634 phrases in French).
`SetStageMessageIndex(n)` now picks the stage's `MISSION_OBJECTIVE_n`, and the HUD shows it in
place of the objective's type: m1 opens on "COLLECT LISA'S SCIENCE PROJECT", or with
`--language F` "TROUVEZ LE PROJET SCIENTIFIQUE DE LISA". The frontend's FrontendMultiText,
which is this ticket, is still to do; the phrase table is what it will read from.

The failure screen says, in the game's words, what went wrong and what to do about it: each
failure is filed with a hint term (`MISSION_FAILED_VEHICLE_DAMAGE_HINT1`), and the term
before `_HINT` is the reason itself ("You destroyed your vehicle."). Checked on sr2 with the
car's health forced to 0: "MISSION FAILED!", the reason, the E-brake hint, then the retry.

## Critères d'acceptation
- [ ] `FrontendMultiText` rendu avec position et alignement corrects (cohérents avec les sprites).
- [ ] Le texte utilise la `TextureFont` chargée depuis le P3D frontend.
- [ ] Le contenu provient du text bible (pas de chaîne hardcodée).
- [ ] Les groupes imbriqués propagent leurs textes (`AddGroup`).

## Done (2026-10-20)

- **Texts drawn** (`FrontendProject::AddMultiText`): a `FrontendMultiText` (0x18007) of a
  page, its group's or its layer's, shows its current string -- the text bible's phrase
  for its key (`FrontendStringTextBible`, 0x1800B) -- in its font, aligned in its box
  (the position is the box's bottom left, y up, as the sprites': the splash screen's
  "PRESS START" stands at y 60 of 480), in its colour. Too wide for its box, it is shrunk
  to fit (`SpriteBatch::DrawText` takes a scale), as "RESUME GAME" is between the main
  menu's arrows.
- **Fonts**: a page names its text style ("font0_24", "fontB_16"), the font file its
  font ("boulder_24"); the frontend's `resource/fonts/*.p3d` are loaded under both names,
  and each text style a page declares (`FrontendTextStyleResource`, 0x18104) under its
  own.
- **Images**: a page's image resources (`FrontendImageResource`, 0x18100: name and
  `images\LArrowG.png`) are read from the frontend's `resource/` directory, by name.
- `SetText` shows a text of ours in place of a page's strings; `GetRect` gives where an
  element stands, for clicks.

## Not done

- `FrontendStringHardCoded` (0x1800C) strings; a text's shadow; text effects.
