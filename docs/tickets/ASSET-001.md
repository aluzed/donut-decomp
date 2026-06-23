# ASSET-001 — Construire unshield (extraction des CAB InstallShield)

- **Status:** TODO
- **Priority:** P0
- **Module:** Tools
- **Depends on:** —
- **Files:** `tools/extract_cab.sh:7-16`, `tools/setup.sh:20-27`

## Contexte
Les assets sont dans des CAB InstallShield qui nécessitent `unshield` (github.com/twogood/unshield), construit depuis les sources. `tools/extract_cab.sh:10-16` et `tools/setup.sh:21-27` tentent déjà ce build mais vers des chemins divergents (`/tmp/opencode/...` vs `/tmp/unshield_build/...` vs `/tmp/donut_unshield`).

## Approche
- Cloner et compiler : `git clone --depth 1 https://github.com/twogood/unshield.git`, `cmake -S unshield -B unshield/build`, `cmake --build unshield/build -j$(nproc)` (cf. `tools/setup.sh:23-26`).
- Dépendances : `zlib`/`libssl` requises par unshield ; documenter l'install (apt) dans le script.
- Unifier le chemin du binaire entre `tools/extract_cab.sh:7` et `tools/setup.sh` (une seule variable, ex. `tools/unshield`).
- Vérifier : `unshield -d out x data1.cab` liste/extrait sans erreur sur un `data1.cab` réel.

## Critères d'acceptation
- [ ] `unshield` compile depuis les sources sur Linux.
- [ ] `unshield -d out x data1.cab` s'exécute sans erreur et produit des fichiers.
- [ ] Chemin du binaire cohérent entre `extract_cab.sh` et `setup.sh`.
