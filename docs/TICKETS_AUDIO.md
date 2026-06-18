# Tickets — Audio

## AUDIO-001 — Multi-source management
**Priority:** P1
**File:** `src/Audio/AudioManager.h/.cpp`
**Description:** Single OpenAL source and buffer. Cannot play multiple sounds simultaneously (engine + music + dialogue).
**Fix:** Source pool (e.g. 32). Dynamic allocation/release.

## AUDIO-002 — Audio streaming (music)
**Priority:** P1
**Description:** RSD music files are too large for full buffer. Implement circular buffer double-buffering streaming.

## AUDIO-003 — Spatial / 3D audio
**Priority:** P2
**Description:** Position sources in 3D space. Car sounds, pedestrians, dialogue must be spatialized. OpenAL supports this natively.

## AUDIO-004 — Implement PlayAudio with hash
**Priority:** P1
**File:** `src/Audio/AudioManager.cpp`
**Description:** `PlayAudio(uint32_t hash)` overload is declared but missing. Mission scripts reference sounds by hash.

## AUDIO-005 — Populate `_sounds` cache
**Priority:** P2
**File:** `src/Audio/AudioManager.cpp`
**Description:** `LoadRCF` doesn't populate `_sounds` map. Add index on load.

## AUDIO-006 — Auto-load RCF files
**Priority:** P3
**File:** `src/Audio/AudioManager.cpp` (constructor)
**Description:** Hardcoded list of 10 names. Scan `audio/` directory automatically.

## AUDIO-007 — Dialogue management
**Priority:** P2
**Description:** `DIALOGF.RCF` (159MB) contains all game dialogue. Implement:
- Lip sync (based on sample durations?)
- Subtitles (from text bibles)
- Dialogue priority (cutscene > gameplay)

## AUDIO-008 — EFX support (reverb)
**Priority:** P3
**Description:** Per-zone reverb for interiors vs exterior. OpenAL EFX.
