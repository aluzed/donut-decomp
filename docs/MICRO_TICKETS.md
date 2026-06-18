# Micro-tickets restants — découpage fin

## 1. Extraction assets (6 tickets)

### ASSET-001 — Compiler unshield en statique
- Compiler `unshield` depuis https://github.com/twogood/unshield
- S'assurer que la commande `unshield -d out x data1.cab` fonctionne
- **~5 min**

### ASSET-002 — Extraire data1.cab + data2.cab depuis le CD1
- Utiliser `7z e` pour extraire les CABs de l'ISO
- Placer dans `assets/`
- **~10 min**

### ASSET-003 — Tester l'extraction d'un fichier P3D
- `unshield x data1.cab` → vérifier que `Art/b00.p3d` est extrait
- Ouvrir le fichier avec `P3DFile` dans donut
- **~15 min**

### ASSET-004 — Extraire tous les fichiers du CAB
- Lancer l'extraction complète
- Organiser dans l'arborescence `art/`, `audio/`, `movies/`
- **~20 min**

### ASSET-005 — Charger un vrai modèle de personnage
- Remplacer `SimpleMesh` par le P3D chargé depuis `art/chars/homer_m.p3d`
- Vérifier que le skinning + animations marchent
- **~30 min**

### ASSET-006 — Charger les textures et shaders du jeu
- Charger les vraies textures depuis les P3D du CAB
- Vérifier que les shaders P3D sont correctement interprétés
- **~30 min**

---

## 2. IA Police/Poursuite (8 tickets)

### POLICE-001 — Créer une classe ChaseManager
- Stocker le niveau de heat (0-5)
- Stocker la voiture de police à spawn
- **~15 min**

### POLICE-002 — Spawn d'une voiture de police
- Créer un Vehicle avec CreatePhysicsBody
- Placer à une intersection proche du joueur
- **~20 min**

### POLICE-003 — IA de poursuite basique
- La voiture de police suit le joueur
- Utilise `seekSteer` vers la position du joueur
- **~20 min**

### POLICE-004 — Gestion du heat level
- Augmente quand le joueur percute des voitures de trafic
- Diminue avec le temps
- **~15 min**

### POLICE-005 — Multiples voitures de police
- Spawn 1-5 voitures selon le heat level
- Chacune suit le joueur avec un offset
- **~20 min**

### POLICE-006 — Comportement de déploiement
- Les voitures apparaissent devant le joueur (pas derrière)
- Utilisent les Paths pour trouver des positions
- **~20 min**

### POLICE-007 — Arrestation
- Si une voiture de police touche le joueur → BUSTED!
- Afficher "BUSTED!" → MissionFailed
- **~15 min**

### POLICE-008 — Évasion
- Si le joueur s'éloigne assez des voitures → heat baisse
- Heat à 0 → EVADED! → poursuite terminée
- **~15 min**

---

## 3. UI Menus/Boutons (6 tickets)

### UI-001 — Détection de clic souris sur FrontendMultiSprite
- Lire la position de la souris via SDL
- Comparer avec les sprites chargés
- Loguer le nom du sprite cliqué
- **~20 min**

### UI-002 — Callbacks de navigation
- Associer un nom de sprite à un ScreenName
- Sur clic, changer de screen (pile LIFO)
- **~15 min**

### UI-003 — États visuels des boutons
- Normal : opacité 100%
- Hover : opacité 80% + surbrillance
- Pressed : opacité 60%
- Disabled : grisé
- **~20 min**

### UI-004 — Rendu de texte (FrontendMultiText)
- Lire les données MultiText du P3D
- Dessiner le texte avec TextureFont via SpriteBatch
- Gérer l'alignement horizontal/vertical
- **~30 min**

### UI-005 — Écran principal (MainMenu)
- Charger `BootScreen.p3d` → afficher les boutons
- "New Game" → charger le niveau
- "Options" → écran options (placeholder)
- "Quit" → fermer le jeu
- **~30 min**

### UI-006 — Écran pause in-game
- Afficher un overlay semi-transparent
- "Resume", "Restart", "Main Menu", "Quit"
- Navigation au clavier (flèches + Enter)
- **~25 min**

---

## 4. Audio streaming/dialogues (5 tickets)

### AUDIO-001 — Streaming double-buffer
- Créer 2 buffers OpenAL pour la musique
- Remplir buffer 1, jouer, remplir buffer 2 pendant que 1 joue
- Boucler
- **~30 min**

### AUDIO-002 — Charger un fichier RCF de musique
- Extraire `music00.rcf` du CAB
- Charger via `AudioManager::LoadRCF`
- Jouer en streaming
- **~20 min**

### AUDIO-003 — Système de dialogues priorisés
- File d'attente : `std::deque<Dialogue>`
- Niveau 0 (cutscene) interrompt tout
- Niveau 1 (mission) interrompt niveau 2
- Niveau 2 (gameplay) joue si file vide
- **~20 min**

### AUDIO-004 — Callback de fin de son
- Détecter quand une source OpenAL termine
- Déclencher le prochain dialogue dans la file
- **~20 min**

### AUDIO-005 — Sous-titres synchronisés
- Extraire le texte des bibles depuis les P3D Frontend
- Afficher le texte en bas de l'écran pendant la durée du son
- **~25 min**

---

## 5. Post-FX / Ombres (5 tickets)

### FX-001 — Bloom basique
- Rendre la scène dans un FBO
- Flou gaussien (2 passes)
- Additionner à l'image originale
- **~30 min**

### FX-002 — Vignette
- Ajouter un overlay sombre sur les bords dans le shader post-process
- Intensité configurable
- **~10 min**

### FX-003 — Motion blur
- Sauvegarder la matrice viewProj de la frame précédente
- Reprojetter les pixels dans le shader
- Mélanger avec l'image courante
- **~30 min**

### FX-004 — Shadow mapping basique
- Rendre la scène depuis la lumière directionnelle dans un depth FBO
- Projeter la depth texture dans le shader world
- Comparer pour déterminer les ombres
- **~45 min**

### FX-005 — Color grading LUT
- Charger une texture LUT 16×16×16
- Appliquer dans le shader post-process
- Ajuster contraste/saturation
- **~20 min**

---

## Total : 30 micro-tickets

Tous faisables en sessions de 10-45 minutes.
Priorité suggérée : ASSET (débloque les vrais graphismes) → POLICE (gameplay).
