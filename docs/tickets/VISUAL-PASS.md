# VISUAL-PASS — What a look round the levels found (2026-10-21/22)

- **Status:** DONE (2026-10-22)
- **Priority:** —
- **Module:** Render / UI / Scripting
- **Files:** see each item's commit

## How
Every level walked through, the player put on twelve road nodes spread over it with a
camera turned a different way at each, and each level's story played under the
autopilot with a picture every 10 to 20 seconds; then the menus, the loading screen, a
street race. What looked wrong was traced to its cause in the data or the executable.

## Found and fixed
| What was seen | Cause | Commit |
|---|---|---|
| The town's cars dark on top, lit from below | A level's light groups were made of lights by name across every file read; the Simpsons' house (l1i02), read after the terrain, names one of its lights as the sun does, shining up | `736f1ff` |
| Dark halos and smoke drawn with whatever blending came before | Only alpha and additive blending; 196 shaders use mode 3, reverse subtract (`pddidx8r.dll`'s blend table, `0x1005d600`) | `466c8dd` |
| The cars' paint flat | "spheremap" and "environment" shaders: their sphere map's reflection (REFL, ENVB) not added | `ac3ae37`, `4bc0f68` |
| Every instanced tree's bark striped red and green, a sign in a blue-purple gradient | The instanced vertex layout read the colour 5 floats in, on the lightmap UV | `01b6290` |
| Every tree's shadow a white patch | Shaders and textures kept by name, the last loaded winning: the living room's white "treeshadow" | `dffa0af` |
| After the movies, every bootup page at once (language list, memory card, quit prompt) | The splash drew the whole bootup project; the PC game has no title screen | `2ac6acc` |
| "Time" and "Record" overlapping on the result screen | Fixed 20-pixel offsets in a taller font | `6989b6e` |
| The characters seemed to float; no shadow under the cars | The game's ground shadows were not drawn | `86c3cad` |
| The school and the living room opening onto the sky | The town's sky drawn round the cutaway interiors | `c63ec95` |
| Long objectives touching their frame's rim | Too small a margin for the rim and the cap | `04f15de` |
| Level 1 m0's "talk to Apu" skipped, no marker | The objective looked only among the mission's own characters; Apu is the level's | `d0e42f5` |
| Dr Hibbert's coat buttonless (Beeman's wings missing) | A character's rigid props on its joints were skipped | `a11b0f1` |
| German and French titles in quotes, ""LARD LAD'S"" | The text bible's spreadsheet quoting not undone | `4f85a6e` |
| "Appuyez sur E pour monter" lost on pale walls | Mid grey, no shadow | `2aa23e2` |
| The givers and car sellers standing still, bonus missions starting with no word | Their rounds and their conversation not read | `4ec8be7`, `349288e` |
| Passers-by walking silently | Only the player's steps were heard | `7144d2e` |
| Warnings for what is as it should be | Drives with no file of their own, "npd" the voice-only character, resources loaded again | `be44d4c`, `fddb4d9` |

## Seen and left
- `A_Grave.png` (the HUD's capital A grave): named by the page, not on the CD.
- Two meshes of 12 and 3 vertices on "lambert1", Maya's default shader, which the files do
  not hold: export leftovers, not drawn.
- Level 3 m3's car sometimes wrecked under the autopilot (it ramming at speed): the
  previous binaries did the same.
