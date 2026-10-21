# AUDIO-008 — EFX reverb by place (indoors / outdoors)

- **Status:** DONE (2026-10-21)
- **Priority:** P3
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/Reverb.{h,cpp}`, `src/Audio/AudioManager.{h,cpp}`, `src/Level.{h,cpp}`
  (`Trigger::event`), `src/Game.cpp` (`placeEvent`), `src/GameLevel.cpp` (interiors),
  `tools/ghidra/DisasmRange.java`

## Context
The OpenAL EFX headers were included but no reverb was set up: a tunnel sounded like the
open street. The game shipped `eax.dll` and used EAX reverbs.

## What the game does
- **Its reverbs are in its sound scripts** (scripts.rcf): 12 `create reverbSettings named
  <name>` blocks, each with its gain, fade in and out times (ms), and one set of parameters
  a platform: the Xbox's are I3DL2 (room, room HF, rolloff, decay time and HF ratio,
  reflections and their delay, late reverb and its delay, diffusion, density, HF reference),
  the PC's adds the air absorption.
- **The places are event locators** (type 0) in the levels' terrain files, their event
  the first word of their data: `burns_tunnel_in` 40, `PP_room_1_in` 41,
  `stonecutters_tunnel_in` 37...
- **Which event sets which reverb** is the reverb controller's event handler, at `0x4ce140`
  in `Simpsons.exe` (code Ghidra left undisassembled, reached only through its vtables
  `0x60a050` and `0x609b68`; `tools/ghidra/DisasmRange.java` reads it): a switch on the
  event less 20, through a byte table at `0x4ce278` and a jump table at `0x4ce244`:

  | Event | Reverb |
  |---|---|
  | 20 | stadiumtunnel |
  | 34 | krustylu |
  | 37 | stonecuttertunnel |
  | 38 | stonecutterhall |
  | 39 | sewers |
  | 40 | burns_tunnel |
  | 41 | pproom1 |
  | 42 | pproom2 |
  | 43 | pproom3 |
  | 44 | PP_tunnel_01 |
  | 45 | PP_tunnel_02 |
  | 46 | mansion_interior |

  Any other event it listens to takes the reverb off. Its constructor (`0x4ce020`) listens
  to the places' events, 7 to 46, 50 to 62 and 67 to 76, and to 138 and 157 (going into an
  interior and out).

## Done
- `Reverb` (`src/Audio/Reverb.*`): the EFX functions looked up at run time, one EAX reverb
  effect in one auxiliary slot; the game's settings read from its scripts, the I3DL2
  millibels turned to gains; `Set(name)` fades the one there out and the next in, as their
  fade times say. Without the EFX extension (or the EAX reverb), nothing happens, and no
  sound changes.
- Heard through it: the sounds placed in the world, the player's engine and the car's loops
  (tyres). Dry: the music, the voices, the ambience, the interface's sounds.
- A level's event locators keep their event (`Level::Trigger::event`); stepping into one of
  the places the game listens to sets the reverb from the table above, or takes it off.
  Going into a building or out of it takes it off.
- Checked by putting the player in `burns_tunnel_in`, then `PP_room_1_in`: "reverb
  'burns_tunnel'", then "'pproom1'".

## Acceptance criteria
- [x] An EFX reverb is created and attached to an auxiliary effect slot at start-up.
- [x] Going in and out of the game's reverb places changes the reverb of the sounds placed
      in the world.
- [x] Without EFX: no reverb, no crash; the EFX objects are freed at shutdown.
