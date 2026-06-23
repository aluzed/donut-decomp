# UI-MENU — Affichage du menu principal et du menu pause

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Le menu principal s'affiche au démarrage (état `MainMenu`) avec boutons centrés et hover visible.
- [ ] Le menu pause s'affiche sur ESC et reprend correctement le jeu via "Resume".
- [ ] `LoadP3D` peut charger toutes les pages ou une page nommée (plus de hardcode "MessageBox.pag").
- [ ] Clic souris déclenche l'`action` du bouton survolé.
