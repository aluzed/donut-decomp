# TOOL-007 — Tests unitaires de parsing P3D (parse → reserialize → compare binaire)

- **Status:** TODO
- **Priority:** P3
- **Module:** Tools
- **Depends on:** TOOL-004, ASSET-003
- **Files:** `src/P3D/P3DFile.h`, `src/P3D/P3DChunk.h`, `src/P3D/P3D.generated.cpp`

## Contexte
Aucun test ne couvre le parser P3D. Un test round-trip (lire un P3D, le ré-sérialiser, comparer octet à octet) verrouille la fidélité du parsing/écriture et protège des régressions lors des régénérations (TOOL-004).

## Approche
- Mettre en place un framework de test (ex. Catch2/GoogleTest) dans le CMake.
- Pour un jeu de P3D de référence (issus de ASSET-003/004) : `P3DFile::Load` → ré-écrire via la sérialisation des chunks → comparer au fichier source octet à octet.
- Démarrer sur des chunks simples puis étendre ; tolérer (et documenter) les écarts légitimes (padding/alignement) sinon les corriger.
- Inclure un P3DZ compressé pour valider la branche `0x5A443350` (`src/P3D/P3DFile.h:17`).

## Critères d'acceptation
- [ ] Framework de test branché dans le build.
- [ ] Test round-trip parse→réécrit→compare passant sur ≥1 P3D réel.
- [ ] Au moins un cas P3DZ couvert.
- [ ] Écarts binaires soit éliminés, soit documentés et justifiés.
