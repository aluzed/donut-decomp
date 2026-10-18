# LEVEL-TRIG — Câbler les volumes de trigger parsés vers les callbacks de script

- **Status:** PARTIAL (2026-10-18)
- **Priority:** P1
- **Module:** Level
- **Depends on:** REGR-001
- **Files:** `src/Level.cpp:177-199` (parsing), `src/Level.cpp:280-299` (`CheckTrigger`), `src/Game.cpp:698`, `src/Game.cpp:695-723`, `src/Scripting/GameCommands.cpp`

## Contexte
Les volumes de trigger (`Trigger { name, position, bounds, isRect }`) sont parsés et stockés dans `_triggers` (`Level.cpp:177-199`). `CheckTrigger` (`Level.cpp:280-299`) ne fait qu'un test booléen ponctuel, et son seul appel (`Game.cpp:698`) est dans le bloc gameplay désactivé (`Game.cpp:695-723`, cf. REGR-001). Aucun callback `ActivateTrigger`/`DeactivateTrigger` n'est émis.

## Approche
1. Ajouter un suivi d'état par trigger dans `Level` : mémoriser pour chaque `Trigger` si le joueur est actuellement à l'intérieur (set/flag), afin de détecter les transitions entrée/sortie plutôt qu'un état permanent.
2. Faire de `Level::CheckTrigger` (ou une nouvelle `Level::UpdateTriggers(pos)`) un émetteur de transitions : sur entrée → `ActivateTrigger(name)`, sur sortie → `DeactivateTrigger(name)`, en réutilisant le test rect/sphère existant (`Level.cpp:288-298`).
3. Exposer ces callbacks vers le scripting (`Scripting/GameCommands.cpp`) pour que les scripts s'abonnent par nom de trigger.
4. Réactiver et brancher l'appel par-frame dans `Game.cpp:695-723` (dépend de REGR-001) en remplaçant l'appel booléen `Game.cpp:698`.

## Done (2026-10-18)

- `Level::UpdateTriggers(pos)` keeps an inside/outside flag per volume and returns only the
  transitions. `Game` feeds it the player's position (the car's, when driving) and hands
  each event to `ScriptEngine::OnTrigger`. Over a 140s M1race run: **7** named events, in
  place of an anonymous "player entered trigger zone" every 2s.
- Box volumes are tested **in their own frame**. The old test compared world-axis offsets,
  which is wrong for turned boxes, and L1 has them: `loaderTrigger1` at 90 degrees,
  `loader3Trigger` at 45.
- Each trigger now knows its locator, the locator's type and the text at the head of its
  data. The type values, checked against L1, follow the game's enum. On L1, with or
  without triggers:

  | type | | count | data |
  |---|---|---|---|
  | 0 | event (cameras, mission spots) | 163 | — |
  | 1 | script | 11 | name (`tire_fire`, `toxic_ooze_01`) |
  | 3 | car start | 248 | — (no trigger) |
  | 5 | dynamic zone | 19 | DynaLoad string (`l1z3.p3d:l1z4.p3d:...`) |
  | 6 | occlusion | 9 | — |
  | 7 | interior entrance | 3 | interior (`SimpsonsHouse`, `KwikEMart`) |
  | 9 | action | 66 | — |
  | 13 | ped group | 16 | — |
  | 14 | coin | 177 | — (no trigger) |

Not yet: nothing *acts* on the events. The dynamic-zone loaders are what LEVEL-STREAM
needs, but unloading a region removes its collision, and with it the ground under any
car still there. That deserves its own step. Interior entrances are GAME-014's.

## Critères d'acceptation
- [x] L'entrée dans un volume émet un événement une seule fois (pas chaque frame) — `ScriptEngine::OnTrigger`.
- [x] La sortie émet un événement de sortie.
- [ ] Les scripts peuvent s'abonner à un trigger par son nom.
- [x] Les volumes rect (orientés) et sphère sont gérés.
- [x] L'appel par-frame est actif.
