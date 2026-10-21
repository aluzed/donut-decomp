# CHAR-FEET — Characters drawn floating

- **Status:** DONE (2026-10-21)
- **Priority:** P1 (reported in play: "je vois souvent les personnages flotter")
- **Module:** Character / AI
- **Files:** `src/Character.{h,cpp}`, `src/CharacterController.{h,cpp}`, `src/Game.cpp`
  (`SetPlayerPosition`, `seatDriver`, `scriptedCamera`), `src/AI/PedestrianManager.cpp`

## Context
Characters often stood in the air. Looked at from above and without shadows, a character
0.85m up reads as one further away, and the screenshots taken so far were taken from above:
it went unseen until a camera was put at ground level beside the player (the shoes stood
above the horizon of a camera 0.4m off the ground).

## Causes
1. **Drawn at the capsule's middle.** A character's position is its controller's capsule's
   middle (radius 0.4m, cylinder 0.95m: 0.875m over its bottom), and `Character::Draw`
   drew the model there. The models have their feet at their origin (the lowest vertex of
   Homer's, Bart's, an old lady's mesh: y 0.00): every character stood 0.875m up.
2. **Some placed by their feet, some by their middle.** `SetPosition` was given ground
   spots (a pedestrian's, a locator), and each physics step then moved the character to
   its capsule's middle (`FollowController`). A pedestrian's body was drawn at its feet
   one frame and 0.875m higher the next, depending on which came last.
3. **Pedestrians stood on overhangs.** Each step looked for the ground from 4m above the
   pedestrian, and the first thing a ray down from there met could be an awning, a tree, a
   car's roof: people walked about up to 3.5m in the air.

## Fix
- A character's position is its capsule's middle, always; it is drawn `CentreHeight()`
  lower, at its feet.
- `SetPosition(feet)` stands the character with its feet on a spot (the managers', the
  locators'); `SetCentre(centre)` places the capsule's middle (the player's teleports,
  which give a spot a metre over the ground). The drivers seated in cars are placed by
  their model's origin, raised accordingly; the conversation shots' head height is from
  the middle (0.6m above it).
- Pedestrians look for the ground at most 0.7m above their feet (a kerb, a step), and
  spawn on the ground within 1.5m of the player's height.

## Verification
Measured over `val.sh 240` (7 levels, about 13,000 samples a run): the lowest joint of each
character's animated pose against the static ground under it.

| | before | after |
|---|---|---|
| samples more than 0.25m off the ground | about 12,000 (all, at 0.75-0.9m) | 235 |

The 235 left: characters standing on things that are not static ground (benches, cars,
other people's capsules), interiors (whose floors the probe did not see), and Hibbert,
whose lowest joint in Apu's borrowed animations is a coat button. A ground-level
screenshot shows Homer and a passer-by on the pavement.
