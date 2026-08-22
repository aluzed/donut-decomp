# UI-HUD — HUD complet (vie, jauge Hit&Run, vitesse, coins, cartes, timer, objectifs, radar, nitro)

- **Status:** DONE (2026-08-22)
- **Priority:** P1
- **Module:** UI
- **Depends on:** — (UI-TEXT n'était pas requis : la police `boulder_16` du frontend suffit)
- **Files:** `src/UI/Hud.h/.cpp` (nouveau), `src/Game.cpp` (collecte de `HudState`), `src/Render/Texture.*` (constructeur pixels bruts)

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

## Réalisation (2026-08-22)

Le rendu est sorti de `Game::Run` vers `src/UI/Hud.{h,cpp}`. `Game` ne dessine plus le HUD :
il remplit une structure `HudState` (viewport, santé, véhicule, vitesse, chaleur, pièces,
mission, blips radar, indications de touches) et appelle `Hud::Draw`. Le HUD ne connaît donc
ni `Game`, ni `ScriptEngine`, ni `ChaseManager`. Les *écrans* (menus, résultats de mission)
restent dans `Game` : ils remplacent le monde au lieu de se superposer à lui.

`SpriteBatch` n'a pas de primitive de rectangle plein, et toutes ses surcharges `Draw`
exigent une `Texture*`. `Texture` gagne donc un constructeur à partir de pixels RGBA bruts,
et le HUD s'alloue un blanc 1×1 qu'il teinte à chaque appel — le shader sprite multiplie le
texel par la couleur du sommet, ce qui donne des quads plats. La texture est créée sans
mipmaps et avec `GL_TEXTURE_MAX_LEVEL = 0`, sinon un sampler réclamant du mip la rendrait
incomplète (le piège déjà rencontré dans `blitSceneToBackbuffer`).

Disposition, tout l'espacement vertical dérivé de `Font::GetHeight()` — `boulder_16` fait en
réalité ~40 px de haut, et les constantes en dur écrasaient les libellés sur les barres :

| Zone | Contenu |
|---|---|
| Haut gauche | fps, pièces, cartes, indications de touches / aide |
| Bas gauche | barres SANTÉ (ou VOITURE), HIT & RUN, NITRO, empilées vers le haut |
| Bas droite | compteur de vitesse (véhicule uniquement) |
| Haut droite | radar |
| Haut centre | objectif, tour/checkpoint, temps restant |

## Limites assumées

- **Nitro** : rien ne simule de réserve de boost ; la barre reflète l'appui sur la touche,
  comme le ticket l'autorisait (« placeholder valeur si non encore simulé »). Une vraie
  ressource relève d'un ticket véhicule.
- **Cartes** : `0/7` en dur — la collecte des cartes est `GAME-010`.
- **Icônes** pièces/cartes : tuiles colorées, pas les sprites du frontend. Idem pour les
  boutons de menu, `Draw9Slice` n'est pas encore utilisé.
- Le compteur de vitesse a été validé en forçant temporairement `inVehicle` avec une vitesse
  balayée : **le chemin réel n'a pas pu être exercé**, la course de `M1race.con` se termine
  instantanément en boucle (« STAGE COMPLETE! Time: 0.0s », « Next race in N… »), donc aucun
  véhicule de mission n'est atteignable. À traiter dans `SCRIPT-A` / `AI-RACE`.

## Critères d'acceptation
- [x] Vie, jauge Hit&Run et nitro affichées en barres graphiques.
- [x] Compteur de vitesse affiché uniquement en véhicule (cadran gradué + aiguille + valeur ; garde `if (state.inVehicle)`).
- [x] Coins et cartes affichés avec icône + valeur (icônes = tuiles colorées, cf. Limites).
- [x] Timer/objectifs de mission affichés pendant une mission active.
- [x] Radar/minimap affiché avec au moins la position du joueur (flèche centrale orientée, blips mission et police tournés dans le repère joueur).
- [x] Rendu HUD extrait de `Game.cpp` vers `src/UI/Hud.*`.
