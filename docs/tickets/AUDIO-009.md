# AUDIO-009 — Son de klaxon

- **Status:** DONE
- **Priority:** P3
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Game.cpp:508`, `src/Audio/AudioManager.h:43-45,52` (PlayAudio, PlayRaw), `src/Audio/SoundGenerator.h`

## Contexte
À l'appui sur la touche klaxon, le jeu se contente de logger « HONK! » (`Game.cpp:508`) sans jouer le moindre son.

## Approche
- Dans `Game.cpp:508` (`Input::JustPressed(GameAction::Honk)`), remplacer le log par la lecture d'un son de klaxon.
- Si un échantillon de klaxon existe dans les RCF chargés, le jouer via `AudioManager::PlayAudio(name)` (`AudioManager.h:43`) ; sinon synthétiser un klaxon via `SoundGenerator` et le jouer avec `PlayRaw` (`AudioManager.h:52`), comme déjà fait pour les bips/chirps de `ScriptEngine`.
- Jouer le son en 3D à la position du véhicule (`PlayAudio(name, position)`, `AudioManager.h:45`) ; éviter le spam (anti-répétition pendant la durée du son).

## Critères d'acceptation
- [x] Appuyer sur la touche klaxon joue réellement un son (échantillon ou synthétisé).
- [x] Le son n'est plus un simple `Log::Info("HONK!")`.
- [x] Maintenir/spammer la touche ne sature pas le pool de sources audio.
