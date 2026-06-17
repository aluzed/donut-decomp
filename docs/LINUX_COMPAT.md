# Compatibilité Linux — Guide complet

## Statut : COMPATIBLE (GCC 13.3 / Ubuntu 24.04 LTS)

Ce guide explique comment compiler et exécuter donut sur Linux, pas à pas.

---

## Étape 0 — Vérifier les prérequis matériels

Le projet cible **OpenGL 4.3 core profile**. Vérifie que ton GPU le supporte :

```bash
glxinfo | grep "OpenGL version"
# Doit afficher >= 4.3
```

Si `glxinfo` n'est pas trouvé : `sudo apt install mesa-utils`

Si ta carte supporte < 4.3, tu peux forcer avec Mesa (Intel/AMD) :
```bash
MESA_GL_VERSION_OVERRIDE=4.3 MESA_GLSL_VERSION_OVERRIDE=430 ./bin/donut
```

---

## Étape 1 — Installer les dépendances système

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install \
  cmake build-essential git \
  libsdl2-dev \
  libopenal-dev \
  libbullet-dev \
  libfmt-dev \
  libglm-dev \
  libgl1-mesa-dev \
  mesa-utils
```

| Paquet | Rôle | Obligatoire ? |
|--------|------|:---:|
| `cmake` | Système de build | Oui |
| `build-essential` | GCC, make, libc | Oui |
| `git` | Cloner le repo | Oui |
| `libsdl2-dev` | Fenêtre + input + OpenGL context | Oui |
| `libopenal-dev` | Audio (dialogues, musique, SFX) | Oui |
| `libbullet-dev` | Physique (collisions, véhicules) | Oui |
| `libfmt-dev` | Formatage de chaînes | Oui |
| `libglm-dev` | Mathématiques matricielles (glm) | Oui |
| `libgl1-mesa-dev` | Headers OpenGL | Oui |
| `mesa-utils` | `glxinfo` (diagnostic GPU) | Non |

### Arch Linux

```bash
sudo pacman -S cmake git sdl2 openal bullet-dp libfmt glm mesa
```

### Fedora

```bash
sudo dnf install cmake git gcc-c++ \
  SDL2-devel openal-soft-devel bullet-devel fmt-devel glm-devel \
  mesa-libGL-devel mesa-utils
```

---

## Étape 2 — Cloner le projet

```bash
git clone --recursive https://github.com/plowteam/donut.git
cd donut
```

> Si tu as oublié `--recursive` : `git submodule update --init --recursive`

---

## Étape 3 — Compiler

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

`$(nproc)` utilise tous tes cores CPU. Si la RAM est limitée, mets `-j2` ou `-j4`.

**Ça compile ?** Le binaire est dans `build/bin/donut`.

### Options de build utiles

| Option | Effet |
|--------|-------|
| `-DCMAKE_BUILD_TYPE=Debug` | Symboles de debug, sans optimisation |
| `-DCMAKE_BUILD_TYPE=Release` | Optimisé, sans debug |
| `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | Optimisé avec debug (défaut) |

---

## Étape 4 — Installer les assets du jeu

donut ne contient **pas** les assets de *The Simpsons: Hit & Run*. Tu dois les fournir depuis ta copie légale du jeu.

### Structure attendue

Place les fichiers extraits du jeu dans un dossier `assets/` à la racine :

```
donut/
├── assets/
│   ├── art/
│   │   ├── chars/       # Modèles personnages (*_m.p3d, *_a.p3d)
│   │   ├── frontend/    # Menus (scrooby.p3d, BootScreen.p3d...)
│   │   ├── level/       # Niveaux (L1_TERRA.p3d, L1_Z1.p3d...)
│   │   └── global.p3d   # Textures globales
│   ├── audio/
│   │   ├── dialog/      # DIALOGF.RCF, dialog*.rcf
│   │   ├── music/       # musiqu00.RCF... music00.RCF...
│   │   ├── ambience/    # Ambience RCFs
│   │   ├── soundfx/     # Sound effects RCFs
│   │   ├── nis/         # Non-interactive sequence audio
│   │   ├── scripts/     # Scripts RCF
│   │   └── carsound/    # Sons de véhicules
│   └── movies/          # FMV (vidéos .rmv)
```

