# Tickets Outils / Reverse Engineering

## TOOL-001 — Extraction automatique des assets depuis les ISOs
**Priorité :** P0
**Description :** Script pour extraire les fichiers de jeu depuis les 3 CDs :
1. Extraire les .7z → ISOs
2. Monter ISOs ou utiliser 7z pour lister/extraire
3. Extraire les CAB InstallShield (DATA1.CAB, DATA2.CAB...) avec `unshield` ou `cabextract`
4. Organiser dans `assets/` selon l'arborescence attendue (GAME-002)
5. Documenter l'arborescence complète des fichiers

```bash
# Exemple de pipeline
7z x CD1.7z
# Trouver le vrai exe (pas le launcher)
# Extraire les CAB
unshield x DATA1.CAB -d assets/
```

---

## TOOL-002 — Reverser SIMPSONS.EXE dans Ghidra
**Priorité :** P0
**Description :** Le vrai binaire du jeu (pas le launcher 22KB du CD1). Objectifs :
- [ ] Identifier l'entry point et la boucle principale
- [ ] Trouver les fonctions de parsing P3D (pour les chunks inconnus)
- [ ] Trouver les fonctions de parsing de script (.con)
- [ ] Trouver l'algorithme P3DZ (header 0x5A443350)
- [ ] Trouver le format RMV (si nécessaire)
- [ ] Documenter les structures de données clés (GameState, Entity, Vehicle)
- [ ] Renommer les fonctions identifiées dans la base Ghidra
- [ ] Exporter un fichier de symboles

**Setup Ghidra :**
- Créer un projet "SimpsonsHitAndRun"
- Importer le vrai exe (probablement dans les CAB CD2/CD3)
- Analyser avec les options par défaut
- Chercher les strings "p3d", "rcf", "Pure3D" pour trouver les fonctions

---

## TOOL-003 — Créer un script de test Ghidra (headless)
**Priorité :** P3
**Description :** Automatiser l'extraction de données depuis Ghidra :
```python
# Exemple: extraire tous les noms de chunks P3D
fm = currentProgram.getFunctionManager()
funcs = fm.getFunctions(True)
for func in funcs:
    if "Chunk" in func.getName():
        print(func.getName())
```

---

## TOOL-004 — Mettre à jour donut-codegen.exe pour supporter les nouveaux chunks
**Priorité :** P1
**Description :** L'outil existant `dev/codegen/donut-codegen.exe` prend `p3d.json` et génère `P3D.generated.h/.cpp`. Quand on ajoute des chunks à `p3d.json`, il faut relancer le codegen.
- [ ] Vérifier que l'exe fonctionne (Windows ? .NET ? Mono ?)
- [ ] Ajouter les définitions manquantes dans `p3d.json`
- [ ] Relancer la génération
- [ ] Corriger les erreurs de compilation résultantes

---

## TOOL-005 — Outil de visualisation de chunks P3D
**Priorité :** P2
**Description :** La fonction `debugDrawP3D` dans `Game.cpp` affiche un arbre ImGui des chunks. L'étendre en :
- [ ] Hex dump des données brutes
- [ ] Comparaison entre deux fichiers P3D
- [ ] Recherche par type de chunk
- [ ] Export de texture

---

## TOOL-006 — Script d'extraction des commandes depuis l'exe original
**Priorité :** P2
**Description :** Ghidra script pour extraire automatiquement les commandes enregistrées (si elles sont dans une table de strings) pour compléter `cmd.json`.

---

## TOOL-007 — Validation automatique du parsing P3D
**Priorité :** P3
**Description :** Tests unitaires pour le parsing P3D :
- Fichier P3D canonique → parse → re-serialize → compare binaire
- Roundtrip test pour chaque chunk type
- Comparaison entre parsing donut et lecture hex manuelle

---

## TOOL-008 — Documentation du format de script (.con)
**Priorité :** P0
**Description :** Créer `docs/script_format.md` avec :
- Structure du fichier (header, bytecode/texte)
- Liste des opcodes (si bytecode)
- Mapping opcode → commande
- Format des paramètres
- Exemples de scripts décompilés

---

## TOOL-009 — Cross-reference avec les projets similaires
**Priorité :** P3
**Description :** Le moteur Radical a été utilisé dans plusieurs jeux. Étudier les projets de reverse :
- **OpenC1** (Crazy Taxi-like, moteur Radical modifié)
- **Donut Mod Loader** (mods existants pour Simpsons H&R)
- **Lucas' Pure3D Editor** (outil d'édition P3D)
- **P3DView** (visualisateur)
- Documentation Pure3D existante sur les wikis communautaires

---

## TOOL-010 — Setup CI/CD (Azure Pipelines ou GitHub Actions)
**Priorité :** P3
**Description :** Le projet mentionnait Azure Pipelines dans les commits (d3fda55). Configurer une CI :
- Build Linux (GCC/Clang) + Windows (MSVC)
- Tests unitaires (quand ajoutés)
- Code style check (clang-format)
- Le projet a déjà une target `clang-format` dans CMake
