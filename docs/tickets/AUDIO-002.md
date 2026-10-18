# AUDIO-002 — Finir le streaming musique double-buffer

- **Status:** PARTIAL (2026-10-18: the game's music plays)
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

## The game's music (2026-10-18)

The music RCFs (`music00/01/03.rcf`, picked up by the `audio/` scan) hold the game's
score as short RSD segments (7-20s, 24 kHz stereo), and a music script per level
(`l1_music.rms`... binary, not read) that chains them. `AudioManager::PlayMusicPlaylist`
plays a list of segments one after another, round and round: each is decoded whole into
one buffer (no streaming needed for a segment), and `Update` starts the next when one
ends. `PlayMusic` no longer leaks the previous track's source and buffers.

Each level plays its own set in order: Homer's tubas on level 1 (61 segments), Lisa's
on 3, Marge's lounge on 4, Halloween on 7; the levels without a set of their own play
the `*_main` themes. Checked by log: tuba_001 (7.8s), tuba_002, tuba_003 in turn. Not
heard: this machine was not listened to.

Not done: the music script (what plays when, the mission and chase themes, the endings).

## The cars' own engines and horns (2026-10-18)

Each car has a sound script in `scripts.rcf`, `sound\scripts\<car>.spt`, of
`daSoundResourceData` blocks: the engine loop (the first resource not named `*_horn`:
`famil` is `familycar.rsd`, `snake_car` `snake.rsd`, `lisa_v_car` `malibustacy.rsd`) and
the horn (`*_horn`). The car the player drives now plays its engine loop, pitched from
0.7 at rest up with the speed, and stops when the player gets out; the horn key plays the
car's horn, the generated one only for a car without. Checked by log for the family
sedan, the Honor Roller, the Canyonero, Lisa's and Apu's cars; the police car has no
script. Not heard on this machine.

## The place's background (2026-10-18)

`ambience.rcf` holds looped backgrounds by place (suburb by day and night, the busy city,
the Squidport, Halloween, small rooms...), which the game picks by zone with a script
(`ambience.rms`) not read here. Each level now plays one for its outdoors (level 1 the
suburb by day, 2 the busy city, 3 the Squidport, 4 the suburb at night, 5 the city at
night, 6 the Squidport at night, 7 Halloween), and a small room indoors, softly, looped.
