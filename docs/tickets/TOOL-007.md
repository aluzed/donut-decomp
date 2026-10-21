# TOOL-007 — P3D parsing unit tests (parse, reserialise, compare)

- **Status:** DONE (2026-10-21)
- **Priority:** P3
- **Module:** Tools
- **Depends on:** TOOL-004, ASSET-003
- **Files:** `tests/` (`Test.h`, `Main.cpp`, `P3DTests.cpp`, `CMakeLists.txt`), `CMakeLists.txt`,
  `src/P3D/P3DChunk.{h,cpp}` (`Serialize`), `src/P3D/P3DFile.{h,cpp}` (`GetBytes`,
  `DecompressP3DZ`)

## Context
Nothing tested the P3D parser. A round trip (read a file, write it back, compare byte for
byte) holds the parsing to what the files are, and guards the regenerations (TOOL-004).

## Done
- **`donut_tests`, run by `ctest`** (`DONUT_TESTS`, on by default). Built from the few
  sources the tests need (Core, P3D), not the game: no window, SDL, OpenAL or Bullet. The
  harness is `tests/Test.h`, a few lines (`TEST`, `CHECK`, `REQUIRE`, `SKIP`): no test
  framework is installed here, and one header did the job. `donut_tests <part of a name>`
  runs some of them.
- **`P3DChunk::Serialize`** writes a chunk back as a file has it; `P3DFile::GetBytes` is what
  the file's chunks were read from (the file, or what it decompresses to).
- The tests:
  - chunk trees built in the test, read and written back (nested, empty chunks);
  - a whole file, through `P3DFile`;
  - an `RZ` (zlib) file, built in the test: none of the game's files is one;
  - a `P3DZ` block decompressed (literals, then a copy), and refused cut short or copying
    from before its start;
  - a generated class (`FrameController`) on a chunk built in the test;
  - damaged chunks (bytes after the last, a child cut short);
  - **every file under `art/`** (`DONUT_ART` names another folder; skipped when there is
    none, as in CI): 1964 files, 28 of them `P3DZ` (level 5's mission files), all written
    back byte for byte. There is no padding or alignment to tolerate: the files are their
    chunks exactly.

## What the tests found
- A file with bytes after its last chunk, or a child cut short, was read past its end
  (a crash): `P3DChunk` now keeps within the chunk's sizes and stops at a child that does not
  fit.
- `RZ` files were decompressed into a buffer twice their size: the size zlib gave was
  doubled once more after it succeeded, and the chunks were read on into the zeroes.

## Acceptance criteria
- [x] A test harness in the build (`ctest`).
- [x] The round trip passes on real P3D files: all 1964 of the game's.
- [x] P3DZ covered: the 28 game files, and a block built in the test.
- [x] No byte differs; nothing to document.
