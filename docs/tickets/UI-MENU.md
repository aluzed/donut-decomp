# UI-MENU — Affichage du menu principal et du menu pause

- **Status:** DONE (2026-08-22)
- **Priority:** P1
- **Module:** UI
- **Depends on:** —
- **Files:** `src/Game.h:114-115` (`_mainMenu`/`_pauseMenu`), `src/Game.cpp:232-256` (création boutons), `src/UI/GameMenu.h:18-56`, `src/FrontendProject.cpp:105-154` (`LoadP3D`), `src/FrontendProject.cpp:127` (boucle pages)

## Contexte
`GameMenu` gère déjà le hover (`Update`) et le clic (`CheckClick`). `_mainMenu` et `_pauseMenu` sont déclarés (`src/Game.h:114-115`) et peuplés (`src/Game.cpp:232-256`), mais seul `_pauseMenu` est partiellement rendu (`src/Game.cpp:1098-1112`) et `_mainMenu` n'est jamais affiché ni câblé à un état. De plus `FrontendProject::LoadP3D` itère toutes les pages mais l'appelant ne charge en pratique que la page "MessageBox.pag".

## Approche
- Ajouter un `GameState::MainMenu` et brancher `_mainMenu->Update/CheckClick` + rendu dans la boucle de `Game::Draw` (à côté du bloc `_pauseMenu` `src/Game.cpp:1098`).
- Positionner les boutons réellement (les `x/y` valent 0 dans `src/Game.cpp:233-253`) : centrer via `viewportWidth/Height` comme le fait déjà le rendu pause `src/Game.cpp:1106-1108`.
- Dessiner un fond/cadre pour chaque bouton (via `SpriteBatch::Draw9Slice`, `src/Render/SpriteBatch.h:38`) avec couleur distincte si `btn.hovered`.
- Dans `LoadP3D` (`src/FrontendProject.cpp:127`), permettre de filtrer/charger des pages nommées : ajouter un paramètre `const std::string& pageName = ""` ; si vide, charger toutes les pages (comportement actuel) ; sinon ne traiter que `page->GetName()` correspondant. Remplacer l'appel limité à "MessageBox.pag" par le chargement des pages écran (menu, HUD).
- Câbler les actions : New Game → `GameState::InGame`, Quit → fermeture, Resume → reprise, Restart/Quit pour la pause.

## Réalisation (2026-08-22)

Trois défauts corrigés, chacun vérifié à l'écran :

1. **Hit-test contre un rectangle périmé.** `AddButton` reçoit un placeholder `(0,0,200,40)`
   parce que la taille du viewport est inconnue à la construction, mais `Update()` /
   `CheckClick()` testaient toujours contre lui alors que les libellés sont dessinés au
   centre. « New Game » n'était cliquable que dans le coin haut-gauche, sous la barre ImGui.
   Le rendu transmet désormais sa mise en page via `GameMenu::SetButtonRect`, et
   `Font::MeasureWidth` donne une boîte à la largeur réelle du libellé.

2. **Mise en page écrite pour un axe Y vers le haut.** La projection est
   `MakeOrtho(0, w, h, 0)` — y croît vers le bas — donc les entrées remontaient à mesure que
   l'index augmentait et le titre s'affichait *sous* elles. Les deux menus empilent
   maintenant vers le bas depuis un `menuTop` commun, centrés via `centredTextX()`.

3. **Le menu pause était du code mort.** Un `if (Paused) { ImGui::Render(); Swap(); continue; }`
   placé bien avant le bloc de rendu de la pause coupait la frame : l'écran affichait le
   backbuffer non redessiné, jamais « PAUSED ». Ce raccourci ne gelait d'ailleurs *rien* —
   `_worldPhysics->Update` et les scripts tournaient déjà plus haut dans la frame. Retiré, et
   la simulation est réellement gelée par un `const bool simulate = _gameState != Paused`.

`FrontendProject::LoadP3D` prend un `pageName` optionnel. Vérifié sur `bootup.p3d`
(13 pages) : nom valide → la page passe, nom inexistant → les 13 sont ignorées et un
avertissement est émis plutôt qu'un silence.

Le rendu du texte lui-même était bloqué par [UI-SPLASH](UI-SPLASH.md), corrigé juste avant.

## Reste à faire (hors périmètre de ce ticket)

Les boutons sont du texte coloré, pas des cadres : `SpriteBatch::Draw9Slice` n'est pas
utilisé. À reprendre avec `UI-HUD` quand les sprites du frontend seront exploités.

## Critères d'acceptation
- [x] Le menu principal s'affiche au démarrage (état `MainMenu`) avec boutons centrés et hover visible.
- [x] Le menu pause s'affiche sur ESC et reprend correctement le jeu via "Resume".
- [x] `LoadP3D` peut charger toutes les pages ou une page nommée (le hardcode "MessageBox.pag" avait déjà disparu ; le filtre par nom est ajouté et testé dans les deux sens).
- [x] Clic souris déclenche l'`action` du bouton survolé.
