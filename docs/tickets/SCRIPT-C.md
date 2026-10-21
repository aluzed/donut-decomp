# SCRIPT-C — Commandes caméra

- **Status:** DONE (2026-10-21)
- **Priority:** P2
- **Module:** Scripting
- **Depends on:** —
- **Files:** `src/Scripting/GameCommands.cpp:216-217,239,251-252,426-431,437`, caméra dans `src/Game.cpp`

## Contexte
Les commandes de caméra scriptée (caméras animées, caméras de conversation, caméra de départ voiture) sont toutes des stubs vides `{}`. Les missions définissent des plans de caméra qui sont silencieusement ignorés.

## Approche
- Introduire dans `ScriptEngine` un petit registre de caméras nommées (nom -> type/cibles) consommé par le système de caméra existant de `Game`.
- Câbler les stubs suivants (`GameCommands.cpp`) vers ce registre + `Log::Info` :
  - `SetAnimatedCameraName` (`:216`), `SetAnimCamMulticontName` (`:217`),
  - `SetConversationCam` (`:239`), `SetConversationCamName` (`:426`), `SetConversationCamPcName` (`:427`), `SetConversationCamNpcName` (`:428`), `SetConversationCamDistance` (`:429`),
  - `SetMissionStartCameraName` (`:251`), `SetMissionStartMulticontName` (`:252`),
  - `SetCarStartCamera` (`:431`), `SetStageCamera` (`:416`), `SetCamBestSide` (`:123`).
- Au démarrage d'un stage/mission, activer la caméra nommée correspondante via le pivot caméra existant (cf. commit `mouse-controlled third-person orbit camera`).

## Done (2026-10-21)

| command | scripts | now |
|---|---|---|
| `SetConversationCam(line, shot[, mission])` | 92 | each line of a dialogue stage framed as the script says: `pc_*` the player's character, `npc_*` the other, from behind the one facing them; `far` takes in both, `near` the subject alone. The level script's, tagged with a mission ("sr1"), stand in for a stage that gives none |
| `SetCamBestSide(locator[, mission])` | 79 | the side of the two speakers the camera keeps to |
| `SetMissionStartCameraName` / `SetMissionStartMulticontName` | 60 | the mission's camera move, played as it starts, under its title card |
| `SetAnimatedCameraName` / `SetAnimCamMulticontName` | 76 | the same for the races (sr, gr), whose scripts give only these |
| `SetConversationCamPcName`, `SetConversationCamNpcName` | only commented out ("ASK IAN ABOUT THIS COMMAND") | left empty |
| `SetStageCamera`, `SetConversationCamName`, `SetConversationCamDistance`, `SetCarStartCamera` | none | left empty |

- `AnimCamera`, which loaded a camera file and returned the identity, now reads the move:
  the animation `CAM_<camera>` of `art/missions/levelNN/<multicont>.p3d`, its position
  (`TRAN`, world space), direction (`LOOK`) and up (`UP`) tracks, interpolated between
  their keys at 30 frames a second (level 1's `mission2cam`: 91 frames, 3 s). Its field
  of view and clip planes (`FOV`, `NCLP`, `FCLP`) are left to the game's camera.
- A line's number is the count of dialogue lines `AudioManager` has started since the
  conversation was queued.
- **Found on the way: mission characters were missing.** The game names its character
  files in six letters at most (`milhou_m.p3d`, `kearne_m.p3d`, `skinne_m.p3d`, 56 of
  them). `Game::makeNpc` looked for the full name, and Milhouse, Kearney, Skinner,
  Smithers and others never appeared in their missions. It now tries the six letters.
- The autopilot walks up to someone to talk to and stops 1.5 m away; it stood on the very
  spot, and the shots framed the two as one.

## Critères d'acceptation
- [x] The mission scripts run with no unimplemented-command warning.
- [x] The mission start moves and the conversation shots change the view (level 2: `mission1cam`, 6 s; Milhouse's and Jimbo's conversations, a shot a line).
- [x] Every camera command a script uses does something; the six left empty are in no script, or only commented out.
