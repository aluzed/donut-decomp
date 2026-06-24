# GAME-001a — Extraire GameInput (lectures SDL clavier/souris/manette → struct d'intentions)

- **Status:** DONE
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.cpp:381-401` (boucle SDL_PollEvent), `src/Game.cpp:408-414` (deltas souris), `src/Game.cpp:416-527` (lectures `Input::GetActionAxis`/`JustPressed`), `src/Game.cpp:749-790` (toggles), `src/Game.cpp:306-325` (`LockMouse`)

## Contexte
`Game::Run` lit directement SDL et `Input::*` partout : `SDL_PollEvent`/`SDL_QUIT`
(`Game.cpp:393-401`), `SDL_GetPerformanceCounter` (`Game.cpp:368`), `Input::GetMouseDeltaX/Y`
(`Game.cpp:410-411`), `Input::GetActionAxis`/`JustPressed`/`IsActionDown` (`Game.cpp:420-524`,
`749-790`) et `LockMouse` (`Game.cpp:407`, qui appelle `SDL_SetRelativeMouseMode`/`SDL_WarpMouseInWindow`,
`Game.cpp:313-324`). Cette logique d'entrée est noyée dans la boucle.

## Approche
1. Créer `src/Game/GameInput.{h,cpp}` exposant une struct d'intentions par frame :
   `struct GameIntent { Vector3 move; float mouseDX, mouseDY; bool interact, jump, boost, honk,
   vehicleJump, pauseToggle, uiConfirm, uiClick, restart, resetBest, toggleDebug, toggleHelp, quit; bool requestQuit; }`.
2. `GameInput::Poll()` encapsule `SDL_PollEvent` (`Game.cpp:393-401`, y compris
   `ImGui_ImplSDL2_ProcessEvent` et `Input::HandleEvent`) et remplit le `GameIntent` depuis
   `Input::GetActionAxis`/`JustPressed`/`IsActionDown` (regrouper `Game.cpp:420-524`, `749-790`).
3. Déplacer la gestion souris : `GetMouseDeltaX/Y` (`Game.cpp:410-411`) dans le struct, et
   `LockMouse` (`Game.cpp:306-325`) dans `GameInput`.
4. `Game::Run` consomme uniquement `GameIntent` ; aucun appel `SDL_*` ni `Input::*` direct ne
   subsiste dans `Game::Run`.

## Critères d'acceptation
- [x] `Game::Run` n'appelle plus directement SDL (`SDL_PollEvent`, `SDL_*MouseMode`, etc.) ni `Input::*` ; tout passe par `GameInput`/`GameIntent`.
- [x] Le mapping des touches (clavier/souris/manette) est inchangé, rebind compris.
- [x] Le jeu se joue à l'identique (déplacement, véhicule, pause, debug toggles).
- [x] Le jeu compile et reste runnable à l'identique.
