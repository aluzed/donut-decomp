# AUDIO-007 — Sous-titres de dialogue + lip sync

- **Status:** TODO
- **Priority:** P2
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/AudioManager.h:32-48,93,102` (Dialogue, QueueDialogue, processDialogueQueue), bibles frontend

## Contexte
La file de dialogue prioritaire est déjà fonctionnelle (`Dialogue`, `QueueDialogue`, `processDialogueQueue`, `PlayDialogue`). En revanche, aucun sous-titre n'est affiché : le texte associé à chaque ligne de dialogue n'est pas extrait ni rendu de façon synchronisée.

## Approche
- Étendre `struct Dialogue` (`AudioManager.h:32-36`) avec le texte et une durée d'affichage.
- Charger une table nom -> texte sous-titre extraite des bibles frontend (parser le format des bibles de dialogue) au chargement audio.
- Dans `processDialogueQueue`/`PlayDialogue`, quand une ligne démarre, publier le sous-titre actif (texte + horodatage de fin) ; l'effacer dans `Update` quand sa durée est écoulée.
- Exposer le sous-titre courant pour que la couche HUD/rendu l'affiche ; optionnellement, piloter un paramètre de lip sync (amplitude) à partir du RMS du signal en cours de lecture.

## Critères d'acceptation
- [ ] Le texte du dialogue en cours est extrait des bibles frontend et exposé par `AudioManager`.
- [ ] Le sous-titre apparaît au début de la ligne et disparaît à la fin de sa durée.
- [ ] La priorité de file existante reste respectée (un dialogue Cutscene coupe toujours un Gameplay).
