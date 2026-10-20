# UI-FMV — The game's movies

- **Status:** IN PROGRESS (View Movies done; the movies in the story not yet)
- **Priority:** P3
- **Module:** UI
- **Files:** `src/UI/MoviePlayer.{h,cpp}`, `src/UI/OptionsMenu.cpp` (View Movies),
  `src/Game.cpp` (`GameState::Movie`, `movieUnlocked`, `playMovie`),
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

## Not done

- The movies in the story: the `fmv` objective (`SetFMVInfo`), `SetPostLevelFMV`, the
  gags' (`GagPlayFMV("tele.rmv")`, `GagCheckMovie`), the introduction on a new game.
- The logos at start-up, the credits' movie.
