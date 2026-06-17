# Analyse Ghidra — Simpsons Hit & Run

## Statut

Le vrai binaire du jeu est dans `data3.cab` (CD2), un archive InstallShield auto-extractible.
Le CAB contient le jeu complet (assets + DLLs + EXE). L'extraction nécessite `unshield`.

## Setup

```bash
# Installer unshield
sudo apt install unshield

# Extraire le CAB
unshield x /tmp/cd2/data3.cab -d /tmp/game_files

# Trouver le binaire principal
find /tmp/game_files -name "*.exe" -o -name "*.dll" | xargs ls -lh
```

Le jeu original utilise probablement une architecture :
- `Simpsons.exe` → launcher (22KB)
- `game.dll` ou `engine.dll` → logique principale

## Cibles d'analyse priorisées

### 1. P3DZ — Compression propriétaire (0x5A443350)

**Rechercher dans Ghidra :**
- Search → Memory → chercher la valeur `43 50 44 5A` (P3DZ big-endian) ou `5A 44 50 43` (little-endian)
- Identifier la fonction qui compare avec `0x5A443350`
- La fonction de décompression est probablement proche
- Chercher les références à `zlib`, `deflate`, `compress`

**Classes C++ probables :**
- `tP3DFile`, `tP3DFileHandler`
- `tFile` → `LoadP3D`

### 2. Character Controller

**Fonctions à identifier :**
- Chercher les strings `"gravity"`, `"stepHeight"`, `"walkDirection"`
- La classe contient probablement : `mVerticalVelocity`, `mVerticalOffset`, `mStepHeight`
- `OnGround()` → chercher la fonction qui teste `mVerticalVelocity == 0` ou fait un raycast

**Classes C++ identifiées (via RTTI) :**
- `BaseDamper` (amortisseur)
- `JumpAction` (saut)
- `SteeringSpring` (direction)
- `SuspensionJointDriver` (suspension)

### 3. Constantes physiques véhicules

**Valeurs par défaut à extraire :**
- `SetMass` → masse par défaut (~1000-1500 kg)
- `SetGasScale` → accélération
- `SetTopSpeedKmh` → vitesse max (~180 km/h)
- `SetSuspensionLimit` → limite suspension
- `SetSpringK` → raideur ressort
- `SetDamperC` → amortissement

Chercher les valeurs initialisées dans le constructeur du véhicule.

### 4. Format script .con

**Déjà confirmé :** texte clair, pas bytecode.

**À vérifier dans Ghidra :**
- Chercher la fonction qui lit/parse `.con` (probablement `tScriptLoader` ou `tMission`)
- Confirmer la syntaxe exacte
- Identifier toutes les commandes non documentées

### 5. Game flow / State machine

**Fonctions à identifier :**
- `SelectMission` → charge le `.con`, init MissionState
- `AddStage` / `CloseStage` → gestion étapes
- `CloseMission` → fin de mission

## Méthodologie

1. **Importer le binaire dans Ghidra**
   - File → Import → sélectionner le .exe ou .dll
   - Langage : x86 32-bit / PE
   - Analyse avec options par défaut

2. **Chercher les strings significatives**
   - Window → Defined Strings
   - Filtrer par : `p3d`, `gravity`, `jump`, `SelectMission`, `chunk`

3. **Identifier les classes via RTTI**
   - Chercher les patterns `.?AV` (MSVC RTTI)
   - Ghidra peut reconstruire la hiérarchie via Class Recovery

4. **Renommer les fonctions**
   - Double-clic sur un symbole → press `L` → entrer le nom
   - Exporter : File → Export Program → format XML ou C header

5. **Exporter les données**
   - Script Python headless :
   ```python
   fm = currentProgram.getFunctionManager()
   for func in fm.getFunctions(True):
       print(func.getName())
   ```

## Constantes déjà connues (depuis les strings)

| Commande | Valeur par défaut | Source |
|----------|-------------------|--------|
| `SetHitPoints` | 15.0 | `SetHitPoints(15.0)` |
| `SetGamblingOdds` | 2.0 | `SetGamblingOdds(2.0)` |
| `Gravity` | 9.8 | Standard physique |
| `Jump velocity` | ~5.0 | déduction |
| `Step height` | 0.5 | P3D données |

## Alternatives sans Ghidra

- **Cheat Engine** : scanner la mémoire du jeu en cours pour trouver les adresses
- **x64dbg** : debugger le processus, poser des breakpoints sur les fonctions
- **Process Monitor** : voir quels fichiers le jeu charge
- **DLL export viewer** : `dumpbin /exports game.dll`
