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

## Constat (2026-08-22, en traitant P3D-008)

`dev/codegen/donut-codegen.exe` **fonctionne** tel quel :

```
donut-codegen.exe --p3din dev/codegen/p3d.json --p3dout <dir>                   --cmdin dev/codegen/cmd.json --cmdout <dir> --copyright "..."
```

Il produit **135 classes** de chunks contre **107** commitées — 28 de plus, **aucune
supprimée**, et les 107 communes ont exactement les mêmes accesseurs. Les 28 nouvelles
couvrent le *parsing* de plusieurs tickets P3D ouverts : `Light`/`LightDirection`/`LightGroup`/
`LightPosition`/`LightShadow` (P3D-002), `FrontendTextBible`/`FrontendStringHardCoded`
(P3D-005), `InstancedStaticEntity` (P3D-006), `Tree`/`TreeNode` (P3D-007),
`CollisionWallVolume` (P3D-009), `StaticPropData` (P3D-010),
`Mesh`/`OffsetList`/`PackedNormalList`/`DrawShadow` (P3D-011), `ExportInfo*` (P3D-012).

**Mais la régénération globale casse la compilation**, pour deux raisons :

1. **Le codegen émet des types glm** (`glm::vec3`, `glm::mat4`) alors que le fichier commité
   utilise `Donut::Vector3` / `Matrix4x4`. Le fichier « généré » du dépôt a donc été **adapté
   à la main** après génération. Sans étape de remappage, tout le code appelant casse.
2. Au moins une définition produit du code invalide (`Locator` : `_dataSize` référencé hors
   portée dans le `Load()` généré).

Et le contenu de `p3d.json` n'est pas fiable non plus : sur les 3 chunks examinés pour
`P3D-008`, **2 layouts sur 3 étaient faux** (cf. ce ticket). Régénérer en masse sans vérifier
chaque layout contre les assets réels produirait du parsing silencieusement erroné — dans le
cas de `Spline`, un `resize()` de 23 Go.

Ce que ce ticket doit donc livrer, dans l'ordre :

1. Une étape de remappage de types (glm → types du projet) après génération, dans `run.bat`.
2. La correction de la définition `Locator`.
3. Une vérification de layout par chunk contre les assets réels avant d'activer une classe —
   le scanner récursif écrit pour `P3D-008` est réutilisable.

Note : `Commands.generated.cpp` est en revanche **identique en substance** (mêmes 245
commandes) ; seule l'indentation diffère. Rien à régénérer de ce côté.

## Critères d'acceptation
- [ ] Le codegen s'exécute sur la plateforme dev (Windows ou via mono/dotnet sous Linux).
- [ ] Régénération sans diff sur les fichiers existants (reproductible).
- [ ] `p3d.json` enrichi des chunks manquants ; sortie régénérée compile.
