# VEH-DAMAGE — A damaged car looks damaged

- **Status:** DONE (2026-10-18)
- **Priority:** P3
- **Module:** Vehicle
- **Depends on:** PHYS-004 (impact damage)
- **Files:** `src/Game.cpp` (`updateCarDamageLook`), `src/ResourceManager.cpp` (`ShowTextureAs`),
  `src/Vehicle.h` (`GetModelName`)

## Context

The ticket first assumed a separate `car_damage.p3d` model to swap in. There is none: the
game's cars carry their dents as **textures**. 53 of the 89 car files hold, for up to four
parts, a pair `<model><part>Norm.bmp` / `<model><part>Dam.bmp` (`famil_vHoodNorm.bmp` and
`famil_vHoodDam.bmp`; parts `Hood`, `DoorD`, `DoorP`, `Back`). The other cars (the ATV, the
coffin, the ambulance...) have no damaged look.

## What is done

- `ResourceManager::ShowTextureAs(texture, shownAs)` makes every shader whose own texture is
  `texture` draw `shownAs` instead; showing a texture as itself puts it back.
- `Game::updateCarDamageLook` follows the health of the car the player drives (the one
  `updateCarDamage` takes down on impacts): the hood gives way below 80, both doors below
  60, the back below 35. A car the player leaves keeps its dents; a car they swap for is
  mended to the eye, and a car mended (a wrench, a phone booth, a retry) loses them.
- Shaders are shared by name, and a mesh asking for its shader the first time resets its
  texture, so a damaged look is put again every 2 seconds.

Checked: level 1 m0, the family sedan at 20 health, seen from the chase camera: the boot is
crumpled and the bumper torn, where the same frame at full health shows it smooth.

## Not done

- Two cars of the same model share their look. The traffic is made of generic models
  (`pickupA`, `compactA`...), none a player's car, so only a mission car of the player's
  own model would show the player's dents.
- The game also breaks parts off (doors, bumpers); nothing does here.
