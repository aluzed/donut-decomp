#pragma once

#include "Core/Math/Matrix4x4.h"

#include <memory>
#include <optional>
#include <string>

namespace Donut
{

class FrontendProject;
struct GameIntent;

// The game's scrapbook, from the main menu (scrooby2's ScrapBook.pag): Open
// Book, the contents (ScrapBookContents.pag: the story's missions, clothes,
// vehicles and cards, so many of each unlocked), and the game's stats
// (ScrapBookStats.pag). The galleries the contents lead to are not done.
class ScrapBookMenu
{
public:
	// What the save files say was done; none where they do not keep it.
	struct Stats
	{
		int storyMissions = 0, storyMissionsTotal = 0;
		int cards = 0, gags = 0, wasps = 0;
		std::optional<int> bonusMissions, streetRaces, clothing, vehicles;
	};

	ScrapBookMenu();
	~ScrapBookMenu();

	bool IsLoaded() const { return _hub != nullptr; }
	void Open(const Stats& stats);
	bool IsOpen() const { return _screen != Screen::Closed; }
	// Draws the page open and takes the keys and the mouse: up and down pick,
	// the confirm key opens, Escape goes back.
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

	enum class Screen
	{
		Closed,
		Hub,
		Contents,
		Stats,
	};
	Screen GetScreen() const { return _screen; }

private:
	std::unique_ptr<FrontendProject> _hub, _contents, _stats;
	Screen _screen = Screen::Closed;
	int _choice = 0;
	bool _upHeld = false, _downHeld = false;
	// Not the frame a page opens: the key that opened it would act on it.
	bool _shown = false;
	int _mouseX = -1, _mouseY = -1;
};

} // namespace Donut
