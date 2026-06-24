# AUDIO-006 — Auto-scan du répertoire `audio/`

- **Status:** DONE
- **Priority:** P3
- **Module:** Audio
- **Depends on:** —
- **Files:** `src/Audio/AudioManager.cpp` (constructeur AudioManager), `src/Audio/AudioManager.h:41,90` (LoadRCF, discoverFiles)

## Contexte
Le constructeur d'`AudioManager` charge une liste de ~10 noms de fichiers RCF codés en dur. Tout nouveau fichier audio déposé dans `audio/` est ignoré tant qu'on ne modifie pas le code.

## Approche
- Implémenter/utiliser `discoverFiles(directory, extension)` (déclarée `AudioManager.h:90`) pour énumérer le répertoire `audio/` via `FileSystem`.
- Dans le constructeur, remplacer la liste codée en dur par un scan des `*.rcf` (et extensions audio pertinentes) puis appeler `LoadRCF` (`AudioManager.h:41`) pour chacun.
- Gérer proprement répertoire absent / liste vide (log d'avertissement, pas de crash) et éviter les doubles chargements.

## Critères d'acceptation
- [x] Tous les RCF présents dans `audio/` sont chargés automatiquement, sans liste codée en dur.
- [x] Un nouveau fichier RCF déposé dans `audio/` est pris en compte au démarrage sans modification du code.
- [x] Un répertoire `audio/` vide ou absent ne provoque pas de crash.
