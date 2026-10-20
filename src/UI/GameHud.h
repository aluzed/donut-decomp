#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector2.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Font;
class Mesh;
class FrontendProject;
class SpriteBatch;
class Texture;
struct HudState;

// The game's own HUD (scrooby2's ingame.p3d, Hud.pag and Coins.pag), filled
// from a HudState:
// - the radar, bottom right: the player in its middle, what lies about them
//   as the page's icons (the mission's cars, the police, the objective's flag
//   or a race's checkered flags, phone booths, shops, bonus missions, street
//   races, the player's own car when on foot), the roads as dots; round it
//   the Hit & Run meter, its eight sectors lit as the meter fills; under it
//   the speed, in the car;
// - the counters, top left, one under the other: the stage's clock, the place
//   in a race, its laps, the car's damage; the coins, top right, for a while
//   after they change;
// - the objective, bottom left, its box and icon, for a few seconds after it
//   changes; the countdown in the middle.
// The counters and the countdown are drawn in the page's letters (images of
// digits and capitals, `letters/*.png`), as the game draws them.
class GameHud
{
public:
	GameHud();
	~GameHud();

	bool IsLoaded() const { return _page != nullptr; }
	// Whether the objective's box is up (low on the screen, where the
	// subtitles would be).
	bool IsObjectiveShown() const { return _objectiveFor > 0.0; }
	// `dt` times what is shown for a while.
	void Draw(const HudState& state, const Matrix4x4& proj, double dt);
	// The radar's map of `level` (art/frontend/scrooby2/resource/pure3d/
	// l<level>hudmap.p3d: its roads and crossings, flat, in the world's
	// coordinates).
	void LoadMap(int level);

private:
	std::unique_ptr<FrontendProject> _page, _coins;
	std::unique_ptr<Texture> _white;
	// What is shown for a while after it changes, and for how much longer.
	std::string _objective;
	double _objectiveFor = 0.0;
	int _coinCount = -1;
	double _coinsFor = 0.0;

	// The map, drawn round the player into a texture with the radar's disc
	// under it, cut round, and given to the radar's sprite.
	std::vector<std::unique_ptr<Mesh>> _map;
	std::unique_ptr<Texture> _radarMap;
	unsigned int _radarFrameBuffer = 0;
	void drawMap(const HudState& state);
	void hideAll();
	void layOutRadar(const HudState& state);
	// `text` in the page's letters, `height` page pixels high, its left at
	// `at` (page coordinates), or centred on it.
	void drawLetters(SpriteBatch& batch, const FrontendProject& page, const std::string& text, Vector2 at, float height,
	                 bool centred);
};

} // namespace Donut
