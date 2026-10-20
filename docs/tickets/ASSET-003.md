# ASSET-003 — Extraction test d'un P3D et ouverture dans P3DFile

- **Status:** DONE (2026-10-20)
- **Priority:** P0
- **Module:** Tools
- **Depends on:** ASSET-001, ASSET-002
- **Files:** `tools/extract_cab.sh:30`, `src/P3D/P3DFile.h:17`, `src/FrontendProject.cpp:115` (`P3DFile(filename)`)

## Contexte
Avant l'extraction complète, valider la chaîne bout-en-bout : sortir un seul P3D du CAB et l'ouvrir dans le loader de donut (`P3D::P3DFile`, utilisé `src/FrontendProject.cpp:115`). Cela confirme qu'unshield produit des P3D valides et que le parser les lit (y compris compression P3DZ `0x5A443350`, `src/P3D/P3DFile.h:17`).

## Approche
- Extraire un fichier ciblé : `unshield -d out x data1.cab` puis récupérer `Art/b00.p3d` (cf. `tools/extract_cab.sh:30`, `Art/*.p3d` listé `:35`).
- Écrire un petit harness (ou réutiliser un binaire de test) appelant `P3D::P3DFile("Art/b00.p3d")` et itérant `GetRoot().GetChildren()` comme `src/FrontendProject.cpp:116-117`.
- Logger les `ChunkType` rencontrés ; vérifier la gestion P3DZ si le header vaut `0x5A443350` (`src/P3D/P3DFile.h:17`).

## Critères d'acceptation
- [ ] `Art/b00.p3d` extrait du CAB via unshield.
- [ ] `P3DFile` ouvre le fichier sans crash et énumère ses chunks.
- [ ] Un P3DZ compressé (si rencontré) est décompressé correctement.

## Done

`tools/extract_assets.sh <dir of the CD images>` (docs/ASSETS.md): unshield, fetched
without root when missing; the cabinets off the three ISOs (or their .7z) side by side;
the whole set extracted, then `art/`, `art/audio/`, DIALOGF.RCF, `scripts/cars`,
`scripts/level0N`, `movies/` placed. Run from scratch into an empty directory, the
engine plays level 1's story from it. The models, textures and shaders have long been
read from the P3D files themselves.
