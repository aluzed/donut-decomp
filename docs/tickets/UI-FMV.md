# UI-FMV — Lecteur de cinématiques RMV (Radical Movie)

- **Status:** TODO
- **Priority:** P3
- **Module:** UI
- **Depends on:** TOOL-002
- **Files:** `tools/extract_assets.sh:54-60` (copie `MOVIES/*.RMV`), `src/Render/Texture.h`, `src/Render/SpriteBatch.h`

## Contexte
Les cinématiques sont au format propriétaire `.RMV` (Radical Movie), extraites dans `movies/` par `tools/extract_assets.sh:54-60`. Aucun lecteur n'existe. Le format doit être rétro-ingénié (TOOL-002, reverse SIMPSONS.EXE) ou rattaché à un codec connu (Bink, vu `binkw32.dll` mentionné `tools/extract_cab.sh:37`).

## Approche
- Identifier le conteneur RMV : inspecter l'en-tête d'un `.RMV` ; vérifier si c'est du Bink (présence de `binkw32.dll` `tools/extract_cab.sh:37`) ou un format radical custom (croiser avec le reverse SIMPSONS.EXE, TOOL-002).
- Si Bink : envisager `libavcodec`/ffmpeg (Bink décodable) pour démuxer/décoder en frames + audio.
- Si custom : documenter la structure (chunks, frames, palette/codec) et écrire un décodeur minimal.
- Implémenter `class MoviePlayer` : décode frame → upload `Texture` → blit plein écran via `SpriteBatch::Draw`, synchro audio, skip touche.
- Ajouter `GameState::Movie` déclenché aux cinématiques.

## Critères d'acceptation
- [ ] Format RMV documenté (codec/conteneur identifié).
- [ ] Au moins une cinématique se décode et s'affiche en vidéo plein écran.
- [ ] Audio synchronisé (ou stub documenté si bloquant).
- [ ] Touche de skip fonctionnelle.
