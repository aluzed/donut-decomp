# Tickets Audio

## AUDIO-001 — Gestion multi-sources
**Priorité :** P1
**Fichier :** `src/Audio/AudioManager.h/.cpp`
**Description :** Une seule source OpenAL (`_source`) et un seul buffer. Impossible de jouer plusieurs sons simultanément (moteur + musique + dialogue).
**Correctif :** Pool de sources (par ex. 32). Gérer l'allocation/libération dynamique.

---

## AUDIO-002 — Streaming audio (musique)
**Priorité :** P1
**Description :** Les fichiers musique RSD sont trop gros pour être chargés en mémoire entièrement. Implémenter le streaming par buffer circulaire (double buffering).

---

## AUDIO-003 — Audio spatial / 3D
**Priorité :** P2
**Description :** Positionner les sources dans l'espace 3D. Les sons de voiture, piétons, dialogues doivent être spatialisés. OpenAL le supporte nativement (`alSource3f` avec position).

---

## AUDIO-004 — Implémenter PlayAudio avec hash
**Priorité :** P1
**Fichier :** `src/Audio/AudioManager.cpp`
**Description :** La surcharge `PlayAudio(uint32_t hash)` est déclarée mais absente du .cpp. Les scripts de mission référencent les sons par hash.

---

## AUDIO-005 — Populer le cache `_sounds`
**Priorité :** P2
**Fichier :** `src/Audio/AudioManager.cpp`
**Description :** `LoadRCF` ne remplit pas la map `_sounds`. À chaque PlayAudio, on recherche linéairement dans tous les RCF. Ajouter un index au chargement.

---

## AUDIO-006 — Chargement automatique des RCF
**Priorité :** P3
**Fichier :** `src/Audio/AudioManager.cpp` (constructeur)
**Description :** La liste des RCF est hardcodée (10 noms). Scanner le dossier `audio/` automatiquement.

---

## AUDIO-007 — Gestion des dialogues
**Priorité :** P2
**Description :** Le fichier `DIALOGF.RCF` (159MB) contient tous les dialogues du jeu. Implémenter :
- Lip sync (basé sur les durées des samples ?)
- Sous-titres (depuis les bibles de texte, P3D-005)
- Priorité des dialogues (cutscene > gameplay)

---

## AUDIO-008 — Support EFX (réverbération)
**Priorité :** P3
**Description :** Réverbération par zone pour les intérieurs vs extérieur. OpenAL EFX.
