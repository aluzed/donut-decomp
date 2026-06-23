# UI-LANG — Texte localisé via les language bibles

- **Status:** TODO
- **Priority:** P2
- **Module:** UI
- **Depends on:** P3D-005, UI-TEXT
- **Files:** `src/P3D/P3DChunk.cpp:159-169` (`FrontendTextBible`, `FrontendStringTextBible`, `FrontendTextBibleResource`), `src/FrontendProject.cpp:105-154`, `src/ResourceManager.h`

## Contexte
Le jeu stocke les chaînes dans des "text bibles" par langue (`FrontendTextBible` / `FrontendStringTextBible` / `FrontendTextBibleResource`, déclarés `src/P3D/P3DChunk.cpp:159-169`). UI-TEXT résout déjà les chaînes via une bible ; il faut sélectionner la bible selon la langue active.

## Approche
- Une fois le parsing des chunks bible disponible (P3D-005), charger les `FrontendTextBible` dans un `TextBible` (map clé→chaîne) au sein du `ResourceManager` (cf. `AddFont` `src/FrontendProject.cpp:149`).
- Ajouter un enum `Language` + un sélecteur (config/CLI) ; charger la bible correspondante (les bibles sont séparées par langue dans les P3D frontend).
- Exposer `ResourceManager::GetString(key)` consommé par `AddMultiText` (UI-TEXT) et le HUD.
- Fallback : si clé absente dans la langue courante, retomber sur l'anglais et logguer (`Log::Warning`).

## Critères d'acceptation
- [ ] Les chaînes frontend proviennent de la bible de la langue active.
- [ ] Changer de langue change le texte affiché sans recompilation.
- [ ] Clé manquante → fallback anglais + warning loggé.
