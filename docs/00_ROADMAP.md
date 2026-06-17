# donut Engine - Roadmap de Reverse Engineering

## État des lieux

**Projet :** Réimplémentation open source du moteur de *The Simpsons: Hit & Run* (2003, PC)
**Moteur original :** Radical Engine (Pure3D + RCL)
**Langage :** C++17 / OpenGL 4.3
**Assets requis :** Fichiers originaux du jeu (.p3d, .rcf, .rsd, scripts .con)

### Modules existants

| Module | Statut | Fichiers | Chunks P3D parsés | Notes |
|--------|--------|----------|-------------------|-------|
| **P3D** | 90% | 6 | 139/182 | Il manque ~50 chunk types (particules, lumières, etc.) |
| **RCL** | 100% | 4 | N/A | Archive RCF + audio RSD complets |
| **Core** | 95% | 18 | N/A | Bugs mineurs sur Matrix3x3, Quaternion, Vector4 |
| **Render** | 80% | 32 | N/A | SkinAnimation et DrawBox sont stubbés |
| **Physics** | 50% | 8 | N/A | Step simulation désactivé, CharacterController partiel |
| **Audio** | 40% | 2 | N/A | Une seule source, pas de cache |
| **Scripting** | 5% | 4 | N/A | 185 commandes définies, 1 implémentée |
| **Top-level** | 70% | 25 | N/A | Level chargé, Game::Run fonctionnel mais viewer-only |

### Assets disponibles
- `files/` : 3 CD du jeu (ISO dans .7z)
  - CD1 : SIMPSONS.EXE (22KB launcher), DIALOGF.RCF (159MB), MOVIES/
  - CD2/CD3 : Assets du jeu (P3D, RCF, Scripts) — extraire des CAB
- L'exécutable original est un petit launcher ; la logique est dans le CAB/DLL

---

## Architecture cible du moteur

```
donut
├── Core/           # Types mathématiques, I/O, mémoire, logging
├── P3D/            # Parsing du format Pure3D (~182 chunk types)
├── RCL/            # Archives RCF, audio RSD/RADP
├── Render/         # OpenGL 4.3 : meshes, shaders, skinning, sprites, post-FX
├── Physics/        # Intégration Bullet Physics
├── Audio/          # OpenAL : spatial audio, musique, dialogues
├── Scripting/      # Moteur de script (VM) exécutant les missions .con
├── Game/           # Gameplay : personnages, véhicules, IA, missions
├── UI/             # Frontend (menus, HUD) — basé sur FrontendProject existant
├── Input/          # Gestion clavier/souris/manette
└── docs/           # Documentation et tickets
```

---

## Phases et priorités

### Phase 1 : Fondations (bugs critiques, parsing P3D manquant)
Fixer ce qui empêche le projet de fonctionner correctement.

### Phase 2 : Rendu et physique
Rendre le viewer pleinement fonctionnel : toutes les entités, animations, physique active.

### Phase 3 : Gameplay de base
Personnage jouable, véhicules, collisions, caméra de jeu.

### Phase 4 : Mission system
Scripting VM, exécution des missions, IA de base.

### Phase 5 : Polish
Audio spatial, post-processing, UI complète, multiplateforme.

---

## Liste complète des tickets

Voir les fichiers détaillés par module :
- [TICKETS_CORE.md](TICKETS_CORE.md) — Bugs mathématiques, logging, P3DZ/RZ
- [TICKETS_P3D.md](TICKETS_P3D.md) — Chunks P3D manquants, codegen
- [TICKETS_RENDER.md](TICKETS_RENDER.md) — SkinAnimation, DrawBox, LOD, post-FX
- [TICKETS_PHYSICS.md](TICKETS_PHYSICS.md) — Physique véhicules, CharacterController
- [TICKETS_AUDIO.md](TICKETS_AUDIO.md) — Audio spatial, multi-sources, streaming
- [TICKETS_SCRIPTING.md](TICKETS_SCRIPTING.md) — Script VM, commandes jeu
- [TICKETS_GAMEPLAY.md](TICKETS_GAMEPLAY.md) — Joueur, véhicules, IA, missions
- [TICKETS_UI.md](TICKETS_UI.md) — Frontend complet, HUD, menus
- [TICKETS_TOOLS.md](TICKETS_TOOLS.md) — Outils, codegen, Ghidra
