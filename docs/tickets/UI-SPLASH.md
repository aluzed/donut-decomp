# UI-SPLASH — Aucun texte 2D ni ligne de debug dessinés, menu invisible donc inatteignable

- **Status:** DONE (2026-08-22)
- **Priority:** P0 (bloquant : impossible d'atteindre `InGame` par les commandes normales)
- **Module:** Game / UI
- **Depends on:** —
- **Blocks:** UI-MENU, UI-HUD, UI-TEXT, GAME-003, et toute validation manuelle du gameplay
- **Files:** `src/Render/SpriteBatch.cpp`, `src/Render/LineRenderer.cpp` (shaders inline)

## Contexte

Découvert en validant `REGR-001` (2026-08-22).

Au lancement, `_gameState = GameState::Splash`. La seule sortie est
`if (intent.uiConfirm || intent.pauseToggle) _gameState = MainMenu`, puis un clic sur le
bouton « New Game » du `MainMenu` fait passer à `InGame`.

En pratique, **rien de tout ça ne se produit** : envoyer Entrée (mappé sur `ui_confirm`)
puis cliquer à l'emplacement du bouton laisse la caméra strictement identique
(`(75.638, 3.378, -646.125)` avant et après). Et **aucun texte dessiné par `SpriteBatch`
n'apparaît jamais** — ni le titre « donut », ni les libellés de boutons, ni le HUD. Le seul
texte visible à l'écran vient d'ImGui (barre de menu de debug, overlay caméra).

## Ce qui est déjà écarté

La police se charge et s'enregistre correctement — instrumenté au point d'appel :

```
DBG font registered as 'boulder_16' | root children=1 | GetFont(boulder_16)=0x2127a8630b0
```

Donc `_resourceManager->GetFont("boulder_16")` ne renvoie **pas** `nullptr`, et le garde
`if (font == nullptr) return;` de `SpriteBatch::DrawText` n'est pas en cause.

Forcer `_gameState = GameState::InGame` à l'init contourne entièrement le problème : le jeu
se déroule alors normalement (physique, piétons, collectibles, triggers, véhicule). Le
blocage est donc dans la transition d'état ou dans le rendu 2D, pas dans le gameplay.

## Cause racine (résolue)

**Les deux shaders *inline* ne fixaient pas leurs emplacements d'attributs.**

`SpriteBatch` et `LineRenderer` déclarent leur GLSL dans le `.cpp`, en `#version 150 core`,
avec de simples `in vec2 vert_position; …`. Sans `layout(location = N)`, l'éditeur de liens
GL attribue les emplacements dans un ordre **défini par l'implémentation**. Mesuré sur ce
poste (Radeon RX 7900 XTX) :

```
prog=8 vert_position=2 vert_texcoord=0 vert_color=1
```

…alors que le tableau `ArrayElement` du constructeur code en dur `0 = position`,
`1 = texcoord`, `2 = couleur`. Le vertex shader lisait donc la position dans les **flottants
de couleur** : chaque quad s'effondrait en un point sous-pixel hors écran. Aucune erreur GL,
aucun avertissement — juste rien à l'écran.

Tous les shaders de `assets/shaders/` sont en `#version 330` avec des `layout(location = N)`
explicites : c'est pourquoi la scène 3D s'affichait correctement et que seuls ces deux
renderers étaient touchés. Ça a dû fonctionner sur l'ancienne machine Linux, dont le
compilateur GLSL assignait les emplacements dans l'ordre de déclaration.

Correctif : passer les deux shaders inline en `#version 330 core` avec des
`layout(location = N)` explicites alignés sur le layout C++ (et renommer l'uniform
`texture` en `spriteTexture`, `texture2D()` ayant disparu du profil core).

## Vérification

- Menu principal : titre « donut » et libellés « New Game » / « Quit » visibles.
- Clic sur « New Game » → passage en `InGame` (HUD « Coins: 1/10 », barre d'aide).
- HUD en jeu visible (fps, HP, pièces).
- Debug draw Bullet et `LineRenderer` : squelette, axes et volumes désormais tracés.

L'état **n'était pas bloqué** sur Splash : la transition fonctionnait, le menu était
simplement invisible.

## Pistes écartées (2026-08-22)

- **Police non chargée** : instrumenté, `GetFont("boulder_16")` renvoie un pointeur valide
  (cf. ci-dessus).
- **`GL_DEPTH_TEST` laissé actif pour la passe 2D** : `blitSceneToBackbuffer()` réactive le
  test de profondeur juste avant que le HUD / les menus ne soient dessinés sur le backbuffer,
  dont le depth buffer n'est jamais nettoyé (la scène va dans `_sceneFBO`). Hypothèse
  plausible mais **testée sans effet** : le désactiver ne fait apparaître ni texte ni lignes.
  (Le laisser actif reste douteux et pourrait être corrigé au passage.)
- Le debug draw de Bullet **s'exécute** (activer `DrawWireframe` fait chuter le FPS de 650 à
  176) mais **aucune ligne n'apparaît**. Le `LineRenderer` est pourtant vidé *à l'intérieur*
  du FBO, avant le blit. Donc le problème touche `LineRenderer` **et** `SpriteBatch` — c'est
  un souci de rendu commun, pas un bug propre au menu ni à la machine à états.

## Pistes d'investigation

- `intent.uiConfirm` vient de `Input::JustPressed(GameAction::UIConfirm)` : vérifier que
  `JustPressed` est bien réarmé chaque frame et que la transition n'est pas court-circuitée
  par un `continue` plus haut dans `Game::Run`.
- Vérifier si l'état atteint `MainMenu` mais que rien ne se dessine (le monde 3D reste
  visible dans les deux cas, donc l'écran ne change pas d'aspect) — logguer la transition.
- Rendu : `SpriteBatch` local (`menuSprites`) puis `Flush(proj)` avec
  `MakeOrtho(0, w, h, 0)`. Vérifier l'état GL au moment du flush (depth test encore actif
  après le pipeline post-process de `FX-000` ? blending ? texture de glyphe liée ?).
- Le pipeline offscreen introduit par `7e41609` dessine la scène dans `_sceneFBO` : le texte
  2D est-il émis **avant** le blit du quad plein écran, donc écrasé ?

## Note

Le hit-test du menu était **aussi** cassé (les boutons étaient testés contre le rectangle
`(0,0,200,40)` passé à `AddButton`, alors qu'ils sont dessinés au centre de l'écran). C'est
corrigé — `GameMenu::SetButtonRect` + `Font::MeasureWidth` — mais ça ne suffit pas à rendre
le menu utilisable tant que ce ticket est ouvert.

## Critères d'acceptation

- [x] Appuyer sur Entrée depuis Splash fait passer en MainMenu de façon observable.
- [x] Le titre et les libellés de boutons s'affichent réellement à l'écran.
- [x] Cliquer « New Game » démarre la partie (`InGame`).
- [x] Le HUD (texte `SpriteBatch`) est visible en jeu.
