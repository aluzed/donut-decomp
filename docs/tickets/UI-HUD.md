# UI-HUD — HUD complet (vie, jauge Hit&Run, vitesse, coins, cartes, timer, objectifs, radar, nitro)

- **Status:** TODO
- **Priority:** P1
- **Module:** UI
- **Depends on:** UI-TEXT
- **Files:** `src/Game.cpp:925-1090` (DrawText HUD actuel), `src/Game.h:130-141` (`_health`, états), `src/Render/SpriteBatch.h:33-38` (Draw/DrawText/Draw9Slice)

## Contexte
Le HUD actuel est minimal : du texte brut via `SpriteBatch::DrawText` (FPS `src/Game.cpp:933`, coins `:940`, HP `:956`, vitesse `:1025`, objectif/timer `:977-986`, heat `:1086`). Il manque les éléments graphiques du jeu original : jauge Hit&Run, compteur de vitesse analogique, coins/cartes iconifiés, minimap/radar, nitro.

## Approche
- Créer `src/UI/Hud.h/.cpp` regroupant le rendu HUD (extrait du bloc `src/Game.cpp:925-1090`) avec une API `Hud::Draw(SpriteBatch&, const GameState&, ...)`.
- Vie : barre via `SpriteBatch::Draw9Slice` (`src/Render/SpriteBatch.h:38`) pilotée par `_health` (`src/Game.h:135`), couleur déjà calculée `src/Game.cpp:956`.
- Jauge Hit&Run : barre de "heat" (cf. `heatText`/`heatCol` `src/Game.cpp:1086`) en gauge graphique.
- Vitesse : compteur (texte chiffré `src/Game.cpp:1025` + arc/aiguille via sprites) actif uniquement `_inVehicle` (`src/Game.h`).
- Coins / cartes : icône + compteur (réutiliser le texte coins `src/Game.cpp:940`).
- Timer mission / objectifs : reprendre `objText`/`timerText` `src/Game.cpp:977-986`, cadrer en haut.
- Radar / minimap : panneau coin écran ; v1 = points relatifs au joueur (position joueur dispo via `Character`/`_activeVehicle`).
- Nitro : barre dédiée (placeholder valeur si non encore simulé).
- Charger les textures HUD via le P3D frontend (UI-TEXT/ResourceManager) plutôt que primitives nues.

## Critères d'acceptation
- [ ] Vie, jauge Hit&Run et nitro affichées en barres graphiques.
- [ ] Compteur de vitesse affiché uniquement en véhicule.
- [ ] Coins et cartes affichés avec icône + valeur.
- [ ] Timer/objectifs de mission affichés pendant une mission active.
- [ ] Radar/minimap affiché avec au moins la position du joueur.
- [ ] Rendu HUD extrait de `Game.cpp` vers `src/UI/Hud.*`.
