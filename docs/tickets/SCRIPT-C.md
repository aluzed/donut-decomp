# SCRIPT-C — Commandes caméra

- **Status:** TODO
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

## Critères d'acceptation
- [ ] Un `.con` représentatif contenant des commandes caméra s'exécute avec 0 avertissement « unimplemented command ».
- [ ] Les noms de caméra sont enregistrés et au moins le passage caméra de départ voiture / caméra de conversation change réellement la vue.
- [ ] Aucune commande caméra listée ne reste un `{}` vide.
