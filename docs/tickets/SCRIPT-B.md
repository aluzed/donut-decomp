# SCRIPT-B — Commandes de réglage véhicule (tuning)

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Un `.con` représentatif de stats véhicule s'exécute avec 0 avertissement « unimplemented command ».
- [ ] Les réglages physiques (`SetBrakeScale`, `SetSpringK`, `SetDamperC`, `SetMaxWheelTurnAngle`, etc.) modifient effectivement le comportement du véhicule actif.
- [ ] Plus aucune des ~30 commandes listées n'est un corps `{}` vide.
