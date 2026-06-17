# Tickets restants — Audit final

## État réel

| Système | Complétion | Verdict |
|---------|-----------|---------|
| Parsing P3D/RCL | 85% | Solide |
| Rendu | 60% | Viewer, pas de lightmaps/shadows/post-FX |
| Physique | 55% | Collisions statiques OK, pas de dynamique |
| Audio | 40% | Lecture basique OK, pas de dialogue/streaming |
| Scripting | 3% | Parser OK, **202/218 commandes vides** |
| Game flow | 5% | Boucle OK, pas de state machine |
| Character ctrl | 45% | Mouvement OK, `onGround()`/`canJump()` cassé |
| Vehicle | 2% | Squelette, pas de physique |
| AI/Traffic/Peds | 0% | Rien |
| UI/HUD | 20% | Sprites OK, pas de navigation/boutons |
| Save/Load | 0% | Rien |

## Tickets restants par catégorie

### CHARCTRL — Character Controller (4 tickets)

- **CHARCTRL-001** : Fix `onGround()` — utiliser le contact normal ou raycast au lieu de `_verticalVelocity >= 0`
- **CHARCTRL-002** : Fix `jump()` — utiliser le paramètre `dir`, ajouter cooldown
- **CHARCTRL-003** : Velocity caps (`_verticalVelocity` max) + fix `stepDown()` convexSweep
- **CHARCTRL-004** : Remplacer `stepUp()` naïf par sweep test avec détection plafond

### VEH — Vehicle (5 tickets)

- **VEH-001** : `CreatePhysicsBody()` — créer `btRaycastVehicle` avec chassis + 4 roues
- **VEH-002** : Wirer ~15 commandes tuning (SetMass, SetGasScale, SetTopSpeedKmh...)
- **VEH-003** : Input conduite (WASD → throttle/steer/brake)
- **VEH-004** : Caméra follow véhicule + enter/exit
- **VEH-005** : Dégâts visuels (swap car_damage.p3d)

### LEVEL — Level loading (5 tickets)

- **LEVEL-001** : `unloadRegion()` retire vraiment les entités/physique
- **LEVEL-002** : Transitions intérieur (@/$ dans DynaLoadData)
- **LEVEL-003** : Streaming par distance (charger/décharger selon position joueur)
- **LEVEL-004** : Système de triggers (Locator2/TriggerVolume → callbacks script)
- **LEVEL-005** : Fog depuis paramètres P3D (par zone)

### SCRIPT — Scripting priorisé (5 tickets)

- **SCRIPT-001** : `SelectMission` charge le `.con`, `AddStage`/`CloseStage` gèrent le cycle
- **SCRIPT-002** : Commandes tuning véhicule (~6) → Vehicle physique
- **SCRIPT-003** : Système objectifs/conditions (~6) → win/lose detection
- **SCRIPT-004** : Trafic basique (~3) → spawn voitures sur Road paths
- **SCRIPT-005** : Gags/collectibles (~5) → spawn props avec trigger

### GAME — Game flow (5 tickets)

- **GAME-001** : State machine (Splash→MainMenu→Loading→InGame→Paused→MissionComplete)
- **GAME-002** : HUD (vitesse, vie, timer, objectif)
- **GAME-003** : Mission loading (SelectMission→charge .con→exécute)
- **GAME-004** : Menu pause (Resume/Restart/Quit)
- **GAME-005** : Détection victoire/défaite (objectifs, conditions, mort)

### AI — Intelligence artificielle (5 tickets)

- **AI-001** : Spawn trafic sur Road paths
- **AI-002** : Path-following controller (steer vers waypoint)
- **AI-003** : Spawn piétons sur trottoirs
- **AI-004** : ChaseManager — police qui poursuit
- **AI-005** : IA course — adversaires avec rubber-banding

### AUDIO — Audio (5 tickets)

- **AUDIO-001** : Dialogue priority queue (cutscene > gameplay)
- **AUDIO-002** : Sous-titres synchronisés (timed text events)
- **AUDIO-003** : Streaming musique (double buffer OpenAL)
- **AUDIO-004** : Index hash sons (O(1) lookup au lieu de O(n))
- **AUDIO-005** : Reverb par zone (EFX intérieur/extérieur)

### UI — Interface (5 tickets)

- **UI-001** : Navigation screens/pages (pile de pages actives)
- **UI-002** : Mouse picking + boutons (hover/pressed/disabled)
- **UI-003** : Rendu MultiText (TextureFont)
- **UI-004** : HUD core (vie, vitesse, compteur pièces)
- **UI-005** : Menu pause overlay

### RENDER — Rendu (5 tickets)

- **RENDER-001** : Lightmap support (2ᵉ canal UV + texture lightmap)
- **RENDER-002** : CompositeModel_AnimObjectWrapper shaders/textures
- **RENDER-003** : Frustum culling basique
- **RENDER-004** : Fog paramétrable par zone P3D
- **RENDER-005** : LOD basique (distance-based model swap)

### PHYS — Physique (5 tickets)

- **PHYS-001** : Gravité correcte (~9.8) + réconcilier perso/monde
- **PHYS-002** : Utiliser BulletFenceShape au lieu de btBoxShape
- **PHYS-003** : Rigid bodies pour AnimDynamicPhysics/DynamicPhysics
- **PHYS-004** : Collision callbacks (dégâts, pickup, son)
- **PHYS-005** : Surface friction (route vs herbe vs trottoir)

---

**Total restant : 49 tickets répartis en 10 catégories.**

**Chemin critique** (minimum playable) : CHARCTRL-001, VEH-001+003, SCRIPT-001+002, GAME-001+002, AI-001 ≈ **8 tickets**.
