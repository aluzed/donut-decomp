# donut — Backlog (source de vérité unique)

> **Ce fichier remplace** les 11 fichiers de tickets fragmentés et leurs **3 générations
> de numérotation contradictoires** : les `TICKETS_*.md` par module (G1), `MICRO_TICKETS.md`
> (G2) et `TICKETS_RESTANTS.md` (G3). Les anciens IDs sont mappés en bas (§ Crosswalk).
>
> Numérotation canonique = préfixes par module (G1), les plus granulaires et seuls à porter
> des pointeurs `fichier:ligne`.
>
> **Statuts vérifiés contre le code** (audit src/, 2026-06-23), pas contre les anciennes docs.

## Légende

| Statut | Sens |
|--------|------|
| ✅ DONE | Implémenté et câblé, vérifié dans le code |
| 🟡 PARTIAL | Squelette présent, pièces manquantes (détaillées) |
| ⬜ TODO | Absent / commenté / stub vide |
| 🧩 EPIC | Trop gros — éclaté en sous-tickets plus bas |

---

## A. Clôturés — déjà livrés (ne pas refaire)

Vérifié dans le code. Ces tickets étaient marqués « ouverts / P0 bloquants » dans les anciennes docs mais sont **faits**.

| Ancien ID(s) | Sujet | Preuve |
|---|---|---|
| RENDER-001/004/005 (G1) | Skeletal animation (`Track::Evaluate`, lerp/slerp, `UpdatePose`, upload GPU) | `Render/SkinAnimation.cpp:13-21`, `.h:41-82`; `Character.cpp:108-122` |
| RENDER-002/003 (G1) | `LineRenderer::DrawBox` (3 variantes + AABBox) | `LineRenderer.cpp:82-135` |
| RENDER-010 (G1) | Fog world/skin | STATUS + shaders |
| CORE-001..010 (G1) | Bugs math Vector2/4, Matrix3x3 Inverse, Quaternion Euler | shippé (cf. STATUS) — confirmé indirectement (anim/caméra fonctionnent) |
| CORE-013 (G1) — partie RZ | Décompression RZ | RZ OK (P3DZ reste, cf. P3D-016) |
| PHYS-001 (G1) | `stepSimulation` actif | physique tourne |
| PHYS-002 (G1) | CharacterController complet (jump, onGround, stepUp/Down, sweep) | `CharacterController.cpp:67-475` |
| **CHARCTRL-004 (G3)** | stepUp = convex sweep test | **fait** : `CharacterController.cpp:311-347` (le « ticket critique » G3 est clos) |
| PHYS-003 (G1) | Collisions statiques intégrées au monde | `Level.cpp` (volumes câblés) |
| PHYS-007 (G1) | Véhicule `btRaycastVehicle` (4 roues, suspension, boost ×3, jump) | `Vehicle.cpp:32-146`; `Game.cpp:425-525` |
| GAME-004 (G1) | Contrôles personnage 3e pers. (move/jump/interact/enter-exit) | `Game.cpp:459-510` |
| GAME-006 (G1) | Caméra follow + orbite souris + freecam | `Game.cpp:403-554`; `FreeCamera.cpp` |
| GAME-013 (G1) | Parsing DynaLoadData | `Level.cpp:227-251` |
| SCRIPT-001/002/003 (G1) | ScriptEngine + machine d'état mission (Select→AddStage→Close) | `Scripting/ScriptEngine.cpp:1-420` |
| GAME-007 (G1), POLICE-* (G2), AI-004 (G3) | Traffic AI, ChaseManager (heat, busted) | `AI/TrafficManager.cpp`, `ChaseManager.cpp` |
| GAME-008 (G1), AI-003 (G3) | Pedestrian AI — boucle Update réactivée le 2026-08-22 (REGR-001) | `AI/PedestrianManager.cpp`; appelée depuis `Game.cpp:813` |
| AUDIO-001/003 (G1) | Pool 24 sources, audio spatial, sons procéduraux, file dialogue | `Audio/AudioManager.cpp:98-143` |
| LEVEL-004 (G3) — parse | Locator2/TriggerVolume parsés + stockés | `Level.cpp:177-199` (reste : câbler les callbacks → LEVEL-TRIG) |

---

## B. Backlog ouvert

