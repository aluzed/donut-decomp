# UI-LANG — Texte localisé via les language bibles

- **Status:** DONE — per the backlog: `--language E/F/G/I/S` for the game's own text; the language defaults to the dialogue's (2026-10-21)
- **Priority:** P2
- **Module:** UI
- **Depends on:** P3D-005, UI-TEXT
- **Files:** `src/P3D/P3DChunk.cpp:159-169` (`FrontendTextBible`, `FrontendStringTextBible`, `FrontendTextBibleResource`), `src/FrontendProject.cpp:105-154`, `src/ResourceManager.h`

## Contexte
Le jeu stocke les chaînes dans des "text bibles" par langue (`FrontendTextBible` / `FrontendStringTextBible` / `FrontendTextBibleResource`, déclarés `src/P3D/P3DChunk.cpp:159-169`). UI-TEXT résout déjà les chaînes via une bible ; il faut sélectionner la bible selon la langue active.

## Approche
- Une fois le parsing des chunks bible disponible (P3D-005), charger les `FrontendTextBible` dans un `TextBible` (map clé→chaîne) au sein du `ResourceManager` (cf. `AddFont` `src/FrontendProject.cpp:149`).
- Ajouter un enum `Language` + un sélecteur (config/CLI) ; charger la bible correspondante (les bibles sont séparées par langue dans les P3D frontend).
- Exposer `ResourceManager::GetString(key)` consommé par `AddMultiText` (UI-TEXT) et le HUD.
- Fallback : si clé absente dans la langue courante, retomber sur l'anglais et logguer (`Log::Warning`).

## Critères d'acceptation
- [ ] Les chaînes frontend proviennent de la bible de la langue active.
- [ ] Changer de langue change le texte affiché sans recompilation.
- [ ] Clé manquante → fallback anglais + warning loggé.

## One language, chosen in the options (2026-10-21)

Reported in play: the objectives were sometimes in French, sometimes in English ("Talk to
them"). The game's phrase table gives an objective only when its stage names one; the
others were the engine's own words, in English.

- **The engine's own words in the five languages.** `UI/GameText` holds them (the
  objectives' plain lines, in capitals as the game's are, the HUD's labels, the hints, the
  results, the pause, the options' rows), in Latin-1 as the game's fonts take them.
- **The language is an option.** The game settings page's last row, the steering wheel's
  (the PC has none), is the language's: Français, English, Deutsch, Italiano, Español, built
  like the subtitles' row above it. The choice is kept in `donut_settings.dat`
  (`language 70` for F) and used at the next start. The voices and their subtitles keep the
  CD's language.
- **The pages change with it.** Each page keeps its texts' keys, and a change of language
  takes them all again from the phrase table (`FrontendProject::RefreshAllTexts`).
- **No "???" any more.** The data leaves untranslated phrases as "???": the whole Italian
  column, and in French most of the PC's own terms in scrooby2's compiled bible. Both
  compiled bibles are loaded (scrooby's has those terms translated), "???" counts as no
  phrase, and the English is given for what a language lacks. The game settings page read
  "???" on five of its rows; it reads "Utiliser la vue souris", "Inverser souris X"...
