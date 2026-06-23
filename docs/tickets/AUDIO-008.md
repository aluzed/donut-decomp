# AUDIO-008 — Réverbération EFX par zone (intérieur / extérieur)

- **Status:** TODO
- **Priority:** P3
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/AudioManager.h:9-11,81-82,95-96` (en-têtes EFX, initializeOpenAL, _alContext/_alDevice)

## Contexte
Les en-têtes OpenAL EFX sont déjà inclus (`AudioManager.h:9-11`) mais aucun effet de réverbération n'est configuré. Le son est identique en intérieur et en extérieur, ce qui casse l'immersion.

## Approche
- Dans `initializeOpenAL` (`AudioManager.h:81`), créer un auxiliary effect slot et deux effets `AL_EFFECT_REVERB`/`EAXREVERB` (preset intérieur réverbérant, preset extérieur plus sec).
- Ajouter une API `SetReverbZone(zone)` qui bascule l'effet attaché au slot selon la zone courante du joueur.
- Router les sources de jeu vers le slot via `alSource3i(..., AL_AUXILIARY_SEND_FILTER, ...)` ; laisser la musique/dialogue 2D non affectés si souhaité.
- Détruire effets et slot dans `shutdownOpenAL`. Prévoir un repli silencieux si l'extension EFX est absente.

## Critères d'acceptation
- [ ] Un effet de réverbération EFX est créé et attaché à un auxiliary effect slot au démarrage.
- [ ] Le passage intérieur/extérieur change audiblement la réverbération des sons spatialisés.
- [ ] Absence de l'extension EFX : repli sans réverbération, sans crash ; ressources EFX libérées à l'arrêt.
