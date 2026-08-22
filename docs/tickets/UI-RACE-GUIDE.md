# UI-RACE-GUIDE — Repérer le prochain checkpoint

- **Status:** DONE (2026-08-22)
- **Priority:** P1
- **Module:** UI / Game
- **Depends on:** UI-HUD, VEH-SINK
- **Blocks:** le dernier critère de `VEH-SINK` (franchir un checkpoint)
- **Files:** `src/UI/Hud.h/.cpp`, `src/Game.cpp`

## Contexte

Après `VEH-SINK`, la voiture roule. Mais rien à l'écran n'indiquait **où aller** : les
6 checkpoints du circuit n'existaient visuellement que dans le bloc `if (_showDebug)`,
derrière la touche `1`. Impossible de terminer une course sans activer le mode debug.

## Réalisation

- Les piliers de checkpoint et les pointillés reliant le circuit sortent du bloc debug : ils
  s'affichent dès qu'une mission est active. Ce sont des repères de jeu, pas du debug. Le
  code de couleur existant est conservé — jaune pour la cible, vert pour les franchis, rouge
  pour les suivants. Le squelette du personnage et la flèche vers le véhicule restent, eux,
  sous `_showDebug`.
- Radar : tous les checkpoints en blip blanc discret, **le prochain en doré et plus gros**,
  dessiné en dernier pour rester lisible par-dessus les autres.
- Bandeau mission : « Next checkpoint: N m », mis à jour depuis la position du véhicule quand
  on conduit, du personnage sinon.

## Vérification

Course lancée, en voiture : « Next checkpoint: 92m » puis 90 m puis 89 m en roulant, blips
présents au radar avec la cible dorée distincte.

## Limite assumée

**Le franchissement d'un checkpoint n'a pas été démontré.** Piloter la voiture à distance
jusqu'à un rayon de 5 m à travers Springfield demande une navigation manuelle que je n'ai pas
réussie en un temps raisonnable ; la voiture s'est arrêtée contre un bâtiment. Le guidage est
en place et la distance décroît bien, mais le critère « franchir un checkpoint incrémente
`CP n/6` » de `VEH-SINK` reste **non validé**.

Reste donc à confirmer, idéalement à la main :

- [ ] Rouler jusqu'au repère doré incrémente `CP 1/6` et le repère passe au checkpoint suivant.
