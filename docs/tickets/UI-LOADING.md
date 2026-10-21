# UI-LOADING — Loading screen (progress and the level's newspaper)

- **Status:** DONE (2026-10-21)
- **Priority:** P3
- **Module:** UI
- **Depends on:** UI-TEXT
- **Files:** `src/UI/LoadingScreen.{h,cpp}`, `src/GameLevel.cpp` (`loadLevel`, `switchLevel`),
  `src/FrontendProject.{h,cpp}` (`MoveSprite`, polygons' boxes)

## Context
Loading a level held the game up behind one still frame of the game's loading page
(backend.p3d's `Loading.pag`): the level's picture and "LOADING", the fuse whole. The
ticket asked for a progress bar following the load and a hint text.

## What the game has
`Loading.pag` is the level's newspaper (dynaload/images/loading<n>.p3d), "LOADING" (in the
game's language) and a **fuse**: a wick (`Wick`, a polygon) from a flame (`Fire`) to a
stick of dynamite (`Dynamite`), Itchy tied to it. That fuse is the game's progress bar.
There are **no hint texts**: the text bible's loading entries are the newspapers' date,
headlines and caption (`LOADING_NEWSPAPER_<level><A-D>`), and they are printed in the
pictures already (level 3's "October 27 — Bart Simpson Disappears in Bright Light").

## Done
- `LoadingScreen` (`src/UI/LoadingScreen.*`) holds the page: `Show(level)` puts the level's
  picture in and the fuse whole, `SetProgress(fraction)` burns the wick down from its left
  end and moves the flame along it, then draws a frame (and pumps the window's events, so
  it stays answerable through the load).
- `loadLevel` reports its steps: each of the world's files (most of the time, up to 70 %),
  the roads, the traffic and pedestrians, the gags, the end. The game's first level and
  every switch of level show it.
- A page's polygons now have their box, as its sprites do (`GetSpriteBox` gave the wick
  nothing); `FrontendProject::MoveSprite` moves an element without resizing it.
- No `GameState::Loading`: the load runs through, nothing else happens meanwhile, and the
  game goes on to its state as before.

## Acceptance criteria
- [x] The loading screen is shown while a level loads.
- [x] The progress follows the load (0 to 100 %): the fuse burns down step by step.
- [~] A varied hint text: the game has none; its newspapers, one a level, are shown.
- [x] Back in the game once loaded.
