# ASSET-002 — Extraire data1.cab + data2.cab de l'ISO CD1 via 7z

- **Status:** TODO
- **Priority:** P0
- **Module:** Tools
- **Depends on:** —
- **Files:** `tools/extract_cab.sh:18-30`, `tools/setup.sh:32-45`, `files/jeu-03626-the_simpsons_hit_and_run_cd1-pcwin.7z`

## Contexte
Le CD1 est livré comme `.7z` dans `files/` ; il contient une ISO d'où il faut sortir `data1.cab`, `data1.hdr` et `data2.cab` (le `.hdr` est requis pour qu'unshield lise le multi-volume). `tools/extract_cab.sh:22-24` et `tools/setup.sh:40-45` font déjà l'extraction via `7z e -so`.

## Approche
- Décompresser le 7z CD1 : `7z x files/jeu-03626-...cd1-pcwin.7z -o/tmp/donut_iso` (cf. `tools/setup.sh:33-34`).
- Localiser l'ISO produite puis extraire les volumes : `7z e -so <iso> data1.cab > assets/.../data1.cab`, idem `data1.hdr` et `data2.cab` (`tools/extract_cab.sh:22-24`, `tools/setup.sh:40-42`).
- Placer les CAB/HDR ensemble dans un dossier de travail (`assets/` ou `/tmp/donut_cabs`).
- Vérifier la présence et la taille non nulle de `data1.cab`, `data1.hdr`, `data2.cab`.

## Critères d'acceptation
- [ ] L'ISO CD1 est extraite depuis le `.7z` de `files/`.
- [ ] `data1.cab`, `data1.hdr`, `data2.cab` extraits dans le dossier de travail.
- [ ] Les fichiers ont une taille non nulle et sont co-localisés pour unshield.
