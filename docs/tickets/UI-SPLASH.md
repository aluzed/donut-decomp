# UI-SPLASH — Le jeu reste bloqué sur Splash : aucun texte 2D, menu inatteignable

- **Status:** TODO
- **Priority:** P0 (bloquant : impossible d'atteindre `InGame` par les commandes normales)
- **Module:** Game / UI
- **Depends on:** —
- **Blocks:** UI-MENU, UI-HUD, UI-TEXT, GAME-003, et toute validation manuelle du gameplay
- **Files:** `src/Game.cpp:232` (état initial), `src/Game.cpp:~726` (sortie de Splash), `src/Game.cpp:1303+` (rendu MainMenu), `src/Render/SpriteBatch.cpp`

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

- [ ] Appuyer sur Entrée depuis Splash fait passer en MainMenu de façon observable.
- [ ] Le titre et les libellés de boutons s'affichent réellement à l'écran.
- [ ] Cliquer « New Game » démarre la partie (`InGame`).
- [ ] Le HUD (texte `SpriteBatch`) est visible en jeu.
