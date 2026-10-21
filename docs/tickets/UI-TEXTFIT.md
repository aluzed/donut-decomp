# UI-TEXTFIT — Mission objectives out of their frame

- **Status:** DONE (2026-10-21)
- **Priority:** P1 (reported in play: the objectives sometimes overflow their frame)
- **Module:** UI
- **Files:** `src/FrontendProject.{h,cpp}` (`SetTextWrapped`, `Draw`)

## Context
The HUD's objective box (`Message` in its frame `MessageBox`) breaks its text into lines
(`SetTextWrapped`). The words were measured with the font's own advances, but each line
was then drawn at the box's whole height, one under the other: a text of two or three
lines stood out of the frame by one or two.

## Fix
The text is broken into lines when drawn: from the size at which one line fills the box's
height, down by 8% at a time (to 35% at least), until the text broken at the box's width at
that size fits its height too. The block is placed in the box as the text's alignment
says, each line too, none wider than the box.

## The frame, not the page's text box (2026-10-21, reported again with a screenshot)

The first fix kept to the page's own text box, and that box is not the frame's inside:
`Message` is one line, 280 wide from x 160 and 16 high, while its frame `MessageBox` spans
x 90 to 420 and y 340 to 428. Text ran 20 past the frame's right end ("RENTREZ DANS LE CAMION
DE BEIGNETS ET RAMASSEZ" over its cap), and two lines squeezed into 16 came out 8 high. The
text box is now the frame's inside: from the icon to 22 short of the frame's right end, its
height less 8 above and below, the lines centred, each at most the page's 16 high
(`FrontendProject::SetTextBox`). A short objective keeps the game's size on one line; the
longest take three or four lines, all inside.

## Verification
The longest objective in French (88 characters, `MISSION_OBJECTIVE_195`, "VOUS DEVEZ ACHETER
DES VETEMENTS COOLS DANS LE DONJON DE L'ANDROIDE OU A L'OBSERVATOIRE.") shown in the HUD:
two lines inside the frame.
