# CORE-015 — Supprimer les includes <iostream>/<fmt> superflus → Core/Log.h

- **Status:** DONE
- **Priority:** P3
- **Module:** Core
- **Depends on:** CORE-014
- **Files:** `src/Core/Log.h`, `src/ResourceManager.cpp`, `src/Game.cpp`, `src/Character.cpp`, `src/Entity.cpp`, `src/Scripting/GameCommands.cpp`, `src/main.cpp`

## Contexte
De nombreux fichiers incluent directement `<iostream>` ou `<fmt/...>` alors qu'ils devraient passer par `Core/Log.h` (qui encapsule déjà `<fmt/format.h>` et `<fmt/printf.h>`). Cela disperse les dépendances de log et alourdit les unités de compilation. Fichiers concernés : `ResourceManager.cpp`, `Game.cpp`, `Character.cpp`, `Entity.cpp`, `Scripting/GameCommands.cpp`, `main.cpp`.

## Approche
1. Après CORE-014 (remplacement des `cout`/`printf` par `Log::*`), retirer les `#include <iostream>` et `#include <fmt/...>` devenus inutiles dans les fichiers applicatifs.
2. Là où le formatage `fmt`/log reste nécessaire, inclure `Core/Log.h` à la place.
3. Ne pas modifier les includes du code tiers (`ThirdParty/`, `Render/imgui/`) ni `P3D.generated.cpp`/`P3DChunk.h` s'ils en dépendent légitimement.
4. Rebuild complet pour vérifier qu'aucun symbole `fmt`/`std::cout` n'était utilisé via include transitif supprimé.

## Critères d'acceptation
- [x] Plus de `#include <iostream>` superflus dans les fichiers applicatifs listés.
- [x] Plus de `#include <fmt/...>` directs hors `Core/Log.h` côté applicatif.
- [x] Les fichiers ayant besoin de log incluent `Core/Log.h`.
- [x] Le projet compile sans erreur après nettoyage.
