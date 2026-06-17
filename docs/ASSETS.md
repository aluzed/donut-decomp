# Game Assets

donut requires the original game files from *The Simpsons: Hit & Run* (PC).

## Directory structure

Place extracted game files in `assets/` at the project root:

```
assets/
├── art/
│   ├── chars/
│   │   ├── *_m.p3d          # Character models
│   │   ├── *__a.p3d         # Character animations
│   │   └── global.p3d       # Global textures
│   ├── cars/
│   │   └── *.p3d            # Vehicle models
│   ├── frontend/
│   │   └── scrooby2/
│   │       └── resource/
│   │           ├── fonts/   # Font P3D files
│   │           └── images/  # Frontend images
│   └── level/
│       ├── L1_TERRA.p3d     # Level 1 terrain
│       ├── l1z1.p3d         # Zone 1
│       └── ...
├── audio/
│   ├── dialog/
│   │   └── *.rcf            # Dialogue archives
│   ├── music/
│   │   └── *.rcf            # Music archives
│   ├── ambience/
│   ├── soundfx/
│   ├── nis/
│   ├── scripts/
│   └── carsound/
├── movies/
│   └── *.rmv                # FMV videos (not yet supported)
└── shaders/                 # Included in repo
```

## Extraction from original CDs

The PC version ships on 3 CDs with InstallShield CAB archives.

### Linux

```bash
# Install tools
sudo apt install cabextract p7zip-full

# Extract each CD ISO
7z x files/CD1.7z -o/tmp/cd1
7z x files/CD2.7z -o/tmp/cd2
7z x files/CD3.7z -o/tmp/cd3

# Extract game data from CABs
cabextract /tmp/cd1/DATA1.CAB -d assets/
cabextract /tmp/cd2/DATA1.CAB -d assets/
cabextract /tmp/cd3/DATA1.CAB -d assets/

# Copy RCF audio files from CD root
cp /tmp/cd1/DIALOGF.RCF assets/audio/dialog/

# Copy movies
cp /tmp/cd1/MOVIES/*.RMV assets/movies/
```

### Windows

Use tools like 7-Zip, WinRAR or ISOBuster to extract CAB archives.

## Loading

The engine loads files from paths relative to the working directory. Run from the project root:

```bash
./build/bin/donut
```

Paths are hardcoded in `src/Game.cpp`. Adjust as needed for your directory layout.