### Extraction depuis les CDs (PC)

Le jeu PC original est sur 3 CDs. Chaque CD contient des archives CAB (InstallShield) :

```bash
# 1. Monter ou extraire les ISOs
7z x CD1.iso -o/tmp/cd1

# 2. Extraire les archives CAB
# sudo apt install cabextract unshield   (si pas déjà installé)
cabextract /tmp/cd1/DATA1.CAB -d assets/
cabextract /tmp/cd2/DATA1.CAB -d assets/  # CD2
cabextract /tmp/cd3/DATA1.CAB -d assets/  # CD3

# 3. Copier les fichiers RCF des CDs (audio)
cp /tmp/cd1/DIALOGF.RCF assets/audio/dialog/
# ... etc pour chaque CD
```

---

## Étape 5 — Lancer

```bash
./build/bin/donut
```

Le jeu démarre avec ImGui debug UI. Les contrôles par défaut :
- **WASD** : Déplacer la caméra libre
- **Souris** : Orienter la caméra
- La barre de menu ImGui permet de charger des niveaux, personnages, etc.

---

## Étape 6 — Dépannage

### Erreur : "Could NOT find OpenAL"

```bash
sudo apt install libopenal-dev
```

### Erreur : "Could NOT find Bullet"

```bash
sudo apt install libbullet-dev
```

### Erreur : "Could NOT find fmt"

```bash
sudo apt install libfmt-dev
```

### Erreur OpenGL : "GLSL 4.30 is not supported"

Ton GPU ou driver ne supporte pas OpenGL 4.3. Essaie :

```bash
MESA_GL_VERSION_OVERRIDE=4.3 MESA_GLSL_VERSION_OVERRIDE=430 ./build/bin/donut
```

### Crash au lancement / segfault

Le crash le plus probable est un **fichier d'asset manquant**. Les chemins sont hardcodés dans `Game.cpp` :
- `art/frontend/scrooby.p3d` (police)
- `art/level/L1_TERRA.p3d` (niveau)
- `art/level/L1_Z1.p3d`, `L1_Z2.p3d` (zones)
- `art/chars/global.p3d` (textures globales)
- `art/chars/homer_m.p3d` (modèle Homer)

Vérifie que tous ces fichiers existent dans ton dossier `assets/`.

Pour lancer sans assets, édite `src/Game.cpp` pour commenter les chargements.

### Son qui ne marche pas

Vérifie qu'OpenAL fonctionne :
```bash
aplay /usr/share/sounds/alsa/Front_Center.wav   # test ALSA
openal-info                                        # test OpenAL
```
Si `openal-info` n'existe pas : `sudo apt install openal-info`

---

## Détails techniques

### Corrections déjà appliquées pour la compatibilité Linux

1. **`src/CMakeLists.txt`** : `donut.rc` et `donut.exe.manifest` (Windows) encapsulés dans `if(WIN32)`
2. **`src/Core/MemoryStream.h`** : ajout `#include <cstdint>` (requis par GCC, implicite sur MSVC)
3. **`src/Character.cpp`** : `fmt::print` avec `ChunkType` casté en `uint32_t` (fmt v9+ est plus strict)

### Codegen tool (Windows .NET)

`dev/codegen/donut-codegen.exe` est un outil Windows .NET. Il n'est nécessaire que si tu modifies `p3d.json` ou `cmd.json` (définitions de chunks P3D / commandes de script). Les fichiers générés sont déjà dans le repo. Sur Linux, tu peux l'exécuter avec mono :

```bash
sudo apt install mono-complete
mono dev/codegen/donut-codegen.exe --p3din dev/codegen/p3d.json --p3dout src/P3D \
  --cmdin dev/codegen/cmd.json --cmdout src/Scripting
```

### Dépendances développement supplémentaires (optionnel)

```bash
sudo apt install clang-format   # Formatage de code
# La cible cmake "clang-format" formate automatiquement tout src/
cmake --build build --target clang-format
```