### B.0 — Bloquants (P0)
| ID | Statut | Sujet | Pointeur |
|---|---|---|---|
| **REGR-001** | ✅ | **Boucle gameplay désactivée réactivée.** La cause du crash n'était pas dans le bloc : `db9c8db` l'a commenté pour isoler un crash que `9860a4f` a corrigé le même jour (`Character::_rotation` non initialisé + bugs `stepUp`/`stepDown`/`onGround`) ; le désactivage a survécu 22 commits. Vérifié 2026-08-22 : 91 882 exécutions, 0 crash, triggers ✅, collectibles ✅, piétons ✅ | `Game.cpp:813` + `Game.cpp:643` |
| **CHARCTRL-FALL** | ✅ | **Le personnage traversait le sol dès qu'il avançait.** Cause : aucun `btGhostPairCallback` installé sur le broadphase, donc le ghost object du personnage n'accumulait aucune paire — ni dépénétration, ni collision horizontale. 56 chutes → **0** sur le même parcours de 60 s | `WorldPhysics.cpp` (constructeur) |
| **LEVEL-MISSIONP3D** | ✅ | **Aucun locator de mission n'était chargé.** Ils vivent dans `art/missions/level01/level.p3d` (124) et `<mission>.p3d` (11), que rien n'ouvrait — et `Locator2` ne lisait même pas sa propre position, si bien que `Level` jetait tout locator sans trigger. Layout reversé et vérifié sur 1045 occurrences | `Locator2`, `Level.cpp`, `ScriptEngine.cpp` |
| **SCRIPT-PARSE** | ✅ | **`find_first_of("//")` tronquait toute commande `.con` ayant un `/` dans un argument** (il cherche un `/` isolé, pas la séquence). `AddStageVehicle(..., "Missions/level01/M1race.con", ...)` et `SetPresentationBitmap("art/...")` étaient rejetées sans message — d'où l'absence de véhicule de mission. Corrigé aussi : `stdout` bufferisé qui perdait les journaux, téléport debug hors de portée, `E` qui faisait entrer puis sortir | `Commands.h`, `Core/Log.h`, `Game.cpp` |
| **MISSION-LOOP** | ✅ | **Chaque `.con` terminait sa propre mission à la fin du parsing.** `CloseMission()` (marqueur de fin de bloc) déclenchait `GameState::MissionComplete`, d'où « STAGE COMPLETE! Time: 0.0s » en boucle et aucun véhicule atteignable. La complétion revient au gameplay | `ScriptEngine.cpp`, `Game.cpp` |
| **UI-SPLASH** | ✅ | **Aucun texte 2D ni ligne de debug dessinés.** Les shaders inline de `SpriteBatch` et `LineRenderer` (`#version 150 core`, sans `layout(location)`) laissaient l'éditeur de liens GL choisir les emplacements d'attributs — sur AMD `position=2, texcoord=0, color=1` au lieu de 0/1/2 — donc le shader lisait la position dans les octets de couleur. L'état n'était pas bloqué : le menu était invisible | `SpriteBatch.cpp`, `LineRenderer.cpp` |

> ✅ Plus de bloquant P0. La boucle complète est jouable et **observable** :
> Splash → Entrée → menu → « New Game » → en jeu, avec HUD, piétons, collectibles, triggers,
> véhicule, et le debug draw Bullet de nouveau visible.

### CORE — nettoyage (P3)
| ID | Statut | Sujet | Pointeur |
|---|---|---|---|
| CORE-014 | ✅ | Log : Warn/Error→stderr, filtrage niveau, remplacer `cout`/`printf` épars | `Core/Log.h` |
| CORE-015 | ✅ | Retirer includes `<iostream>`/`<fmt>` superflus → `Core/Log.h` | multiple |

