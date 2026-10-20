#pragma once

#include "Core/Math/Matrix4x4.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

class FrontendProject;
struct GameIntent;

// The pause screen between missions, driving about the level (scrooby2's
// PauseSunday.pag): Continue, Mission Select (MissionSelect.pag: the level's
// seven missions, those won to play again), Level Progress (LevelStats.pag:
// the level's counts), View Cards (PauseViewCards.pag: the level's seven
// cards, those found), Options, Save Game and Quit Game. Up and down pick,
// the confirm key opens, Escape goes back or resumes.
class PauseSundayMenu
{
public:
	struct Mission
	{
		bool won = false;
		// Its place in the level's story, to play it again; -1 when none.
		int story = -1;
	};
	// The level's counts, as "won/all" texts, by the page's names
	// ("StoryMissions_Unlocked", "Cards_Unlocked"...), and how much of the
	// level is done, 0 to 100.
	struct Stats
	{
		std::vector<std::pair<std::string, std::string>> counts;
		int percent = 0;
	};
	struct Actions
	{
		std::function<void()> resume, options, quit;
		// Plays the mission at that place in the story again.
		std::function<void(int)> replay;
	};

	explicit PauseSundayMenu(Actions actions);
	~PauseSundayMenu();

	bool IsLoaded() const { return _pause != nullptr; }
	// `missions` m1 to m7; `cards` the level's seven, each its place in the
	// game's deck if found, else -1.
	void Open(int level, std::vector<Mission> missions, Stats stats, std::vector<int> cards);
	bool IsOpen() const { return _screen != Screen::Closed; }
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

private:
	enum class Screen
	{
		Closed,
		Pause,
		MissionSelect,
		LevelStats,
		ViewCards,
	};
	std::unique_ptr<FrontendProject> _pause, _missionSelect, _levelStats, _viewCards;
	Actions _actions;
	Screen _screen = Screen::Closed;
	int _level = 1;
	std::vector<Mission> _missions;
	std::vector<int> _cards;
	int _choice = 0;
	bool _shown = false, _upHeld = false, _downHeld = false, _leftHeld = false, _rightHeld = false;
	int _mouseX = -1, _mouseY = -1;
	// Mission select's highlight bar, at its first row.
	float _barX = 0.0f, _barY = 0.0f, _barWidth = 0.0f, _barHeight = 0.0f;
	void drawPause(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm);
	void drawMissionSelect(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm);
	void drawViewCards(const GameIntent& intent, const Matrix4x4& proj, bool back);
	void layOutCards();
	// Up and down (and the mouse) among `count` rows named `name(i)`, those
	// `allowed` only; the row clicked, or -1.
	int pick(FrontendProject& page, int count, const std::function<std::string(int)>& name,
	         const std::function<bool(int)>& allowed, const GameIntent& intent);
};

} // namespace Donut
