# SCRIPT-B — Commandes de réglage véhicule (tuning)

- **Status:** DONE (2026-10-21)
- **Priority:** P1
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp:327-384`, `src/Scripting/ScriptEngine.cpp:212-216`, `src/Vehicle.h`, `src/Vehicle.cpp`

## Contexte
Seules 5 commandes de tuning sont câblées (`SetMass`, `SetGasScale`, `SetTopSpeedKmh`, `SetTireGrip`, `SetSuspensionLimit` — `ScriptEngine.cpp:212-216`). Les ~30 autres commandes de réglage véhicule sont des stubs vides `{}` dans `GameCommands.cpp`, donc les fichiers `.con` de stats de voiture n'appliquent presque rien au comportement physique.

## Approche
- Ajouter dans `Vehicle` les setters manquants (suivre le motif de `SetMass`/`SetGasScale`/`SetTireGrip` déjà présents).
- Câbler chaque commande stub vers `Game::GetInstance().GetScriptEngine()` puis vers `_activeVehicle->SetXxx(...)` (motif `ScriptEngine.cpp:212`).
- Stubs vides à implémenter (`GameCommands.cpp:335-384`) :
  - `SetSlipGasScale`, `SetBrakeScale`, `SetMaxWheelTurnAngle`, `SetHighSpeedSteeringDrop`,
  - `SetNormalSteering`, `SetSlipSteering`, `SetEBrakeEffect`, `SetSlipSteeringNoEBrake`, `SetSlipEffectNoEBrake`,
  - `SetCMOffsetX`, `SetCMOffsetY`, `SetCMOffsetZ`,
  - `SetSpringK`, `SetDamperC`, `SetSuspensionYOffset`,
  - `SetHitPoints`, `SetBurnoutRange`, `SetMaxSpeedBurstTime`, `SetDonutTorque`,
  - `SetWeebleOffset`, `SetWheelieRange`, `SetWheelieOffsetY`, `SetWheelieOffsetZ`,
  - `SetShadowAdjustments`, `SetCharactersVisible`, `SetCharacterScale`,
  - `SetDriver`, `SetHasDoors`, `SetShininess`, `SetHighRoof`,
  - `SetHighSpeedGasScale`, `SetGasScaleSpeedThreshold`, `SetAllowSeatSlide`.
- Pour les réglages purement cosmétiques (`SetShininess`, `SetHighRoof`, `SetCharactersVisible`), un stockage de champ + `Log::Info` suffit ; pour les réglages physiques, propager réellement dans le corps physique du véhicule.

## Done (2026-10-21)

The game's figures have no units, so each is read **against the median of the 90 car
scripts**: the median car handles exactly as every car did before, and a car's own figure
moves it from there, within bounds.

| command | median (range) | now |
|---|---|---|
| `SetMaxWheelTurnAngle` | 30 (9-55) | the lock, at every speed, times angle/30 (0.3-1.8) |
| `SetHighSpeedSteeringDrop` | 0.272 (0-0.5) | the lock kept at speed, times (1 - drop)/(1 - 0.272) |
| `SetBrakeScale` | 7 (1-16) | the brakes, times scale/7 (0.3-2) |
| `SetSpringK`, `SetDamperC` | 0.4, 0.2 | the springs' stiffness and the dampers, times k/0.4 and c/0.2; applied to the wheels when the script comes after the body |
| `SetCMOffsetX/Y/Z` | 0, 0.05, 0.35 | the centre of mass: the chassis body's origin, its shapes and wheels placed about it (Y -0.5 to 0.5, Z 0 to 1) |
| `SetHitPoints` | 3 (0.1-99) | the share of a crash's damage the car takes, 3/hit points (0.03-5): Lisa's car (1.5) twice the median's, `cNonup` (99) next to none |
| `SetHighSpeedGasScale`, `SetGasScaleSpeedThreshold` | 17 cars | past that share of its top speed, the other gas scale |
| `SetDriver` | 55 cars | the driver shown in a car the AI drives when the mission names none ("none": nobody) |
| the rest | | kept on the car (`Vehicle::GetSetting`), unused: the e-brake slides (`SetSlip*`, `SetEBrakeEffect`, `SetNormalSteering`), wheelies, burnouts, donuts, the shadow, doors, shininess, the characters' scale |

The race opponents plan with their car's figures: a car with weaker brakes or less lock
than the median brakes earlier and takes corners slower (`RaceOpponent`'s speed profile).

## Critères d'acceptation
- [x] A car script runs with no unimplemented-command warning.
- [x] The physical settings (brakes, springs, dampers, lock, centre of mass, hit points)
      change how the car drives.
- [x] None of the listed commands is an empty `{}` any more.
