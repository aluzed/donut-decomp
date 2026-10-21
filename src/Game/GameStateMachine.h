#pragma once

#include <initializer_list>

namespace Donut
{

enum class GameState
{
	Splash,
	MainMenu,
	Loading,
	InGame,
	Paused,
	MissionComplete,
	MissionFailed,
	Credits,
	// One of the game's movies, from the options' View Movies.
	Movie,
	// The phone booth's car select, over the game standing still.
	PhoneBooth,
	// A shop (cars or clothes), likewise.
	Shop,
	// The level's end, its counts, before the next level.
	LevelEnd,
	// The bonus game: its menu, a race, its standings.
	MiniGame
};

// What moves the game from one state to another.
enum class GameEvent
{
	Play,           // into the game: started, resumed, a shop or booth closed, a movie over
	Pause,          // the pause screen
	MissionWon,     // a mission's result, over the game
	MissionLost,
	ResultOver,     // the result's five seconds up: back to the game
	OpenShop,       // a shop's or the phone booth's page over the game
	OpenPhoneBooth,
	LevelOver,      // the level's end and its counts
	ShowCredits,    // the credits roll
	ToMainMenu,
	PlayMovie,
	OpenMiniGame,
	ShowSplash,     // the title screen, after the logos
};

const char* ToString(GameState state);
const char* ToString(GameEvent event);

// The game's states and how it goes from one to another, from one table: each
// event, the states it may come from, the one it goes to. An event fired from a
// state the table does not list is refused, and logged. Its result screen's
// clock is here too.
class GameStateMachine
{
public:
	explicit GameStateMachine(GameState start = GameState::Splash): _state(start) {}
	// Where the game starts: no transition.
	void Start(GameState state) { _state = state; _resultTime = 0.0; }

	GameState GetState() const { return _state; }
	bool Is(GameState state) const { return _state == state; }
	bool IsAny(std::initializer_list<GameState> states) const;

	// Moves on as the table says; false (and logged) when it does not allow it.
	bool Fire(GameEvent event);

	// The result screen's clock: true once, when its five seconds are up.
	bool UpdateResult(double dt);
	// How long the result has shown, seconds.
	double GetResultTime() const { return _resultTime; }
	static constexpr double kResultSeconds = 5.0;

private:
	GameState _state;
	double _resultTime = 0.0;
};

} // namespace Donut
