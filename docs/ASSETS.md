# Game Assets

donut requires the original game files from *The Simpsons: Hit & Run* (PC).

## Directory structure

The engine reads `art/` at the project root (`audio` and `shaders` are
symlinks into it). **Nothing under `art/` ships with the repository** -- it all
comes off your own CDs and the whole tree is gitignored. Only the shaders, in
`assets/shaders/`, are ours:

```
art/
├── chars/
│   ├── *_m.p3d              # Character models
│   ├── *_a.p3d              # Character animations
│   ├── *.cho                # Character collision
│   └── global.p3d           # Global textures
├── cars/
│   └── *.p3d                # Vehicle models
├── L1_TERRA.p3d             # Level 1 terrain
├── l1z1.p3d, l1r1.p3d, ...  # Level 1 zones and roads
├── frontend/                # Menus and HUD
│   └── scrooby2/resource/{fonts,images}/
├── missions/                # Mission props and cameras
│   └── level01/ ... level08/
└── audio/                   # Scanned recursively for .rcf
    ├── dialog/DIALOGF.RCF   # extracted from the CD1 root
    ├── music/*.rcf
    └── ambience.rcf, soundfx.rcf, carsound.rcf, nis.rcf, scripts.rcf

scripts/Missions/level01/*.con   # Mission scripts       (in the repo)
scripts/cars/*.con               # Car tuning scripts    (off the CD, gitignored)
scripts/level01/*.mfk            # Level 1 missions      (off the CD, gitignored)
assets/shaders/                  # Shaders, via the `shaders` symlink (in the repo)
```

In a fresh clone `art/` does not exist yet, so the `audio` symlink dangles
until you extract. That is harmless -- the engine logs `directory 'audio' not
found, skipping scan` and runs on without sound.

## Extraction from original CDs

The PC version ships on 3 CDs with InstallShield CAB archives.

### Linux

One command, from the directory holding the three CD images (the `.iso` files, or
the `.7z` archives they come in, at any depth):

```bash
sudo apt install genisoimage      # isoinfo; unshield and 7z are fetched if missing
tools/extract_assets.sh files/    # into art/ and scripts/ at the project root
tools/extract_assets.sh files/ /some/other/dir   # or elsewhere
```

It does what the steps below do, in about ten seconds. Two files do not come out of
these CDs with unshield (`card38_t.p3d`, `music02.rcf`): it says so, and the game runs
without them. `music02.rcf` (Sunday Drive, the Power Plant, the Kwik-E-Mart's muzak,
the main theme...) is stored whole across two cabinets, the end of `DATA3.CAB` and the
start of `DATA4.CAB`, and unshield looks for all of it in the first: the script puts it
back together from where unshield's log says each part starts (byte for byte: its 139
RSD headers all fall on 2048-byte boundaries, either side of the join).

The steps, by hand:

The three cabinets are **InstallShield** archives, not Microsoft CAB: cabextract
refuses them and unshield is the tool that reads them. The whole set is driven by
DATA1.HDR, so every DATA*.CAB has to sit in one directory next to it before
extracting -- unshield follows the volumes itself.

```bash
# Install tools
sudo apt install unshield p7zip-full genisoimage

# Each .7z holds a CD image, not the files themselves
7z x jeu-*cd1-pcwin.7z -ofiles/
7z x jeu-*cd2-pcwin.7z -ofiles/
7z x jeu-*cd3-pcwin.7z -ofiles/

# Pull the cabinets out of the ISOs into a single directory.
# CD1 carries DATA1.CAB + DATA1.HDR + DATA2.CAB, CD2 DATA3.CAB, CD3 DATA4.CAB.
mkdir -p files/cabs
iso() { echo files/*cd$1-pcwin/"Hit & Run $1.iso"; }
for f in DATA1.CAB DATA1.HDR DATA2.CAB; do
  isoinfo -i "$(iso 1)" -x "/$f;1" > "files/cabs/$f"
done
isoinfo -i "$(iso 2)" -x "/DATA3.CAB;1" > files/cabs/DATA3.CAB
isoinfo -i "$(iso 3)" -x "/DATA4.CAB;1" > files/cabs/DATA4.CAB

# One command extracts all four volumes
unshield -d files/extracted x files/cabs/DATA1.CAB

# Place what the engine reads. The whole art/ tree is gitignored, so this
# copies the models, the menus and the missions in one go.
mkdir -p art/audio/music
cp -a files/extracted/Art/.              art/
cp -a files/extracted/Sound/*.rcf        art/audio/
cp -a files/extracted/Sound/sound        art/audio/
cp -a files/extracted/Music1/*.rcf files/extracted/Music2/*.rcf art/audio/music/

# Each car's tuning (mass, engine, top speed). Also gitignored. Without these
# every car drives on the same defaults, and the engine warns once per car.
mkdir -p scripts/cars
cp -a files/extracted/Scripts/cars/*.con scripts/cars/

# The game's own missions, for "New Game", --story and --level. Gitignored too.
for n in 1 2 3 4 5 6 7; do
  cp -a files/extracted/Scripts/missions/level0$n scripts/level0$n
done
# What every car and outfit costs, and who sells it (BindReward).
cp -a files/extracted/Scripts/missions/rewards.mfk scripts/rewards.mfk

# DIALOGF.RCF sits in the clear on CD1, outside the cabinets
mkdir -p art/audio/dialog
isoinfo -i "$(iso 1)" -x "/DIALOGF.RCF;1" > art/audio/dialog/DIALOGF.RCF
```

### Windows

Use tools like 7-Zip, WinRAR or ISOBuster to extract CAB archives.

## Loading

The engine loads files from paths relative to the working directory. Run from the project root:

```bash
./build/bin/donut
```

Paths are hardcoded in `src/Game.cpp`. Adjust as needed for your directory layout.
