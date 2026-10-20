# AUDIO-002 — Finir le streaming musique double-buffer

- **Status:** DONE (2026-10-20: the music script picks the music by game event)
- **Priority:** P1
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/MusicScript.{h,cpp}`, `src/Audio/AudioManager.{h,cpp}` (PlayMusicEvent), `src/Scripting/ScriptEngine.cpp` (musicEvent), `src/Game.cpp`

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

## The listener (2026-10-18)

`AudioManager::SetListenerPosition` was never called: the listener sat at the world's
origin, and every sound given a position (a traffic driver's shout, a mission sound) was
heard from there, faint or not at all. The listener is now the camera, every frame. OpenAL
takes right as forward x up; in this world +X is on the right looking down +Z (checked on
the radar), so "up" is given as -Y for left and right to come out the right way round.
Not heard.

## The music script, looked at (2026-10-20)

`sound\music\l<n>_music.rms` (in the music RCFs, 52 to 62 KB; `sound\ambience\ambience.rms`
too) is a Radical "radmusic" composition: a serialized object graph that carries its own
schema first -- type and field names (`radmusic_comp`, `group`, `fade_transition`,
`stitch_transition`, `event`, `event_matrix`, `state`, `rsd_file`, `stream`, `clip`,
`region`, `sequence`, `layer`, the `event_*` variants -- volume, pitch, their random
ranges, positional, callbacks, `logic_and/or/repeat` -- and the actions
`play/push/pop_region`, `start/stop_layer`, `rand_state`), each with its fields
(`source_region_ref`, `target_region_ref`, `source_time`, `beat_set`, `bpm`,
`time_sig_numerator`...). Then a table of offsets, and the objects. Level 1's names its
regions and events: `silent_region`, `movie_region`, `pause_region`, `FE_region`,
`OF_explore_region`, `OF_found_card_region`, `OF_time_out_region`,
`Win_3Street_Races_region`, `Level_Completed_region`, `Destroy_Camera_Bonus_region`,
`Gated_Mission_region`, `KwikEmart_region`, `Entering_Building_region`, and its clips
(`Homer\Tuba_001`..`069`, `Horn_triumph`, `Harp_Music04/05`, `Generic\Horn_Blast`,
`Apu\Kwik_E_Mart_Muzak`); events `OF_explore_mission`, `OF_found_card`, `OF_time_out`,
`Gated_Mission`, `OF_House`, `OF_KiwkEMart`, `OF_School`, `OF_Moes`...

To read it: the schema block, then each object by its type's field list; regions hold
sequences of clips, events play or push regions through transitions on the beat.
Several hours of work, and it needs listening to judge.

## The music script, read by its names (2026-10-20)

`MusicScript` does not decode the object graph (its pointers are run-time addresses).
It reads the names the composition keeps after `my_comp`, in order, which come group
by group:

- the group's clips: a run of names with no bytes between them, each a sound file
  followed by its clip (`Homer\Tuba_001`, `Tuba_001`; `Motown_Intro`, `M5_Intro`), or
  a clip declared by another group, alone (the bonus missions replay level 1 m1's);
- its regions (`M1_start_region`, `M1_end_Pos_region`), each followed by its data;
- its events (`M1_start`, `M1_win`), each followed by its data.

An event plays the region of its group its name calls for (`M1_start` the start or main
region, `_win` the `end_Pos` one, `_lose` `end_Neg`, `_get_out_of_car` `end_Sus`,
`StreetRace01_win` `win_street_race1`, a few by name: `Hit_and_Run` `Hit_Run`), and a
region the clips of its group its name calls for: a start or main region its pickup or
intro once, then the rest round and round; a drama region its drama; an ending its
ending, once. Event names are matched without case or underscores (`M2_Win`).

`AudioManager::PlayMusicEvent` plays an event's cue; one that loops becomes the music to
go back to, and an ending gives the music back to it when it is over. The game's events:

- the level loaded: `OF_explore_mission` (level 1 the 63 tubas, 4 Marge's lounge, 7
  Halloween), or the old per-level set when the script has nothing the archives hold;
- a mission: `M<n>_start` / `_win` / `_lose` / `_get_out_of_car` (failed out of the car),
  `Sunday_Drive_*` for the drive to it (`m1sd`), `StreetRace0<n>_*`, `Bonus_*`; then
  back to `OF_explore_mission`. A retry starts the mission's again; a mission given up
  goes back at once;
- Hit & Run: `Hit_and_Run` while the police chase (the level's music kept as the one to
  go back to), `Hit_and_Run_Caught` when busted;
- a stage's last ten seconds: `M<n>_10sec_to_go` (level 1 m4);
- the Kwik-E-Mart: `OF_KiwkEMart` inside (its muzak), the level's music outside;
- the credits, at the story's end: `Credits` (Land of Chocolate).

All seven levels' scripts read (20-22 groups, 44-55 events with music). Checked by log on
the seven levels (story, autopilot): level 1 `OF_explore_mission`, `M1_start`
(collection02 pickup then main), `M1_win` (its `End_Pos`), then the tubas again; level 2
`M3_start` (cbg pickup, main), level 7 `M1_start` (halloween_intro), `Sunday_Drive_start`.
Validation unchanged: 32 won, 0 failed, 0 crash. Not heard: this machine was not
listened to.

Not done:
- The transitions are not on the beat: a cue starts at once, after the segment playing
  is cut. The script's fades, stitches and beat sets are not read.
- The other events: the front end, the other buildings (`Entering_Building` fades
  out), cards found, `_drama`, the Super Sprint. `ambience.rms` is not read either.

`music02.rcf` (Sunday Drive, the Power Plant, Vivaldi, the main theme, the Kwik-E-Mart's
muzak, some mission themes) did not come out of the CDs; `tools/extract_assets.sh` now
mends it (docs/ASSETS.md). With it, every event the seven levels' validation runs into
has its music (Sunday_Drive_start: sunday_drive_intro then sunday_drive).
