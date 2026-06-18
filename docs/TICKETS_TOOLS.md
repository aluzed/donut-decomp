# Tickets — Tools / Reverse Engineering

## TOOL-001 — Automatic asset extraction from ISOs
**Priority:** P0
**Description:** Script to extract game files from 3 CDs: extract .7z → ISOs, extract CABs with unshield, organize into `assets/` tree.

## TOOL-002 — Reverse SIMPSONS.EXE in Ghidra
**Priority:** P0
**Description:** The real game binary (not the 22KB launcher). Goals:
- [ ] Identify entry point and main loop
- [ ] Find P3D parsing functions (for unknown chunks)
- [ ] Find script parsing (.con)
- [ ] Find P3DZ algorithm (header 0x5A443350)
- [ ] Find RMV format (if needed)
- [ ] Document key data structures
- [ ] Rename identified functions
- [ ] Export symbol file

## TOOL-003 — Create Ghidra test script (headless)
**Priority:** P3
**Description:** Automate data extraction from Ghidra.

## TOOL-004 — Update donut-codegen.exe for new chunks
**Priority:** P1
**Description:** The tool `dev/codegen/donut-codegen.exe` takes `p3d.json` and generates `P3D.generated.h/.cpp`.
- [ ] Verify the exe works (Windows? .NET? Mono?)
- [ ] Add missing definitions to `p3d.json`
- [ ] Re-run generation
- [ ] Fix resulting compilation errors

## TOOL-005 — P3D chunk visualization tool
**Priority:** P2
**Description:** Extend `debugDrawP3D` in `Game.cpp` with:
- Raw data hex dump
- Comparison between two P3D files
- Search by chunk type
- Texture export

## TOOL-006 — Auto-extract commands from original exe
**Priority:** P2
**Description:** Ghidra script to auto-extract registered commands from the string table.

## TOOL-007 — Automatic P3D parsing validation
**Priority:** P3
**Description:** Unit tests for P3D parsing: canonical P3D file → parse → re-serialize → binary compare.

## TOOL-008 — Script format documentation (.con)
**Priority:** P0
**Description:** Create `docs/script_format.md` with file structure, opcode list (if bytecode), command mapping, parameter format, decompiled script examples.

## TOOL-009 — Cross-reference with similar projects
**Priority:** P3
**Description:** The Radical engine was used in multiple games. Study:
- **OpenC1** (Crazy Taxi-like, modified Radical engine)
- **Donut Mod Loader** (existing Simpsons H&R mods)
- **Lucas' Pure3D Editor** (P3D editing tool)
- **P3DView** (viewer)
- Community wiki documentation

## TOOL-010 — Setup CI/CD (Azure Pipelines or GitHub Actions)
**Priority:** P3
**Description:** Configure CI: Linux + Windows build, unit tests, code style check (clang-format target already exists).
