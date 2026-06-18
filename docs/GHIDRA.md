# Ghidra Analysis — Simpsons Hit & Run

## Status

The real game binary is inside `data3.cab` (CD2), an InstallShield self-extracting archive.
The CAB contains the complete game (assets + DLLs + EXE). Extraction requires `unshield`.

## Setup

```bash
# Install unshield
sudo apt install unshield

# Extract the CAB
unshield x /tmp/cd2/data3.cab -d /tmp/game_files

# Find the main binary
find /tmp/game_files -name "*.exe" -o -name "*.dll" | xargs ls -lh
```

The original game likely uses this architecture:
- `Simpsons.exe` → launcher (22KB)
- `game.dll` or `engine.dll` → main logic

## Priority Analysis Targets

### 1. P3DZ — Proprietary Compression (0x5A443350)

**Search in Ghidra:**
- Search → Memory → look for bytes `43 50 44 5A` (P3DZ big-endian) or `5A 44 50 43` (little-endian)
- Identify the function that compares with `0x5A443350`
- The decompression function is likely nearby
- Look for references to `zlib`, `deflate`, `compress`

**Likely C++ classes:**
- `tP3DFile`, `tP3DFileHandler`
- `tFile` → `LoadP3D`

### 2. Character Controller

**Functions to identify:**
- Search for strings `"gravity"`, `"stepHeight"`, `"walkDirection"`
- The class likely contains: `mVerticalVelocity`, `mVerticalOffset`, `mStepHeight`
- `OnGround()` → find the function that tests `mVerticalVelocity == 0` or does a raycast

**C++ classes identified (via RTTI):**
- `BaseDamper`
- `JumpAction`
- `SteeringSpring`
- `SuspensionJointDriver`

### 3. Vehicle Physics Constants

**Default values to extract:**
- `SetMass` → default mass (~1000-1500 kg)
- `SetGasScale` → acceleration
- `SetTopSpeedKmh` → max speed (~180 km/h)
- `SetSuspensionLimit` → suspension limit
- `SetSpringK` → spring stiffness
- `SetDamperC` → damping

Search for initialized values in the vehicle constructor.

### 4. Script .con Format

**Already confirmed:** plain text, not bytecode.

**To verify in Ghidra:**
- Find the function that reads/parses `.con` (likely `tScriptLoader` or `tMission`)
- Confirm exact syntax
- Identify all undocumented commands

### 5. Game Flow / State Machine

**Functions to identify:**
- `SelectMission` → loads `.con`, initializes MissionState
- `AddStage` / `CloseStage` → stage management
- `CloseMission` → mission end

## Methodology

1. **Import binary into Ghidra**
   - File → Import → select the .exe or .dll
   - Language: x86 32-bit / PE
   - Analyze with default options

2. **Find meaningful strings**
   - Window → Defined Strings
   - Filter by: `p3d`, `gravity`, `jump`, `SelectMission`, `chunk`

3. **Identify classes via RTTI**
   - Search for `.?AV` patterns (MSVC RTTI)
   - Ghidra can rebuild the hierarchy via Class Recovery

4. **Rename functions**
   - Double-click a symbol → press `L` → enter name
   - Export: File → Export Program → XML format or C header

5. **Export data**
   - Headless Python script:
   ```python
   fm = currentProgram.getFunctionManager()
   for func in fm.getFunctions(True):
       print(func.getName())
   ```

## Known Constants (from strings)

| Command | Default Value | Source |
|---------|---------------|--------|
| `SetHitPoints` | 15.0 | `SetHitPoints(15.0)` |
| `SetGamblingOdds` | 2.0 | `SetGamblingOdds(2.0)` |
| `Gravity` | 9.8 | Standard physics |
| `Jump velocity` | ~5.0 | Deduced |
| `Step height` | 0.5 | P3D data |

## Alternatives Without Ghidra

- **Cheat Engine**: scan game memory at runtime to find addresses
- **x64dbg**: debug the process, set breakpoints on functions
- **Process Monitor**: see what files the game loads
- **DLL export viewer**: `dumpbin /exports game.dll`