### P3D — chunks manquants (additif, déjà bien scopé)
| ID | Statut | Pri | Sujet |
|---|---|---|---|
| P3D-013 | ✅ | P3 | `GetShaderTexture()` — **faux bug** : 0 occurrence dans `src/` (ni déclaration ni appel). Ticket = confirmer et clore. Rétrogradé de P0 |
| P3D-001 | ⬜ | P1 | Particle System (8 chunks) — fumée/feu/nitro |
| P3D-006 | ⬜ | P1 | Entity variants (`AnimDynamicPhysicsWrapper`, `AnimCollision`…) |
| P3D-008 | 🟡 | P1 | Spline / LocatorMatrix / SurfaceTypeList : classes ajoutées, layouts **reversés et vérifiés sur 7892 occurrences** (2 définitions de `p3d.json` sur 3 étaient fausses). Splines exposées par nom (13 rails de caméra). Reste `SurfaceTypeList` côté physique → `PHYS-008` |
| P3D-009 | ⬜ | P1 | CollisionWallVolume / CollisionEffect / Atc |
| P3D-002 | ⬜ | P2 | Light chunks (headlights, lumières dynamiques) |
| P3D-004 | ⬜ | P2 | Animation controllers (channels float/vector) |
| P3D-005 | ⬜ | P2 | Frontend text/bible (localisation) |
| P3D-007 | ⬜ | P2 | Tree/TreeNode (spatial — prérequis culling) |
| P3D-010 | ⬜ | P2 | StaticProp (6 chunks) |
| P3D-011 | ⬜ | P2 | Geometry (Mesh, PackedNormalList, DrawShadow…) |
| P3D-014 | ⬜ | P2 | CompositeDrawableSkin/Effect parsing |
| P3D-016 | ⬜ | P2 | **P3DZ** (compression proprio, header `0x5A443350`) — Ghidra |
| P3D-003 | ⬜ | P3 | Expression/facial anim |
| P3D-012 | ⬜ | P3 | ExportInfo (métadonnées 3ds Max) |
| P3D-017 | ⬜ | P3 | Images non-PNG (palettisé, DXT) |

### RENDER
| ID | Statut | Pri | Sujet | Pointeur / note |
|---|---|---|---|---|
| RENDER-006 | 🟡 | P1 | AnimObjectWrapper : peupler `_shaders`/`_textures` | `Render/CompositeModel.h:37-43` — vecteurs déclarés, **restent vides** |
| RENDER-007 | ⬜ | P2 | Méthodes `CompositeDrawable` | `CompositeModel.h:96-105` — déclaration seule |
| RENDER-011 | ⬜ | P2 | Billboards WorldSphere (lens flare, halo soleil) | `WorldSphere.cpp` — commenté |
| RENDER-014 | ⬜ | P2 | Multi-UV (canal 1 = lightmap) | prérequis RENDER-015 |
| RENDER-015 | ⬜ | P2 | Lightmaps / éclairage statique | dépend RENDER-014 |
| RENDER-013 | ✅ | P3 | Cache/précompilation shaders | recompile à chaque lancement |
| RENDER-008 | 🧩 | P2 | LOD + culling spatial | → § C.3 |
| RENDER-009 | 🧩 | P3 | Pipeline post-process | → § C.4 (FBO `_sceneFBO` alloué jamais rendu, `Game.h:116-117`) |

### PHYS
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| UI-RACE-GUIDE | ✅ | P1 | Les repères de checkpoint sortent du bloc debug, le radar montre le circuit avec le prochain en doré, et le HUD affiche « Next checkpoint: N m ». Le franchissement reste à confirmer à la main | `Hud.*`, `Game.cpp` |
| VEH-SINK | ✅ | P1 | **La voiture ne roulait pas** (le châssis s'endormait et `applyEngineForce` n'active pas un corps endormi) et **la caméra passait sous le décor** (offset pivoté par le tangage du châssis). Le décor statique est aussi passé en `btRigidBody` de masse nulle, sans quoi les roues ne trouvaient pas le sol. 58 km/h, 55 s de conduite sans chute | `Vehicle.cpp`, `Game.cpp`, `WorldPhysics.cpp` |
| PHYS-004 | ⬜ | P1 | Callbacks collision (damage / pickup / sound) | seules les vérifs manifold/pénétration existent |
| PHYS-011 | ⬜ | P1 | Corps rigides dynamiques pour `AnimDynamicPhysics` | `Level.cpp:149-169` — instancié visuellement, **pas de body** |
| PHYS-005 | ✅ | P2 | Utiliser `BulletFenceShape` au lieu de `btBoxShape` | `AddP3DFence` |
| PHYS-008 | ⬜ | P2 | Frictions par surface (route/herbe/trottoir) | actuellement `_tireGrip` figé, pas de système matériau |

### AUDIO
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| AUDIO-002 | 🟡 | P1 | **Finir** streaming musique double-buffer | `MusicSource` déclaré (`AudioManager.h:71-79`), `PlayMusic()` incomplet |
| AUDIO-007 | 🟡 | P2 | Dialogue : **sous-titres** + lip sync | queue priorité ✅ ; sous-titres **absents** |
| AUDIO-006 | ✅ | P3 | Auto-scan dossier `audio/` (au lieu de 10 noms en dur) | constructeur |
| AUDIO-008 | ⬜ | P3 | EFX reverb par zone (intérieur/extérieur) | |
| AUDIO-009 | ✅ | P3 | Son de klaxon (actuellement `Game.cpp:508` log « HONK! ») | quick win |

