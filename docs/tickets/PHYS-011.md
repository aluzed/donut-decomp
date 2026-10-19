# PHYS-011 — Props a car knocks about

- **Status:** MOSTLY DONE (2026-10-20)
- **Priority:** P1
- **Module:** Physics
- **Depends on:** —
- **Files:** `src/Level.cpp` (`DynamicPhysics`, `updateProps`, `loadCollisionAttributes`),
  `src/Physics/WorldPhysics.*` (`AddPropBody`, `ReleaseProp`, `GetContacts`),
  `src/Render/Mesh.*` (`MeshInstanced::SetTransform`), `src/Audio/AudioManager.*`
  (`ScriptSoundFile`), `src/Game.cpp`

## What was wrong

The level's `DynamicPhysics` objects (0x3F00002) were drawn and nothing else: 269 on
level 1 alone — mailboxes, bins, lamp posts, traffic lights, stop signs, hydrants,
trees — and the car drove through every one.

## What is done

- **Their collision**: each object carries a `CollisionObject` (0x7010000) in its own
  space. Each instance gets one rigid body of its leaf volumes (boxes, spheres,
  cylinders) as a compound, centred on them, placed by the instance list.
- **What each does** is its `CollisionEffect` (0x3000600: class, attribute row, sound;
  read by hand):
  - class 3 (mailboxes, bins, a basketball hoop, gravestones): dynamic from the start,
    asleep until something touches them;
  - class 10 (lamp posts, traffic lights, parking lights): static until something runs
    into them hard enough (the contacts' impulse over three times their mass, 300 N·s at
    least), then dynamic. The car, stopped dead by the static body in that step, gets
    back the speed it had less what it gave the prop, which leaves with it;
  - class 4 with a breakable (stop signs, hydrants, the Krusty glass, the tomacco, the
    pet graves; trees): hit as hard, they play their **breaking** (below). A stop sign, a
    hydrant go with it (out of the world, their instance drawn at zero scale, the car
    given its speed back); a tree stands, and sheds leaves again five seconds later;
  - everything else: static.
- **Breakings** (`BreakableObject`, 0x3001000: the game's breakable type, how many may
  play at once, then an animation, skeleton, drawable, meshes and particle systems): a
  pool of hidden models per type, from the first file that carries it (every region
  file has its own copy). One plays at the prop, its particles keyed to its animation
  as an animated object's breaking is, and hides once played. The type each attribute
  row names (`eHydrantBreaking`, `eStopsign`, `eOakTreeBreaking`) was read off what each
  type's breaking shows: 3 a hydrant's, 8 the tomacco's, 14 a pine's, 15 an oak's, 16 a
  barrier's, 17 a rail crossing's, 19 the Krusty glass's, 20 a cypress's, 21 a dead
  tree's, 22 a skeleton's, 23 a willow's, 25 a globe light's, 26 the morning tree's,
  27 and 28 the palms', 29 a stop sign's, 30 to 32 the pumpkins'. 18 (a glass
  shattering), 24 (a car's explosion) and 33 are not named by the table. Its pieces go
  one by one as its visibility animation says (`PVIS`: per prop, a bool channel's start
  state and toggles; an oak's trunk pieces gone by frame 30).
- **Their mass** is their row of the **collision attribute table** (`art/atc/atc.p3d`,
  `Atc` 0x3000602: a count, then per row the sound, the particle effect and the
  breakable object as strings, and friction, mass, elasticity): a lamp post 1000 kg, a
  mailbox 100 to 125, a stop sign 50.
- **Drawn where their body is**: an instanced mesh's instance can be moved
  (`MeshInstanced::SetTransform`, one matrix written to the instance buffer).
- **Their hit particles**: the attribute row's particle effect, from the level's effects
  file (`InstParticleSystem`, 0x3001001: the game's particle type, how many may play,
  then the sprites' own textures and shaders, factories and the system naming one):
  type 3 a hedge's leaves (`eShrub`), 4 a bin's rubbish (`eGarbage`: apples, bones,
  banana skins), 5 an oak's leaves (`eOakTreeLeaves`), 6 a mailbox's letters (`eMail`).
  Given off at the prop's foot, where a car meets it, as long as its timeline says, then
  left to die out. A row naming `eSpark` (lamp posts, the basketball hoop, the washer)
  gets the game's own spark burst, the one off a car's impacts.
- **Heard**: the effect's sound names a resource of `sound\scripts\collide.spt`
  (`car_hit_mailbox`: `mailbox_hit_0318a.rsd`, ...), played where the prop is when it
  is knocked loose or sent flying (once a tumble).
- One fallen 50 m below where it stood is taken out of the world.
- **Kicked**: on foot, the strike (or a jump's kick) sends those that move -- a mailbox,
  a bin, a lamp post already down -- off at some 5 m/s, a little up and turning over;
  one within reach makes the attack button strike rather than jump, as a crate does.

## What it took besides

- **Particle gravity** (`GRA`) is in m/s², up: debris fall (a crate's bits at -2, glass at
  -6, letters at -1.5), flames and steam rise (3). It was taken per frame per frame and
  down, so every falling piece shot up at hundreds of g and was never seen: the vending
  machine's cans, the Krusty glass, the crates' bits. The game's own bursts (sparks,
  dust) give theirs the same way now.
- **The emission rate over a step**: the timeline's rate (`RAT`) is averaged over the
  stretch a step covers, not read where it ends; the letters come in a burst five frames
  long, which one slow frame stepped right over.
- **Ray queries about the level** (`staticOnly`) leave the props out, standing or not: a
  traffic van found a prop's top for the ground.

## Checked

- A car at 50 km/h into a mailbox: it is pushed, tips and flies 19 m off.
- Into a lamp post: it leans, falls and lies on the pavement; the car goes on at about
  20 km/h. Its sound plays (`streetlamp_hit_0x.rsd`).
- Into a stop sign: it goes, its breaking plays (the sign flies up turning and lands on
  the road), the car goes on at 34 km/h (`stopsign_hit_0x.rsd`).
- A hydrant's breaking: a geyser; an oak's: its trunk pieces fly and go, its leaves rise
  and fall.
- A mailbox's letters burst out of it and flutter up; a bin's rubbish flies.
- Homer kicks a mailbox: it tumbles off, its letters fly, its sound plays.
- The race opponents knock some over too. The seven stories under `--autopilot`, 240 s
  each: as many missions won as before, no failure, no crash.

## Not done

- The particle types the table names but the effects file does not hold by a name we
  know (`eStars`, `eParkingMeter`, ...): types 9, 10, 11, 14, 19 are dust,
  dirt, grass, a car's damage smoke, fire; which names they answer to is not known.
- The `AnimDynamicPhysics` objects (crates, vending machines) still have no body: they
  break when the car comes near (`Level::BreakObjectsNear`).
- The player on foot does not push them by walking into them (the character is a
  kinematic capsule); a kick does (below).
- Traffic (kinematic) passes through the fixed ones, as before.
- `l1z7_washer` stands at the world's origin (its placement is identity); that was so
  before.
