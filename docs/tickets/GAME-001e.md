# GAME-001e — Game devient un orchestrateur fin (<400 lignes) tenant les sous-systèmes

- **Status:** TODO
- **Priority:** P1
- **Module:** Game
- **Depends on:** GAME-001a, GAME-001b, GAME-001c, GAME-001d
- **Files:** `src/Game.h:88-153` (23 membres), `src/Game.cpp:365-1197` (`Game::Run`, ~830 lignes), `src/Game.cpp:96-258` (constructeur)

## Contexte
`Game.cpp` fait ~1499 lignes et tend vers un God-object (23 membres dans `Game.h:88-153`).
Après extraction de `GameInput` (GAME-001a), `GameCamera` (GAME-001b), `GameHud`
(GAME-001c) et `GameStateMachine` (GAME-001d), `Game::Run` (`Game.cpp:365-1197`) doit se
réduire à l'orchestration de ces sous-systèmes.

## Approche
1. Remplacer les membres dispersés par 4 `unique_ptr` de sous-systèmes : `_input`, `_camera2`
   (GameCamera), `_hud`, `_stateMachine`, en supprimant les membres absorbés (`_camYaw`,
   `_camPitch`, `_smoothCamPos`, `_shakeAmount`, `_gameState`, `_missionCompleteTimer`, etc.).
2. Réécrire `Game::Run` (`Game.cpp:365-1197`) comme une boucle fine : `intent = _input->Poll()`
   → update gameplay (perso/véhicule/physique/script, `Game.cpp:416-724`) → `_camera2->Update(...)`
   → rendu 3D (`Game.cpp:826-925`) → `_hud->Draw(...)` (`Game.cpp:927-1188`) → swap.
3. Conserver dans `Game` uniquement l'orchestration et les sous-systèmes « métier » non
   extraits (Level, WorldPhysics, ScriptEngine, managers, Character).
4. Viser `Game::Run` et l'en-tête nettement plus courts ; cible globale orchestrateur <400 lignes.

## Critères d'acceptation
- [ ] `Game` ne contient plus la logique d'entrée/caméra/HUD/état (déléguée aux 4 sous-systèmes).
- [ ] Le corps d'orchestration de `Game` (`Run` + constructeur câblage) fait moins de 400 lignes.
- [ ] Le nombre de membres de `Game.h` est nettement réduit (sous-systèmes au lieu des champs absorbés).
- [ ] Le jeu se joue à l'identique (gameplay, caméra, HUD, transitions).
- [ ] Le jeu compile et reste runnable à l'identique.
