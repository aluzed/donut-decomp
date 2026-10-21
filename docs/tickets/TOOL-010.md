# TOOL-010 — CI on GitHub Actions (Linux and Windows builds, tests, clang-format)

- **Status:** DONE (2026-10-21)
- **Priority:** P3
- **Module:** Tools
- **Depends on:** TOOL-007
- **Files:** `.github/workflows/ci.yml`, `src/ThirdParty/.clang-format`,
  `src/Render/imgui/.clang-format`, `src/Render/OpenGL/glad/.clang-format`

## Context
Nothing checked that the project built on Linux and Windows, that the tests passed, or
that the style held.

## Done
`.github/workflows/ci.yml`, on every push and pull request, three jobs:
- **linux** (ubuntu-24.04): SDL2, Bullet, OpenAL, fmt, glm and zlib from apt; CMake build;
  `ctest`.
- **windows** (windows-2022, MSVC): the same libraries from vcpkg; CMake build;
  `ctest`.
- **format**: `git clang-format` (clang-format 18.1.8, from pip) on the lines the push or
  pull request changes, against `.clang-format`; a difference fails the job. Only the
  changed lines: half of the older code predates `.clang-format`, and reformatting it all
  would bury the history. The third-party folders keep their authors' format (a
  `.clang-format` with `DisableFormat` in each).

The tests that need the game's files are skipped in CI (none are there); those built in
the tests run.

## What the first runs found
- glm was used but no build file said so: installed on both.
- MSVC: `windows.h` makes `near` a macro (a lambda renamed); `GameLevel.cpp` had its
  standard headers only through others on Linux; `Matrix4x4` was declared a `class` in two
  headers, a `struct` everywhere else, which MSVC mangles apart (unresolved symbols).
- The style check caught this session's own unformatted lines.

All three jobs green since `42967c4`.

## Acceptance criteria
- [x] The workflow runs on push and pull request.
- [x] Green builds on Linux and Windows.
- [x] `ctest` runs and fails the job when a test fails.
- [x] The clang-format check fails the job on a style difference (on the changed lines).
