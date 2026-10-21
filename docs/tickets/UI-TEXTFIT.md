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

## Verification
The longest objective in French (88 characters, `MISSION_OBJECTIVE_195`, "VOUS DEVEZ ACHETER
DES VETEMENTS COOLS DANS LE DONJON DE L'ANDROIDE OU A L'OBSERVATOIRE.") shown in the HUD:
two lines inside the frame.
