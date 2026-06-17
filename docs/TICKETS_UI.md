# Tickets UI

## UI-001 — Frontend : charger toutes les pages (pas seulement "MessageBox.pag")
**Priorité :** P1
**Fichier :** `src/FrontendProject.cpp:82`
**Description :** Le `LoadP3D` ne charge que les pages nommées "MessageBox.pag". Le menu principal, pause, options ont d'autres noms de page.
**Correctif :** Charger toutes les pages (ou au moins par nom demandé).

---

## UI-002 — Frontend : implémenter le système de screens
**Priorité :** P1
**Description :** Le jeu original a un système de screens (FrontendScreen → FrontendPage → FrontendLayer → FrontendGroup). Implémenter la navigation entre screens :
- Bootscreen → Main Menu → Level Select → Pause → Options → Credits

---

## UI-003 — Frontend : interaction utilisateur
**Priorité :** P1
**Description :** Les FrontendObject/FrontendGroup ont des zones cliquables. Implémenter la détection de clic/survol et la navigation :
- Mouse picking sur les sprites
- Changement d'état (hover, pressed, disabled)
- Callbacks de navigation

---

## UI-004 — Rendu de texte frontend (MultiText)
**Priorité :** P1
**Fichier :** `src/FrontendProject.cpp`
**Description :** Les `FrontendMultiText` sont lus mais non rendus. Implémenter le rendu de texte avec `TextureFont` et `FrontendStringTextBible` pour la localisation.

---

## UI-005 — HUD in-game
**Priorité :** P1
**Description :** Éléments du HUD :
- [ ] Barre de vie (hit points)
- [ ] Jauge de Hit & Run (poursuite police)
- [ ] Compteur de vitesse
- [ ] Compteur de monnaie (coins)
- [ ] Compteur de cartes à collectionner
- [ ] Timer de mission
- [ ] Objectifs actuels
- [ ] Radar / mini-map
- [ ] Indicateur de nitro

---

## UI-006 — Menu pause
**Priorité :** P2
**Description :** Overlay de pause avec options (reprendre, recommencer, quitter, options audio/vidéo).

---

## UI-007 — Textes localisés (bible system)
**Priorité :** P2
**Description :** Le jeu supporte plusieurs langues via des bibles de texte. Charger la bible de la langue configurée (FrontendLanguage, FrontendTextBible).

---

## UI-008 — Écran de chargement
**Priorité :** P3
**Description :** Écran avec barre de progression entre niveaux/zones. Afficher le texte de hint (tips).

---

## UI-009 — Cinématiques (RMV player)
**Priorité :** P3
**Description :** Les vidéos du jeu sont au format RMV (Radical Movie). Reverser ou trouver un codec existant pour lire les FMV.
