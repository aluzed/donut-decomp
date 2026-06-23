# ASSET-004 — Extraction complète des CAB et arborescence art/ audio/ movies/

- **Status:** TODO
- **Priority:** P1
- **Module:** Tools
- **Depends on:** ASSET-001, ASSET-002, ASSET-003
- **Files:** `tools/extract_assets.sh:18-71`, `tools/setup.sh:32-61`

## Contexte
Après validation unitaire (ASSET-003), extraire tous les CAB des 3 CD et organiser le résultat. `tools/extract_assets.sh` crée déjà l'arborescence cible (`art/level|chars|frontend|cars`, `audio/*`, `movies`, `:18-21`) mais s'appuie sur `cabextract` (`:32-41`) au lieu d'unshield et ne couvre pas tous les volumes.

## Approche
- Pour chaque `files/*.7z` (CD1/2/3), extraire l'ISO puis les volumes `data{1..N}.cab` + `.hdr` (cf. `tools/setup.sh:39-45`).
- Extraire chaque CAB avec unshield (`-d <dest> x dataN.cab`) plutôt que `cabextract` (corriger `tools/extract_assets.sh:32-41`).
- Ranger : `Art/*.p3d` → `art/` et sous-dossiers `art/{level,chars,cars,frontend}` (cf. `tools/setup.sh:55-59`) ; `*.RCF` dialogues → `audio/dialog` (`tools/extract_assets.sh:43-52`) ; `MOVIES/*.RMV` → `movies/` (`:54-60`).
- Idempotence : ne pas re-extraire si déjà présent ; nettoyer `/tmp` en fin.
- Exposer `assets/` via symlink (`tools/setup.sh:11-14`).

## Critères d'acceptation
- [ ] Les 3 CD extraits, tous volumes CAB traités via unshield.
- [ ] Arborescence `art/ audio/ movies/` peuplée et organisée par catégorie.
- [ ] Script idempotent (relançable sans casser un état existant).
- [ ] `assets/` accessible depuis le projet (symlink ou copie).
