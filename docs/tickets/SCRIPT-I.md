# SCRIPT-I — Achats / récompenses, dialogue / FMV, missions bonus

- **Status:** TODO
- **Priority:** P3
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp:30,119,185-197,218,223-228,233-238,274,413,417`

## Contexte
Les systèmes méta — achats de voitures et récompenses, méta-données de dialogue/FMV, et missions bonus — sont entièrement des stubs vides `{}`. Ces fonctionnalités de progression sont définies dans les bibles de mission mais n'ont aucun effet.

## Approche
- Ajouter dans `ScriptEngine` des registres pour : récompenses/achats, infos de dialogue/FMV, missions bonus (motif builder « Add.../Set... »).
- Câbler les stubs (`GameCommands.cpp`) :
  - achats / récompenses : `BindReward` (`:30`), `AddPurchaseCarReward` (`:193`), `AddPurchaseCarNPCWaypoint` (`:197`), `SetCoinFee` (`:218`), `SetRaceEnteryFee` (`:223`), `SetGamblingOdds` (`:377`), `AddVehicleSelectInfo` (`:233`), `ClearVehicleSelectInfo` (`:385`),
  - dialogue / FMV : `SetDialogueInfo` (`:119`), `SetDialoguePositions` (`:254`), `SetCompletionDialog` (`:246`), `SetFMVInfo` (`:274`), `SetPostLevelFMV` (`:294`),
  - missions bonus : `AddBonusMission` (`:228`), `AddMission` (`:146`), `SetBonusMissionStart` (`:417`), `AddBonusObjective` (`:413`), `AddNPCCharacterBonusMission` (`:185`), `AddBonusMissionNPCWaypoint` (`:190`), `SetBonusMissionDialoguePos` (`:235`).
- Un stockage de champ + `Log::Info` consommant tous les paramètres suffit pour la plupart (P3) ; brancher au vrai système d'économie/progression si disponible.

## Critères d'acceptation
- [ ] Un `.con` représentatif (achats / dialogue / mission bonus) s'exécute avec 0 avertissement « unimplemented command ».
- [ ] Une récompense d'achat de voiture est enregistrée et associée à son coût.
- [ ] Une mission bonus définie est enregistrée et démarrable.
