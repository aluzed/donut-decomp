# SCRIPT-H — The scripts' screen and HUD commands

- **Status:** DONE (2026-10-20)
- **Priority:** P2
- **Module:** Scripting
- **Files:** `src/Scripting/GameCommands.cpp`, `src/Scripting/ScriptEngine.{h,cpp}`
  (`SetStageTransition`, `GetTransition`, `ShowHUD`), `src/Game.cpp` (`drawTransition`)

## What the scripts use
Of the commands this ticket listed, the levels' scripts use `SetFadeOut` (16 stages:
0.1 s before a car is swapped back, 1 or 2 s elsewhere), `SetIrisWipe` (6 stages, all
0.1 s) and `ShowHUD("false")` (level 7's last mission, a movie). `StayInBlack` and
`SetIrisTransition` appear in no level script; `SetPresentationBitmap` was shown before;
`SetPostLevelFMV` is commented out in every level (UI-FMV).

## Transitions
A stage's `SetFadeOut(t)` or `SetIrisWipe(t)`, run as the stage begins, is kept for its
end: when its objective is met, the screen closes over `t` seconds (to black, or a
circle closing on its middle) while the script waits, the next stage (or the mission's
end) begins once it is shut, and it opens again in half a second. While it closes the
stage is over: its objective, conditions, clock and cars decide nothing (level 4 m1's
police car, followed to the end of its route under a 0.1 s iris, else won a "race"
against the player). A mission's start clears one left over.

`Game::drawTransition` draws it over the game and its HUD, in game and paused: a black
quad at the closed fraction, or a 256 x 256 disc clear in black (its edge smoothed over
two pixels) as wide as the screen's diagonal when open, black about it.

Checked by screenshots of each, closing and opening (a stage ended by hand with a 2 s
transition), and by the fraction logged every tenth of a second: 0.16, 0.32 ... 0.93,
shut at 5.0 s, open again by 5.5 s; the fade's measured brightness follows it (28 at
0.75 for 114 open).

## HUD
`ShowHUD("false")` hides the HUD for the rest of the mission; the next mission shows it.

## Not done
The cars swapped behind the fade (`SwapInDefaultCar`, `SetSwapDefaultCarLocator`,
`SetSwapForcedCarLocator`) are still stubs.
