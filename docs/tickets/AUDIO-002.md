# AUDIO-002 — Finir le streaming musique double-buffer

- **Status:** TODO
- **Priority:** P1
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/AudioManager.h:71-79,103`, `src/Audio/AudioManager.cpp` (PlayMusic)

## Contexte
La structure `MusicSource` (source OpenAL + 2 buffers de streaming) est déclarée (`AudioManager.h:71-79`) et les constantes `kStreamingBuffers=2` / `kStreamingBufferSize=65536` existent, mais `PlayMusic` est incomplet : la musique n'est pas streamée en continu via double-buffer.

## Approche
- Dans `PlayMusic` (déclarée `AudioManager.h:53`), allouer `_music` (`MusicSource`), générer la source et les `kStreamingBuffers` buffers OpenAL.
- Charger le flux décodé du morceau et remplir/queue les 2 buffers initiaux par chunks de `kStreamingBufferSize`, puis `alSourcePlay`.
- Dans `AudioManager::Update`, sonder `AL_BUFFERS_PROCESSED` : pour chaque buffer terminé, `alSourceUnqueueBuffers`, le re-remplir avec le chunk suivant, `alSourceQueueBuffers` ; relancer la lecture si la source a calé (underrun). Boucler en fin de morceau.
- Utiliser `getFormat(channels, bits)` (`AudioManager.h:84`) et nettoyer la source/les buffers dans `shutdownOpenAL`.

## Critères d'acceptation
- [ ] `PlayMusic` joue un morceau du début à la fin sans coupure ni underrun audible.
- [ ] Le streaming recycle réellement les 2 buffers dans `Update` (vérifiable via `AL_BUFFERS_PROCESSED`).
- [ ] Les ressources OpenAL de `MusicSource` sont libérées proprement à l'arrêt.
