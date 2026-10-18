# AUDIO-007 — Sous-titres de dialogue + lip sync

- **Status:** TODO
- **Priority:** P2
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/AudioManager.h:32-48,93,102` (Dialogue, QueueDialogue, processDialogueQueue), bibles frontend

## Contexte
La file de dialogue prioritaire est déjà fonctionnelle (`Dialogue`, `QueueDialogue`, `processDialogueQueue`, `PlayDialogue`). En revanche, aucun sous-titre n'est affiché : le texte associé à chaque ligne de dialogue n'est pas extrait ni rendu de façon synchronisée.

## Approche
- Étendre `struct Dialogue` (`AudioManager.h:32-36`) avec le texte et une durée d'affichage.
- Charger une table nom -> texte sous-titre extraite des bibles frontend (parser le format des bibles de dialogue) au chargement audio.
- Dans `processDialogueQueue`/`PlayDialogue`, quand une ligne démarre, publier le sous-titre actif (texte + horodatage de fin) ; l'effacer dans `Update` quand sa durée est écoulée.
- Exposer le sous-titre courant pour que la couche HUD/rendu l'affiche ; optionnellement, piloter un paramètre de lip sync (amplitude) à partir du RMS du signal en cours de lecture.

## Critères d'acceptation
- [ ] Le texte du dialogue en cours est extrait des bibles frontend et exposé par `AudioManager`.
- [ ] Le sous-titre apparaît au début de la ligne et disparaît à la fin de sa durée.
- [ ] La priorité de file existante reste respectée (un dialogue Cutscene coupe toujours un Gameplay).

## The missions' conversations (2026-10-18)

The voices are in `DIALOGF.RCF`, which the `audio/` scan skipped for its capital
extension; it is now picked up (5389 files). A mission conversation's lines are named
`conversations\c_<name>_<n>_<kind>_<speaker>_l<level><mission>.rsd`, and a dialogue stage
names its conversation in `SetDialogueInfo("homer", "lisa", "bladder", 0)`, which did
nothing. A `dialogue` objective now queues that conversation's lines for the mission (a
story mission mN is `mN`, a bonus mission bmN `bN`, a street race srN `rN`), and is met
when the last has been said; with no lines found it is still a 3s pause.

The dialogue queue played every queued line at once, one per free source; it now plays
one line when the last has finished. Sounds played without a position were placed at the
world's origin, far from the listener; they are now heard where the listener is.

Checked by log on level 1 m1's last stage: "bladder", six lines, Lisa at 0s, Homer at
5.3s, Lisa at 8.1s, then 9.9, 11.3 and 16.4s, and the mission complete after the last.
Not heard: nobody listened on this machine. No subtitles yet.

## The player's character speaks (2026-10-18)

Each character has their own lines in `DIALOGF.RCF`, `<character>\<kind>_<who>_NN.rsd`:
`d_*` at the wheel (`d_bcrash` a big crash, `d_mcrash` a lesser one, `d_hitp` someone
run down, `d_air`...) and `w_*` on foot (`w_card` a card found, `w_mfail` a mission
failed...). `Game::SayLine` plays one of the player character's takes of a kind, at most
one every 3s and never over a mission's conversation: on a crash beyond 7 m/s, on running
someone down, on finding a card, on failing a mission, on winning one (`w_mvic`), on a
jump of more than 0.8s (`d_air`), as the turbo kicks in (`w_turbo`). Homer, Bart, Lisa,
Marge and Apu have 3 to 8 takes of each. Others speak too: a traffic driver rammed
(`traffic1..4\d_*crash`), a resident whose doorbell is rung (`w_doorbell`), the level's
people walked up to (`w_idlereply`). Not heard on this machine.
