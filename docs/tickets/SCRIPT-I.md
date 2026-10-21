# SCRIPT-I — Buying and rewards, dialogue and FMV, bonus missions

- **Status:** DONE (2026-10-21)
- **Priority:** P3
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp`, `src/Scripting/ScriptEngine.{h,cpp}`,
  `src/Game/GagManager.{h,cpp}`, `src/Game.cpp`, `src/GameLevel.cpp`

## Context
The meta commands (buying cars and rewards, dialogue and FMV metadata, bonus missions)
were empty stubs. Most of them came to work since by another way: the game reads the
level scripts' calls itself (`levelScriptCalls`, `Game::rewards`). This ticket closes what
was left.

## Already working (other tickets)
- **Buying and rewards**: `BindReward` (rewards.mfk, cars and outfits with their prices),
  `AddPurchaseCarReward` (the car sellers), `SetCoinFee` (a "coins" objective's fee),
  `SetGamblingOdds` (a car's setting): read by the game (`Game::rewards`, `_carSellers`),
  bought in the shops and the phone booths.
- **Bonus missions**: `AddNPCCharacterBonusMission`, `AddBonusMission`, `AddMission`: the
  givers stand where the level says, and their missions start from them
  (`startBonusMission`).
- **Dialogue and FMV**: `SetDialogueInfo` (the conversation, its two characters),
  `SetConversationCam`, `SetFMVInfo` (the story's movies).

## Done here
- **`SetDialoguePositions`** (98 calls): as a stage's conversation starts, the player is put
  on foot on their mark facing the other speaker, the other speaker on theirs, and the
  player's car where the next stage wants it. The bonus missions' own conversations use it
  too (their scripts call it).
- **`SetCompletionDialog`** (38): the stage's closing conversation, heard over what follows
  as the stage is completed ("nuisance": Lisa and Comic Book Guy, level 3 m1).
- **`SetRaceEnteryFee`** (7): a gambling race won pays the fee times the odds of the car
  raced in (its `SetGamblingOdds`), as the scripts' comments say ("sets the Race Payout,
  multiplied by a car's odds").
- **`GagCheckCollCards`**: Comic Book Guy's Itchy & Scratchy Movie ticket (level 3): his
  terms the first time, a sneer after, and the ticket for a whole set of cards (49). The
  Aztec's box office (`GagCheckMovie`) now wants that ticket, not the cards.
- **`GagSetSparkle`** (86): a gag not yet found glints.

Validation (seven levels' stories under the autopilot): 32 missions won; 32 conversations
placed on their marks, 13 closing conversations heard.

## Not done, and why
- `SetPostLevelFMV`: commented out in every level script; only the E3 demo's call it.
- `SetBonusMissionDialoguePos`, `AddBonusMissionNPCWaypoint`, `AddPurchaseCarNPCWaypoint`:
  the givers' and sellers' walks and the conversation before a bonus mission; the givers
  stand still, and the mission starts at once.
- `AddVehicleSelectInfo`, `ClearVehicleSelectInfo`, `AddBonusObjective`,
  `SetBonusMissionStart`: the phone booth lists the cars the player has; the last two are
  in no script.
- `GagSetAnimCollision`, `GagSetIntro`/`Outro`, `ClearGagBindings`,
  `GagSetSoundLoadDistances`: a gag's moving collision, its animation's ends, streaming.

## Acceptance criteria
- [x] A representative script runs with no "unimplemented command" warning (none is
      logged over the seven levels' stories).
- [x] A car bought as a reward is recorded with its cost (`BindReward`,
      `AddPurchaseCarReward`).
- [x] A bonus mission is recorded and can be started (its giver).
