# TOOL-010 — CI GitHub Actions (build Linux+Windows, tests, clang-format)

- **Status:** TODO
- **Priority:** P3
- **Module:** Tools
- **Depends on:** TOOL-007
- **Files:** `tools/setup.sh:67-69` (build cmake), `CMakeLists.txt`, `.clang-format`

## Contexte
Aucune intégration continue n'existe. Une CI garantit que le projet compile sur Linux et Windows, que les tests passent (TOOL-007) et que le style est respecté.

## Approche
- Créer `.github/workflows/ci.yml` avec une matrice `ubuntu-latest` + `windows-latest`.
- Étapes build : `cmake -S . -B build` puis `cmake --build build -j` (cf. `tools/setup.sh:68-69`) ; installer les deps système nécessaires.
- Étape tests : exécuter la suite via `ctest` (issue de TOOL-007) — sans dépendre des assets propriétaires (P3D de test embarqués/minimaux).
- Étape style : vérifier le formatage via `clang-format --dry-run --Werror` sur `src/` (selon `.clang-format`).
- Échec du job si build, tests ou format échouent.

## Critères d'acceptation
- [ ] Workflow CI déclenché sur push/PR.
- [ ] Build vert sur Linux et Windows.
- [ ] `ctest` exécuté et bloquant en cas d'échec.
- [ ] Vérification clang-format bloquante sur écart de style.
