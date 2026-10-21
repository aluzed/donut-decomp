#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Game/SuperSprint.h"

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace Donut
{

class FrontendProject;

// The bonus game's screens (scrooby2's minigame.p3d):
// - its menu (MiniMenu.pag): each player joins (player 1 is in), picks who
//   to be among the five (Lisa, Bart, Homer, Marge, Apu: one each) and a car
//   among those they have, its stars beside it; once all are ready, the
//   track (those unlocked), the number of laps and the way round;
// - its pause (MiniPause.pag): Continue, Quit;
// - its standings (MiniSummary.pag), after each race: the racers in the
//   order of the series, their faces glad or sore as they placed, their
//   wins, points, total time and best lap, a crown on the best of each.
class MiniGameMenu
{
public:
	// One player's hands this frame: held directions, keys pressed now.
	struct Pad
	{
		bool connected = false;
		bool left = false, right = false, up = false, down = false;
		bool confirm = false, back = false, aux = false, start = false;
	};
	struct Car
	{
		std::string name;
		// Its stars, 0 to 9 (S0 to S9).
		int rating = 0;
	};

	MiniGameMenu();
	~MiniGameMenu();

	bool IsLoaded() const { return _menu != nullptr; }

	// The menu, from its start; `cars` those that may be picked, `tracks`
	// how many are unlocked (the first always).
	void OpenSetup(std::vector<Car> cars, int tracks);
	enum class Result
	{
		None,
		Start,
		Quit,
	};
	Result DrawSetup(const std::array<Pad, SuperSprint::kRacers>& pads, const Matrix4x4& proj);
	// What was picked, the computer driving those no one joined for.
	const SuperSprint::Setup& GetSetup() const { return _setup; }
	// Back at the track's choice, the players as they were (after a race).
	void ReturnToTracks();

	enum class PauseResult
	{
		None,
		Continue,
		Quit,
	};
	void OpenPause();
	PauseResult DrawPause(const Pad& pad, const Matrix4x4& proj);

	// A racer's line in the standings.
	struct Standing
	{
		std::string character;
		// The player (0 to 3), or -1 for the computer.
		int player = -1;
		// Its place in the race just run, 1 to 4, or 0 when it did not finish.
		int place = 0;
		int wins = 0;
		int points = 0;
		double totalTime = -1.0;
		double bestLap = -1.0;
	};
	// `standings` in the series' order.
	void OpenSummary(std::vector<Standing> standings);
	// True once it is gone on from.
	bool DrawSummary(const Pad& pad, const Matrix4x4& proj);

private:
	std::unique_ptr<FrontendProject> _menu, _pause, _summary;
	std::vector<Car> _cars;
	int _tracks = 1;
	SuperSprint::Setup _setup;
	enum class Stage
	{
		Out,
		Character,
		Vehicle,
		Ready,
	};
	std::array<Stage, SuperSprint::kRacers> _stages {};
	std::array<int, SuperSprint::kRacers> _character {};
	std::array<int, SuperSprint::kRacers> _vehicle {};
	bool _choosingTrack = false;
	int _track = 0;
	// What was held last frame, a player's left, right, up and down.
	std::array<std::array<bool, 4>, SuperSprint::kRacers> _held {};
	int _pauseChoice = 0;
	bool _pauseHeld[2] = {};
	bool _pauseShown = false;
	// The layout as the page has it: the players' tags and the track
	// cursor's pieces, before they are moved.
	struct Box
	{
		float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
	};
	std::array<Box, SuperSprint::kRacers> _tagBoxes, _tagTexts;
	std::array<Box, 5> _characterBoxes;
	std::vector<std::pair<std::string, Box>> _cursorBoxes;
	Box _lapsBox;
	// The standings' crowns (total time, best lap), on its first row.
	std::array<Box, 2> _crownBoxes;
	// The computer's cars, picked at random.
	std::mt19937 _random {std::random_device {}()};

	void layOut();
	bool characterTaken(int character, int by) const;
	void stepCharacter(int player, int step);
	void fillComputer();
};

} // namespace Donut
