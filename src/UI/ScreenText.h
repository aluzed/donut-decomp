#pragma once

#include "Core/Math/Vector4.h"
#include "Input/Input.h"

#include <string>
#include <vector>

namespace Donut
{

class Font;
class GameMenu;
class SpriteBatch;

// The 2D drawing the game does itself, outside its pages: the plain menus, the
// mission's result, the hints. The 2D projection is MakeOrtho(0, w, h, 0): text
// is placed by its top left corner, y down.

// Where a label starts to stand centred across the viewport.
float CentredTextX(const Font* font, const std::string& text, float viewportWidth);

// The label of the input bound to `action`, a gamepad's when one is in use
// (the HUD's hints match the device), from the live keymap.
std::string ControlLabel(GameAction action, bool preferGamepad);

// A plain menu: its title, then its buttons one under the other, centred, the
// one under the mouse in `hover`. Lays the buttons out where they are drawn and
// updates their hover; the click is the caller's.
void DrawTextMenu(SpriteBatch& sprites, const Font* font, GameMenu& menu, const std::string& title,
                  const Vector4& titleColour, const Vector4& idle, const Vector4& hover, int viewportWidth,
                  int viewportHeight, int mouseX, int mouseY);

// The mission's result over the game: complete (its time, its best, a new
// record, the level done) or failed (why, the game's advice), and when the
// next one comes.
struct MissionResult
{
	bool complete = true;
	bool newRecord = false;
	// The game's words for the title and the level's end, or the engine's.
	std::string title;
	std::string levelComplete; // empty unless the level is done
	float elapsed = 0.0f;
	float best = 999.0f;
	std::string reason, hint;
	int nextIn = 0;
	char language = 'E';
};
void DrawMissionResult(SpriteBatch& sprites, const Font* font, const MissionResult& result, int viewportWidth,
                       int viewportHeight);

// The hints down the left side, a third of the way down, and the frame rate.
void DrawHints(SpriteBatch& sprites, const Font* font, const std::vector<std::string>& hints, const Vector4& colour,
               int fps, int viewportWidth, int viewportHeight);

} // namespace Donut
