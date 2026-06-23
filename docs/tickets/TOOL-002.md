# TOOL-002 — Reverse de SIMPSONS.EXE dans Ghidra

- **Status:** TODO
- **Priority:** P0
- **Module:** Tools
- **Depends on:** ASSET-003
- **Files:** `tools/extract_cab.sh:36` (`Executables/Simpsons.exe`), `src/P3D/P3DFile.h:17` (P3DZ `0x5A443350`), `src/P3D/P3DChunk.h:197`

## Contexte
Le binaire d'origine `Simpsons.exe` (extrait du CAB, `tools/extract_cab.sh:36`) est la source de vérité pour les formats et la logique. Un reverse Ghidra ciblé documente les fonctions critiques pour combler les zones grises (P3DZ, `.con`, RMV) du réimplém.

## Approche
- Importer `Executables/Simpsons.exe` dans Ghidra (auto-analyse PE x86).
- Localiser et documenter : la boucle principale (game loop) ; les fonctions de parsing P3D et `.con` ; l'algo de décompression P3DZ (entête `0x5A443350`, cf. `src/P3D/P3DFile.h:17`, `src/P3D/P3DChunk.h:197`) ; le décodage des cinématiques RMV.
- Repérer chaînes/RTTI/imports (binkw32, pddidx8r, eax — `tools/extract_cab.sh:37`) pour ancrer les fonctions.
- Exporter les symboles (CSV/JSON Ghidra) et consigner pseudo-code + offsets sous `docs/`.

## Critères d'acceptation
- [ ] `Simpsons.exe` importé et auto-analysé dans Ghidra.
- [ ] Game loop, parse P3D, parse `.con`, algo P3DZ et décodage RMV identifiés et documentés.
- [ ] Symboles exportés et offsets/pseudo-code archivés dans `docs/`.
