# CORE-014 — Système de log : Warn/Error→stderr, filtrage par niveau, remplacer cout/printf épars

- **Status:** DONE
- **Priority:** P3
- **Module:** Core
- **Depends on:** —
- **Files:** `src/Core/Log.h`, `src/Game.cpp`, `src/CharacterController.cpp`, `src/Scripting/GameCommands.cpp`, `src/main.cpp`

## Contexte
`Log.h` fournit `Debug/Info/Warn/Error` ; `Warn`/`Error` écrivent déjà sur `stderr`, `Debug`/`Info` sur `stdout`. Il manque un **filtrage par niveau** (tout est imprimé inconditionnellement) et plusieurs fichiers utilisent encore `std::cout`/`printf` directement (`Game.cpp`, `CharacterController.cpp`, `Scripting/GameCommands.cpp`, `main.cpp`).

## Approche
1. Ajouter dans `Log.h` un niveau global courant (enum `Level { Debug, Info, Warn, Error }` + variable/setter `SetLevel`) et garder chaque fonction derrière un test de niveau (early-return si en dessous du seuil).
2. Confirmer le routage : `Debug`/`Info` → `stdout`, `Warn`/`Error` → `stderr` (déjà le cas), éventuellement préfixer par `[WARN]`/`[ERROR]`.
3. Remplacer les `std::cout`/`std::cerr`/`printf` applicatifs (`Game.cpp`, `CharacterController.cpp`, `Scripting/GameCommands.cpp`, `main.cpp`) par les appels `Log::*` appropriés. Ne pas toucher le code tiers (`ThirdParty/`, `Render/imgui/`, `stb_image_write.h`).
4. Permettre de régler le niveau au démarrage (`main.cpp`, ex. via flag/env).

## Critères d'acceptation
- [ ] `Log` supporte un niveau global filtrant (`SetLevel`).
- [ ] Les messages sous le seuil ne sont pas imprimés.
- [ ] `Warn`/`Error` sortent sur `stderr`, `Debug`/`Info` sur `stdout`.
- [ ] Plus de `std::cout`/`printf` applicatifs hors `ThirdParty`/`imgui`.
- [ ] Le niveau est réglable au démarrage.
