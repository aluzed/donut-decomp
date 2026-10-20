# UI-FMV — The game's movies

- **Status:** DONE
- **Priority:** P3
- **Module:** UI
- **Files:** `src/UI/MoviePlayer.{h,cpp}`, `src/UI/OptionsMenu.cpp` (View Movies),
  `src/Game.cpp` (`GameState::Movie`, `playMovies`, `PlayStoryMovie`, `movieUnlocked`, the
  gags'), `src/Scripting/ScriptEngine.cpp` (the `fmv` stage),
  `tools/extract_assets.sh` (`MOVIES/*.RMV` copied to `movies/`), `tools/fetch_deps.sh`

## The format

The `.RMV` files are not Radical's own format: they are Bink (`BIKi`), 640 x 480 at
30 frames a second, YUV 4:2:0, with four soundtracks (`binkaudio_dct`, stereo; the first
at 44.1 kHz, the others at 48 kHz). FFmpeg decodes both. Sixteen on the three CDs:
`INTRO`, `FMV1A`, `FMV2` to `FMV8`, `CREDITS`, the logos (`FOXLOGO`, `RADLOGO`,
`VUGLOGO`, `GRACIE`) and the gags' (`TELE`, `LOOT`).

## The player

`MoviePlayer` reads a movie with libavformat, decodes its pictures (libavcodec,
converted to RGBA by libswscale into one texture, `Texture::Update`) and one of its
soundtracks (libswresample to 16-bit stereo, queued on an OpenAL source half a second
ahead). The sound heard is the clock the pictures keep to (`AL_SAMPLE_OFFSET` and the
buffers heard); without sound, the frames' time. The picture is drawn as large as the
screen holds it, over black.

The soundtrack is the game's language's: 0 English, 1 French, 2 German, 3 Spanish, the
text bible's order. That order is assumed, not heard.

FFmpeg is optional at build time (`FFMPEG_FOUND`, `DONUT_HAS_FFMPEG`): without it, View
Movies is greyed. Its headers come from the `-dev` packages (`tools/fetch_deps.sh`,
into `deps/sysroot`); the libraries are the system's (`libavcodec.so.60` ...), which ship
no unversioned names, so CMake looks for those first.

## View Movies (2026-10-20)

The options' View Movies (from the main menu, not the pause screen) opens
`ViewMovies.pag`: the introduction (`INTRO`), then Movie1 to Movie8 (`FMV1A`, `FMV2` ...
`FMV8`; the text bible's Level 1 Introduction, Level 1, 2, 4, 5, 6, 7 Movie, Bonus
Movie; level 3 has none of its own). The camera goes to the television, as for Display.
The confirm key or a click plays the one picked; the confirm key or Escape (after half a
second) or its end comes back to the page, the show's theme with it.

What may be seen: the introduction and level 1's always (they open the game); a level's
movie once the story is past the step that plays it, its script's `SetFMVInfo` (level
1's `m5sd`, 2's `m8sd`, 4's `m5`, 5's, 6's and 7's `m8sd`); the bonus movie, the Itchy &
Scratchy Movie, with all 49 cards found, as its ticket is (`IS_MOVIE_UNLOCKED`). The
others are greyed.

Checked: `FMV2` played, its clock 0.05 s behind the frames' time throughout; three
screenshots of it; `FMV1A` to its end, back to the page and the theme; the page with the
save at level 7's first mission: up to Level 6 Movie open, Level 7 and Bonus greyed.

## The movies in the game (2026-10-20)

`Game::playMovies` plays a list of them, one after another, over the whole screen, the
game standing still (`GameState::Movie`: no physics, no scripts) and the music off; at
the end, back to where they were played from: the start-up's splash, View Movies (the
show's theme), the game (the level's music back, unless the stage said `stopmusic`).
The confirm key or Escape skips one, or, at start-up, all of them.

- At start-up: the studios' logos, then the game's introduction: `RADLOGO`, `FOXLOGO`,
  `GRACIE`, `VUGLOGO`, `INTRO`. The executable lists them in that order backwards, as it
  does View Movies' (`fmv8` ... `intro`); that the introduction follows the logos is
  assumed.
- A new game: level 1's introduction (`FMV1A`) before the tutorial.
- The `fmv` stage (`SetFMVInfo("fmv3.rmv")`): the movie, the stage over at its end
  (levels 1, 2, 4, 5, 6 and 7; level 7's with `stopmusic`).
- The gags: `GagPlayFMV` (the observatory's telescope, `TELE`, levels 3 and 6; Larry the
  Looter at the Kwik-E-Mart, `LOOT`, levels 4 and 7) plays its movie in place of its
  sound. `GagCheckMovie` (the Aztec's box office, level 3) plays the Itchy & Scratchy
  Movie (`FMV8`) to who holds its ticket, all 49 cards found; to the others, one of the
  clerk's refusals (`c_aztec1` to `c_aztec4`, their lines in turn). It is not a gag
  found.

None under `--autopilot` (the validation's runs, which would wait through them) nor with
`--no-movies`; `--autostart` has no start-up.

Checked: the start-up's logos in turn (Radical, then Fox, then Gracie, at 2, 6, 20 s);
level 2's `m8sd` (`FMV3`, 55 s), the stage complete after it and the game on; the
telescope's movie, then the game; the Aztec without the cards: refused (`aztec2`).

## Not done

- The credits' movie (`CREDITS.RMV`); `SetPostLevelFMV` (commented out in every level's
  script).
- `GagCheckCollCards` (the Comic Book Guy's cards reward, level 3).
