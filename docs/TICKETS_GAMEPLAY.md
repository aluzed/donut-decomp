# Tickets Gameplay

## GAME-001 — Refactor Game::Run (God Object)
**Priorité :** P1
**Fichier :** `src/Game.h`, `src/Game.cpp` (603 lignes)
**Description :** La classe `Game` est un énorme God Object qui fait tout : init SDL, créer les fenêtres, charger le niveau, créer les personnages, gérer ImGui, render loop...
**Correctif :** Créer des classes séparées :
- `GameState` — état global du jeu (menu, in-game, paused)
- `GameRenderer` — pipeline de rendu (extrait de Game::Run)
- `GameLoop` — boucle principale propre
- Garder `Game` comme orchestrateur fin

---

## GAME-002 — Gestion des 3 CDs / structure de fichiers
**Priorité :** P1
**Description :** Le jeu PC original est sur 3 CDs. Implémenter le montage virtuel ou la copie des assets dans une arborescence attendue :
```
assets/
├── art/
│   ├── chars/      # Modèles et animations personnages
│   ├── frontend/   # Menus
│   ├── level/      # Niveaux
│   └── global.p3d  # Textures globales
├── audio/
│   ├── dialog/     # DIALOGF.RCF ...
│   ├── music/      # musiqu00.RCF ...
│   └── ...
├── movies/         # RMV videos
├── scripts/        # Fichiers .con
└── simpsons.exe    # Original binaire (pour reverse)
```

---

## GAME-003 — GameState machine (Menus → Jeu → Pause)
**Priorité :** P1
**Description :** États du jeu :
1. **Splash/Intro** — Logos (Fox, Radical, Gracie Films) → vidéos RMV
2. **Main Menu** — Frontend chargé depuis BootScreen.p3d
3. **Level Select** — Choix du niveau (7 niveaux)
4. **In-Game** — Gameplay
5. **Pause Menu** — Frontend pause
6. **Mission Complete/Fail** — Écrans de résultat
7. **Credits** — RMV + frontend

---

## GAME-004 — Player (contrôles joueur)
**Priorité :** P0 (bloquant)
**Description :** Le joueur peut actuellement bouger une caméra libre (WASD). Implémenter :
- [ ] Contrôle personnage à la 3ème personne (déplacement, rotation caméra)
- [ ] Saut
- [ ] Interaction (parler, entrer dans véhicule)
- [ ] Entrée/sortie de véhicule
- [ ] Coup de pied (attaque)

**Correctif :** Intégrer `Character` + `CharacterController` avec les inputs. Actuellement `Game::Run` appelle `_freeCamera->Move()` et `_freeCamera->LookDelta()` — remplacer par un vrai contrôle de personnage.

---

## GAME-005 — Système de véhicules
**Priorité :** P1
**Description :** Gameplay de conduite :
- [ ] Modèle de véhicule (P3D chargé depuis `art/cars/`)
- [ ] Physique de véhicule (PHYS-007)
- [ ] Entrée/sortie du véhicule
- [ ] Caméra de conduite (follow cam, caméra arrière)
- [ ] Nitro / boost
- [ ] Dégâts visuels (car_damage.p3d)
- [ ] Klaxon

---

## GAME-006 — Caméra de jeu (third-person follow)
**Priorité :** P1
**Description :** Remplacer la `FreeCamera` debug par une caméra de jeu :
- Follow cam derrière le personnage/véhicule
- Orbite manuelle (stick droit)
- Collision caméra (ne pas traverser les murs)
- Caméra de conversation (champ/contrechamp, P3D FollowCameraData)
- Caméra animée (AnimCamera, P3D Cam chunks)

---

## GAME-007 — IA de base (trafic)
**Priorité :** P2
**Description :** Le jeu a un trafic procédural sur les routes. Les Road/RoadSegment/Intersection chunks définissent le réseau routier.
- Voitures qui suivent les routes
- Respect des intersections (feux, stops)
- Évitement de collisions simples
- Spawn/despawn selon distance joueur

---

## GAME-008 — IA piétons
**Priorité :** P2
**Description :** Piétons sur les trottoirs :
- Marche aléatoire
- Réaction au joueur (fuite si course, cri si klaxon)
- Pathfinding simple (Waypoint/Path chunks)

---

## GAME-009 — IA de mission
**Priorité :** P2
**Description :** Personnages de mission avec comportements scriptés :
- Suivre un chemin (race)
- Suivre le joueur
- Fuir le joueur
- Ramasser/collecter des objets
- Attaquer (boss fights)

---

## GAME-010 — Système de collectibles
**Priorité :** P2
**Description :** 
- Coins (monnaie) — achat de véhicules et costumes
- Cartes à collectionner (7 par niveau)
- Gags (jokes visuels, ~50 par niveau)
- État persistant entre sessions (sauvegarde)

---

## GAME-011 — Sauvegarde / Chargement
**Priorité :** P2
**Description :** Format de sauvegarde : état du joueur (position, véhicule, argent, progression missions, collectibles). Format binaire simple (JSON ?).

---

## GAME-012 — Système de dégâts
**Priorité :** P3
**Description :** Dégâts visuels sur les véhicules et objets destructibles (BreakableObject chunks). Interpolation entre états de dégâts.

---

## GAME-013 — Level loading complet (DynaLoad)
**Priorité :** P1
**Fichier :** `src/Level.cpp` (DynaLoadData)
**Description :** Tout le parsing de `DynaLoadData` est commenté. C'est le système de chargement dynamique des zones (intérieurs, sections de niveau). Sans ça, seules les zones loadées dans Game::Game sont visibles.
**Correctif :** Reverser le format DynaLoad et implémenter le chargement par zone.

---

## GAME-014 — Intérieurs (Interior/Locator)
**Priorité :** P1
**Description :** Les intérieurs (maison des Simpsons, école, Kwik-E-Mart...) sont chargés dynamiquement via DynaLoad. Les Locator avec type Interior/E vent déclenchent le chargement. Implémenter la transition intérieur/extérieur.

---

## GAME-015 — Debug UI (ImGui) : le garder mais le cacher en mode jeu
**Priorité :** P3
**Description :** L'UI ImGui actuelle est utile pour le dev. Ajouter un toggle (F1 ? `) pour l'afficher/cacher en mode release. Créer une console de commandes intégrée.
