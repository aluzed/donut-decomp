# AUDIO-010 — Footsteps and tyre squeal, by the ground

- **Status:** DONE (2026-10-20)
- **Priority:** P2
- **Module:** Audio
- **Depends on:** PHYS-008 (the ground's surface)
- **Files:** `src/Game.cpp` (`updateFootsteps`, `updateWheelSprays`),
  `src/Audio/AudioManager.*` (`SetLoop`)

## What was wrong

The player walked and ran without a sound, and a car's tyres skidded silently.

## What is done

- **The game's own clips**, named by its sound settings: `SetFootstepRoadClipName
  "feet_concrete"`, `SetFootstepMetalClipName "feet_metal"`, `SetFootstepWoodClipName
  "feet_wood"`, `SetSkidRoadClipName "skid"`, `SetSkidDirtClipName "dirt_skid"`. Each
  playable character's sound script (`homer.spt`, `bart.spt`, `lisa.spt`, `marge.spt`,
  `apu.spt`) has its own `feet_concrete`, `feet_metal`, `feet_wood`, `feet_dirt` and
  their `_jump` landings: Homer's boots, Lisa's sneakers.
- **Footsteps**: one a stride (1.1 m along the ground, which the autopilot's walking
  covers too), heard at the feet, with the clip for the ground underfoot: concrete on
  the road and pavement, metal, wood, dirt on grass, dirt, sand and gravel. A landing
  plays the ground's `_jump` clip. A costume's model ("h_fat") finds its owner's script
  by its first letter.
- **Tyre squeal**: while the player's car leaves skid marks (PHYS-008), the `skid` clip
  loops, louder as it skids harder; `dirt_skid` when none of the skidding wheels is on
  the road. A looped sound by name (`AudioManager::SetLoop`).

- **As the script plays them**: a resource's `SetTrim` is its volume (Homer's steps on
  dirt 0.47, on metal 0.42) and `SetPitchRange` the range its pitch is picked in, once a
  play (0.9 to 1.1 for a landing): footsteps, the squeal and the props knocked over
  (`collide.spt`) all take them (`AudioManager::FindScriptSound`). A pooled source is
  put back to full volume and pitch before each sound, as the last one's stayed on it.

- **Peel out**: a car set off flat out spins its rear tyres, the more the slower it goes
  (all of it at a standstill, nothing by 20 km/h): they mark the road and, the player's,
  squeal, at most at the settings' `SetPeeloutMaxTrim` (0.8), from `SetPeeloutMin` (0.1).

## Checked

- Homer made to run across his lawn: a step every 1.15 m, `boots_dirt_01..04.rsd`;
  Lisa landing on level 3: `sneakers_conc_jump_01.rsd`.
- Homer's car floored from a standstill: the squeal plays, and a light mark is left
  behind it.
- Homer's car braking hard on the pavement: `skid.rsd`; on the lawn: `dirt_skid.rsd`.
- The seven stories under `--autopilot`, 240 s each: no failure, no crash.

## Not done

- Not timed to the animation's foot falls: a stride is a distance.
- The traffic's tyres. (The passers-by's footsteps came after, 2026-10-22: a step every
  0.75 m walked, heard within 15 m, softly, at most one in 0.15 s.)
