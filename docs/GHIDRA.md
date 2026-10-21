# Ghidra Analysis — Simpsons Hit & Run

## The binaries

The game installs from `data3.cab` (CD2); `tools/extract_cab.sh` puts its files under
`files/extracted/`. The code is in two files of `files/extracted/Executables/`:

| File | Size | What it is |
|---|---|---|
| `Simpsons.exe` | 3.4 MB | The whole game: Pure3D, the radcore libraries (`radLoad`, `radMovie`, `radSound`), the game itself. Not a launcher |
| `pddidx8r.dll` | 420 KB | Pure3D's renderer for Direct3D 8 (`pddi`), exported by name (`pddiColour::Set`...) |
| `binkw32.dll`, `eax.dll` | | Bink video, EAX audio: third-party |

`Simpsons.exe` is wrapped by a copy protection: two extra sections (`.nndkm` at
`0x6d2000`, `.tadb` at `0x717000`), the entry point in `.nndkm` (`0x70ca07`), and the
Windows imports resolved at run time, so the decompiled game calls no `user32` function
directly. The game's own code (`.text`, `0x401000`-`0x5f3000`) and data are intact and
analyse normally.

## Running it

```bash
tools/ghidra/analyze.sh ~/ghidra_12.1.2_PUBLIC          # output in build/ghidra
```

Headless, about 35 minutes. It writes, for each binary:

| File | Content |
|---|---|
| `*.functions.tsv` | every function: address, size, name, thunk, calling convention |
| `*.strings.tsv` | every defined string and the functions referring to it |
| `*.imports.tsv`, `*.exports.tsv` | imports (with reference counts), exports |
| `*.classes.tsv` | the class names Ghidra recovered from the RTTI |
| `*.c` | the decompiled C of every function, each headed `// ==== <address> <name>` |
| `Simpsons.exe.vtables.tsv` | every virtual function table from the RTTI: class, table, subobject offset, slot, function |

The output is the game's code: it stays out of git, like the art.

The scripts, in `tools/ghidra/`:

- `ExportAll.java`, `DecompileAll.java`: the exports above;
- `RefsTo.java <address>...`: what refers to each address (a data reference is usually a
  vtable slot);
- `Disasm.java <address>...`: the instructions of the function containing each address
  (the decompiler loses the arguments of calls through vtables; the instructions keep
  them);
- `FindScalar.java <hex>...`: every instruction using one of these constants;
- `rtti_vtables.py`: the vtables, rebuilt from the RTTI. Ghidra's RTTI analyzer labels the
  1337 type descriptors but none of the tables; each table is preceded by a pointer to its
  complete object locator, which points to the descriptor. 1605 tables, 12477 slots.

To run one script on the analysed project:

```bash
$GHIDRA/support/analyzeHeadless build/ghidra/project shar -process Simpsons.exe -noanalysis \
    -scriptPath tools/ghidra -postScript Disasm.java 00572810
```

## What is where

Found so far; addresses are `Simpsons.exe`'s unless named.

| Subject | Where | Notes |
|---|---|---|
| Game flow | `GameFlow` (vtable `0x614810`), `Context` (`0x614bb8`), `GameplayContext` (`0x614948`), `BootupContext`, `FrontEndContext`, `DemoContext`, `EntryContext`, `ExitContext` | The game runs as contexts; `GameFlow`'s destructor (`0x4331f0`) destroys 12 of them. `Context`'s slots 0x8-0x1c are pure virtual (`_purecall`, `0x5c7c7a`), each context overriding them; their roles are not read yet |
| Chunk files | `tChunkFile` (vtable `0x5f8a24`), its constructor `0x574a80` | Reads the file's first word: `P3D\xff` as is; `0x503344ff` byte-swapped; `P3DZ` (`0x5a443350`): reads the uncompressed size, then switches its stream to decompressing |
| P3D loading | `tP3DFileHandler` (vtable `0x5f88e4`), the many `*Loader` classes (`AnimDynaPhysLoader`, `CStatePropDataLoader`, `FenceLoader`, ...) | The loaders' names match the chunk types they read; how they are registered is not read yet |
| Scripts (`.con`, `.mfk`) | `Console` (`0x614cd8`), `ConsoleFileHandler` (`0x613d08`, `0x613d18`) | Plain text, run line by line by the console; the commands' names (`SelectMission` `0x612858`, `AddStage` `0x612748`, `SetDynaLoadData` `0x612810`...) are referred to from data only, a command table. `bonus/bg%d_%d.con` is built by `0x514860` |
| Movies (`.rmv`) | `radMoviePlayerBink` (`0x5f70f8`), `IRadMoviePlayer2`, `BinkCallback`; logos played by `0x42f740` | Bink files under a radcore wrapper; `0x60f3d8`-`0x60f4ac` list them |
| Rendering | `RenderManager`, `RenderLayer`, `WorldRenderLayer` (`0x60be24`), `FrontEndRenderLayer`; Pure3D's `tView` (`0x5f87e4`), `tPointCamera` (`0x5f8984`), `tLight` (`0x5f87fc`) | The render manager (`0x4a80a0`) gives each layer its views and cameras |
| Fog | `tView` draw `0x572810`; `pddidx8r.dll` `EnableFog` `0x10005ce0` (context vtable +0x148), `SetFog` `0x10005d10` (+0x150) | Never on: see [LEVEL-FOG](tickets/LEVEL-FOG.md) |

## Notes for reading the decompiled C

- Calls through a vtable lose their arguments: `(**(code **)(*DAT_0065ef1c + 0x150))()`.
  `Disasm.java` shows the pushes before the `CALL`.
- A field is written either at a byte offset (`*(undefined1 *)(p + 0xbc)`) or at a word
  index (`*(undefined1 *)(param_1 + 0x2f)`, `param_1[0x2f]`, for an `int *`): search both.
- `DAT_0065ef1c` holds the pddi render context (its `EnableFog`/`SetFog` slots are called on it).
