# LEVEL-TRIG — Câbler les volumes de trigger parsés vers les callbacks de script

- **Status:** TODO
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

## Critères d'acceptation
- [ ] L'entrée dans un volume émet `ActivateTrigger(name)` une seule fois (pas chaque frame).
- [ ] La sortie émet `DeactivateTrigger(name)`.
- [ ] Les scripts peuvent s'abonner à un trigger par son nom.
- [ ] Les volumes rect et sphère sont gérés (réutilisation du test existant).
- [ ] L'appel par-frame est actif (bloc `Game.cpp:695-723` réactivé via REGR-001).
