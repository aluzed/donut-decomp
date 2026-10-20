# AUDIO-007 — Dialogue: subtitles and lip sync

- **Status:** IN PROGRESS (subtitles done; lip sync not)
- **Priority:** P2
- **Module:** Audio
- **Files:** `src/Audio/AudioManager.{h,cpp}` (dialogue queue, `LoadSubtitles`,
  `GetSubtitle`), `src/Game.cpp` (`drawSubtitle`), `tools/transcribe_dialogue.py`

## Context
The dialogue queue plays the game's lines (`QueueDialogue`, `processDialogueQueue`), one
at a time. The first plan, to read each line's text from the frontend's text bibles,
does not hold: the game has no text for its dialogue. Its text bible
(`srr2.txt`, `srr2.p3d`) holds menus, cards, objectives and names; the executable has no
subtitles; `DIALOGF.RCF`'s 3713 lines are sound only. The subtitles are therefore heard
by machine (below).

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

## Subtitles (2026-10-20)

The voices of this copy are French (the game's French release: `DIALOGF.RCF`, "F").
`tools/transcribe_dialogue.py` reads the library (as `RCL::RCFFile`), decodes each line
(16-bit PCM, or Radical's 4-bit ADPCM, `RSD4RADP`, as `RCL::RSDFile`), resamples it to
16 kHz mono and has a speech recogniser hear it, a line at a time: NVIDIA's Parakeet TDT
0.6B v3 (`--engine parakeet`, the default; 25 European languages, French among them),
run by onnx-asr on the graphics card (an RTX 2080 Ti: the 3713 lines in under eight
minutes). faster-whisper stays as `--engine whisper`. The table it writes,
`audio/dialog/subtitles.tsv` (a line's name in the library, a tab, its text, UTF-8), is
the game's text and is not versioned: it is made once, like the assets (`art/`).

Machine heard: mostly right ("Merci d'apporter ma maquette du système digestif. Hé, où
est passée la vésicule biliaire?"), names and shouts wrong in places ("milouz" for
Milhouse). Parakeet tells the language itself: 13 short or shouted lines came out in
Russian or Czech; 14 are empty.

The recogniser is not a dependency of the game. It ran from a virtual environment
outside the repository (`uv venv`; `uv pip install "onnx-asr[gpu,hub]"
"onnxruntime-gpu[cuda,cudnn]" numpy`, which bring CUDA 13 and cuDNN 9), the model
(`istupakov/parakeet-tdt-0.6b-v3-onnx`) downloaded to a plain folder (`--model-dir`):
ONNX Runtime refuses the Hugging Face cache's links to its weights.

In the game (`AudioManager::LoadSubtitles`, at start-up), every sound started by name
(`PlayAudio`, positioned or not; the dialogue queue's lines among them) that has a text
becomes the subtitle, for as long as it is heard and 0.6 s more. Who speaks: a
character's own lines are in its folder (`agnes\p_hitbyc_agn_hurt.rsd`); a
conversation's line names its speaker by a three-letter code two words after its number
(`c_bladder_1_convinit_lis_l1m1`), and each folder's code is the word most of its files
carry (`lis` for lisa). Generic people are not named. `Game::drawSubtitle` draws it low
in the middle of the screen, wrapped to four fifths of its width, the speaker in yellow,
over a shadow, in game, paused and on a mission's end; not over the menus or a movie.

A line with letters beyond Latin-1 (the Russian and Czech ones) has no subtitle.

The game's text is Latin-1 and its fonts have most accented letters, but every letter
past 127 was looked up as a negative number and dropped ("ça" drawn "a"): `Font` and
`SpriteBatch` now take the bytes as unsigned. This was true of every French text drawn.
A letter the font lacks (font0_16 has no "ù") is drawn as its letter without the
accent.

Checked in game: Agnes's "Tu crois que ça fait mal ? ..." on two lines with its
accents; level 1 m1's first line, "Lisa: Merci d'apporter ma maquette...", its speaker
told by its code.

Not done: a setting to hide them; the movies' lines (not transcribed); lip sync.
