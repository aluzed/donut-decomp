# SCRIPT-H — Commandes UI / HUD

- **Status:** TODO
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** UI-HUD, UI-MENU
- **Files:** `src/Scripting/GameCommands.cpp:267,268,298,324,375,294`, `src/Scripting/ScriptEngine.cpp:398-401` (SetPresentationBitmap)

## Contexte
`SetPresentationBitmap` est câblé (log seulement, `ScriptEngine.cpp:398`). Les commandes de transition d'écran et d'affichage HUD (fondu, iris, affichage/masquage du HUD) sont des stubs vides, donc les transitions scénarisées sont invisibles.

## Approche
- S'appuyer sur le HUD et le système de menus livrés par UI-HUD / UI-MENU pour exposer des hooks de fondu, d'iris et d'affichage HUD.
- Câbler les stubs (`GameCommands.cpp`) vers ces hooks via `Game`/`ScriptEngine` :
  - transitions : `SetFadeOut` (`:267`), `StayInBlack` (`:268`), `SetIrisWipe` (`:298`), `SetIrisTransition` (`:375`),
  - HUD : `ShowHUD` (`:324`), `SetPresentationBitmap` (déjà câblé, à brancher sur l'écran de présentation),
  - FMV de transition : `SetPostLevelFMV` (`:294`).
- Piloter les fondus/iris dans la boucle de rendu en fonction des durées passées par les commandes.

## Critères d'acceptation
- [ ] Un `.con` représentatif contenant des transitions/HUD s'exécute avec 0 avertissement « unimplemented command ».
- [ ] `SetFadeOut`/`SetIrisWipe` produisent une transition visible à l'écran.
- [ ] `ShowHUD` affiche/masque réellement le HUD de jeu.