### AI
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| AI-RACE | 🟡 | P2 | Adversaire de course + rubber-banding | `RaceOpponent` pilote un vrai `Vehicle` via `ApplyInput`, rubber-banding borné OK. **Se coince** avant de boucler un tour. Deux causes écartées (tracé rééchantillonné à 12 m, accélérateur piloté par l'angle et non par la distance) ; reste un blocage reproductible à (109, 2, -559). **Débloqué depuis** : `m1_AI_path1/1b/4` et `race_finish` sont des `Locator2` du P3D de mission, désormais chargés (cf. LEVEL-MISSIONP3D). Reste à relier ces trois points isolés en trajectoire via le réseau de `Path` |
| AI-PATH | 🟡 | P2 | Contrôleur path-following réutilisable | extrait dans `src/AI/PathFollower.*` et réutilisé par `RaceOpponent` ; `PathGraph::FindRoute` ajoute un A\*. `TrafficManager` garde ses copies. Le vrai réseau routier (60 `Intersection` + 99 `Road`, layouts vérifiés) est désormais intégré au graphe : connexité passée de **110 composantes / 4 %** à **15 / 91 %** |

### GAME / flow
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| GAME-003 | 🟡 | P1 | Machine d'état complète (splash→menu→levelselect→game→pause→result→credits) | menus déclarés non rendus |
| GAME-010 | ⬜ | P2 | Collectibles (coins / cards 7 / gags ~50) + état persistant | |
| GAME-011 | ⬜ | P2 | Save/Load complet (auj. best-time seul) | |
| GAME-014 | ⬜ | P1 | Intérieurs (maison Simpson, école, Kwik-E-Mart) chargés dynamiquement | |
| GAME-001 | 🧩 | P1 | Refactor `Game` God object (1499 l.) | → § C.1 |

### VEH
| ID | Statut | Pri | Sujet |
|---|---|---|---|
| VEH-DAMAGE | ⬜ | P3 | Dégâts visuels (swap `car_damage.p3d`) |

### LEVEL
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| LEVEL-TRIG | 🟡 | P1 | Câbler triggers parsés → callbacks script (`ActivateTrigger`/`DeactivateTrigger`) | volumes déjà parsés (`Level.cpp:177`) |
| LEVEL-STREAM | ⬜ | P1 | Streaming par distance (load/unload selon position joueur) | **absent** — pas de culling viewport |
| LEVEL-FOG | ⬜ | P2 | Fog par zone depuis paramètres P3D | |

### UI / HUD
| ID | Statut | Pri | Sujet | Note |
|---|---|---|---|---|
| UI-MENU | ✅ | P1 | Menus principal et pause opérationnels : hit-test aligné sur le rendu, empilement Y vers le bas centré, pause qui **gèle réellement** la simulation (elle tournait derrière l'overlay), `LoadP3D(fichier, page)` filtrable | `Game.cpp`, `GameMenu.h`, `FrontendProject.cpp` |
| UI-TEXT | 🟡 | P1 | MultiText + text bible (localisation) via TextureFont | SpriteBatch dispo ; MultiText lu non rendu |
| UI-HUD | ✅ | P1 | HUD complet : barres santé/Hit&Run/nitro, compteur de vitesse en véhicule, pièces/cartes, objectif+tour+timer, radar. Rendu extrait vers `src/UI/Hud.*`, `Game` ne remplit plus qu'un `HudState`. Nitro et cartes restent des placeholders (cf. ticket) | `src/UI/Hud.*` |
| UI-LANG | ⬜ | P2 | Bibles de langue | dépend P3D-005 |
| UI-LOADING | ⬜ | P3 | Écran de chargement + hints |
| UI-FMV | ⬜ | P3 | Lecteur RMV (Radical Movie) | reverse format |

### TOOLS / Assets (chantier transverse)
| ID | Statut | Pri | Sujet |
|---|---|---|---|
| ASSET-001..006 | 🟡 | P0 | Pipeline extraction CAB/ISO → arbo `assets/` (unshield, 7z) |
| TOOL-002 | ⬜ | P0 | Reverse `SIMPSONS.EXE` (Ghidra) : main loop, P3D/.con parse, P3DZ, symboles |
| TOOL-004 | ⬜ | P1 | Faire tourner `donut-codegen` + enrichir `p3d.json` (prérequis chunks P3D-*) |
| TOOL-008 | ✅ | — | Doc format `.con` (cf. `SCRIPT_FORMAT.md`) |
| TOOL-007 | ⬜ | P3 | Tests unitaires P3D (parse→reserialize→compare) |
| TOOL-010 | ⬜ | P3 | CI Linux+Windows + clang-format |

---

## C. Épics éclatés

### C.1 — GAME-001 : refactor `Game` (1499 l., 23 membres, 18 systèmes)

> Pas un God object catastrophique, mais coupling élevé. Découpe en passes **non bloquantes**
> (chacune compile/tourne seule, à faire dans l'ordre).

| Sous-ticket | Sujet | Critère d'acceptation |
|---|---|---|
| GAME-001a | ✅ | Extraire `GameInput` (lecture clavier/souris/manette → struct d'intentions) | `Game::Run` n'appelle plus SDL directement ; jeu jouable à l'identique |
| GAME-001b | Extraire `GameCamera` (logique orbite/follow/freecam, ~150 l. de `Game.cpp:403-554`) | aucune var `_cam*` dans `Game` ; caméra identique |
| GAME-001c | Extraire `GameHud`/`GameUiRenderer` (rendu HUD + menus + texte d'aide) | tout le draw 2D hors `Game::Run` |
| GAME-001d | Extraire `GameStateMachine` (splash/menu/ingame/pause/result) | transitions pilotées par un enum + table, pas par `if` épars |
| GAME-001e | `Game` devient orchestrateur mince (< 400 l.) tenant les sous-systèmes | build vert, run identique, diff comportement nul |

### C.2 — SCRIPT : 215/247 commandes `.con` encore en stub vide `{}`

> **Le plus gros poste restant.** Le moteur (`ScriptEngine`) marche ; audit 2026-08-22 :
> sur 247 commandes définies dans `GameCommands.cpp`, **29 portent de la vraie logique**
> (3 de plus ne font que logger). Le chiffre « 138/203, ~65 marchent » des anciennes docs
> était optimiste.
> On éclate par **catégorie fonctionnelle** : chaque lot = un .con représentatif rejoué sans
> warning « unimplemented command » pour cette catégorie. Cibles : `Scripting/GameCommands.cpp`.

| Sous-ticket | Catégorie | Vol. approx. | Critère d'acceptation |
|---|---|---|---|
| SCRIPT-A | Combler les trous du **P0 mission** (les 18 commandes course de base) | qq-unes | `Missions/level01/M1race.con` tourne 0 stub |
| SCRIPT-B | **Tuning véhicule** (`SetMass`, `SetTopSpeedKmh`, `SetTireGrip`, suspension…) | ~30 | un .con véhicule applique des params mesurables |
| SCRIPT-C | **Caméra** (`SetAnimatedCameraName`, `SetConversationCam`, `SetCarStartCamera`…) | ~10 | caméra scriptée change effectivement de vue |
| SCRIPT-D | **Course** (`SetRaceLaps`, `SetVehicleAIParams`, catchup…) | ~10 | dépend AI-RACE pour effet visible |
| SCRIPT-E | **Gags / collectibles** (`GagBegin/SetPosition/SetTrigger/End`…) | ~51 | dépend GAME-010 |
| SCRIPT-F | **Hit & Run / police** (`EnableHitAndRun`, decay, spawn rate) | ~8 | branché sur ChaseManager (✅) |
| SCRIPT-G | **Traffic / piétons** (`CreateTrafficGroup`, `CreatePedGroup`, `SetMaxTraffic`) | ~6 | branché sur Traffic/Ped managers (✅) |
| SCRIPT-H | **UI / HUD** (`SetPresentationBitmap`, `ShowHUD`, `SetIrisWipe`, `SetFadeOut`) | ~8 | dépend UI-HUD / UI-MENU |
| SCRIPT-I | **Achats / dialogue / FMV / bonus** | reste | P3 |

### C.3 — RENDER-008 : LOD + culling

| Sous-ticket | Sujet | Critère |
|---|---|---|
| RENDER-008a | Frustum culling par AABB de drawable | objets hors-champ non dessinés (compteur draw calls baisse) |
| RENDER-008b | Octree/kd depuis chunks `Tree` (dépend P3D-007) | requêtes spatiales O(log n) |
| RENDER-008c | LOD : swap modèle selon distance | LOD lointain visible au recul |

### C.4 — RENDER-009 : post-process

| Sous-ticket | Sujet | Pri | Critère |
|---|---|---|---|
| FX-000 | ✅ Câbler le pipeline FBO (scène→`_sceneFBO`→quad plein écran) | P2 | prérequis ; image identique en passthrough |
| FX-002 | ✅ Vignette | P3 | bords assombris réglables |
| FX-005 | Color grading LUT 16³ | P3 | contraste/saturation ajustés |
| FX-001 | Bloom (blur gaussien 2 passes) | P3 | halo sur sources vives |
| FX-004 | Shadow mapping (depth FBO directionnel) = ex-RENDER-012 | P3 | ombres portées du soleil |
| FX-003 | Motion blur (reprojection viewProj précédent) | P3 | flou au mouvement rapide |

---

## D. Chemins d'attaque conseillés

0. ~~Débloquer les P0~~ — fait (REGR-001, CHARCTRL-FALL, UI-SPLASH). Tout ce qui suit se valide désormais à l'écran.
1. **Jouabilité visible** : ~~UI-MENU~~ ✅ → ~~UI-HUD~~ ✅ → **GAME-010** (collectibles : cartes/gags, l'état persistant) → SCRIPT-E/H. Donne une boucle de jeu lisible.
2. **Fidélité monde** : LEVEL-STREAM → LEVEL-TRIG → PHYS-011 → PHYS-004. Le monde réagit.
3. **Contenu missions** : SCRIPT-A → SCRIPT-B → AI-RACE → SCRIPT-D. Les vraies missions tournent.
   Débloqué de bout en bout : la course tourne (MISSION-LOOP), le véhicule est créé et
   embarquable (SCRIPT-PARSE), il roule (VEH-SINK) et les checkpoints sont repérables
   (UI-RACE-GUIDE). Reste à confirmer qu'un franchissement incrémente bien `CP n/6`, puis
   **AI-RACE** / **SCRIPT-D** pour l'adversaire.
4. **Dette/qualité** (parallélisable) : GAME-001a→e, P3D-013, AUDIO-009.

---

## E. Crosswalk anciens IDs → canonique

Les anciens fichiers utilisaient des IDs en collision. Correspondance pour retrouver un ticket cité ailleurs :

| Ancien (génération) | Canonique |
|---|---|
| RENDER-001 (G3 « Lightmap ») | RENDER-015 |
| RENDER-001 (G1 « SkinAnimation ») | ✅ clos (§A) |
| RENDER-002/003 (G3 culling/LOD) | RENDER-008 (§C.3) |
| RENDER-002 (G1 DrawBox) | ✅ clos |
| UI-001 (G2 « mouse click ») / UI-003 (G1) | UI-MENU |
| UI-001 (G3 « page nav ») / UI-002 (G1) | GAME-003 + UI-MENU |
| UI-003/004 (G2 états/text) / UI-004 (G1) | UI-TEXT |
| UI-004 (G2 HUD) / UI-005 (G1) | UI-HUD |
| AUDIO-001 (G2/G3 streaming) / AUDIO-002 (G1) | AUDIO-002 |
| AUDIO-002 (G3 subtitles) / AUDIO-007 (G1) | AUDIO-007 |
| AUDIO-004 (G2 hash) / AUDIO-001 (G1 pool) | ✅ clos |
| PHYS-003 (G3 rigid bodies) | PHYS-011 |
| PHYS-004 (G3 callbacks) | PHYS-004 |
| PHYS-005 (G3 friction) / PHYS-008 (G1) | PHYS-008 |
| CHARCTRL-004 (G3 stepUp) | ✅ clos |
| VEH-005 (G3) / GAME-012 (G1) | VEH-DAMAGE |
| LEVEL-003 (G3) | LEVEL-STREAM |
| LEVEL-004 (G3) | LEVEL-TRIG (parse ✅, callbacks TODO) |
| LEVEL-005 (G3) | LEVEL-FOG |
| AI-002 (G3 path) | AI-PATH |
| AI-005 (G3 race) | AI-RACE |
| AI-003/004 (G3), POLICE-* (G2) | ✅ clos |
| SCRIPT-004..010 (G1 par catégorie) | SCRIPT-A..I (§C.2) |
| ASSET-001..006 (G2) | ASSET-001..006 (inchangé) |
| FX-001..005 (G2) | FX-* (§C.4) |
| GAME-001 (G1 refactor) | GAME-001a..e (§C.1) |
</content>
</invoke>
