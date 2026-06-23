# TOOL-004 — Faire fonctionner donut-codegen + enrichir p3d.json + régénérer

- **Status:** TODO
- **Priority:** P1
- **Module:** Tools
- **Depends on:** —
- **Files:** `dev/codegen/donut-codegen.exe`, `dev/codegen/run.bat`, `dev/codegen/p3d.json`, `dev/codegen/cmd.json`, `src/P3D/P3D.generated.cpp`, `src/P3D/P3D.generated.h`

## Contexte
Les classes de chunks P3D sont générées par `donut-codegen.exe` à partir de `p3d.json` (sortie `src/P3D/P3D.generated.*`). C'est un prérequis aux tickets P3D-* (nouveaux chunks). Le générateur (binaire .NET + dépendances `CommandLine.dll`, `Newtonsoft.Json.dll`) doit tourner de façon reproductible.

## Approche
- Exécuter le générateur via `dev/codegen/run.bat` (Windows) ou `mono`/`dotnet dev/codegen/donut-codegen.exe` (Linux) avec les args : `--p3din codegen/p3d.json --p3dout ../src/P3D --cmdin codegen/cmd.json --cmdout ../src/Scripting` (cf. `dev/codegen/run.bat`).
- Vérifier régénération à l'identique sur l'existant (diff vide sur `src/P3D/P3D.generated.*`).
- Enrichir `dev/codegen/p3d.json` avec les chunks manquants requis par les tickets P3D-* (champs/types des bibles texte, etc., cf. `src/P3D/P3DChunk.cpp:159-169`).
- Régénérer et compiler ; mettre à jour `cmd.json`/Scripting si touché.

## Critères d'acceptation
- [ ] Le codegen s'exécute sur la plateforme dev (Windows ou via mono/dotnet sous Linux).
- [ ] Régénération sans diff sur les fichiers existants (reproductible).
- [ ] `p3d.json` enrichi des chunks manquants ; sortie régénérée compile.
