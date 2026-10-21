#include "Game/GameStateMachine.h"

#include "Core/Log.h"

#include <algorithm>
#include <vector>

namespace Donut
{

namespace
{
struct Transition
{
	GameEvent event;
	std::vector<GameState> from;
	GameState to;
};

using S = GameState;
using E = GameEvent;
// Every way between the states. The overlays (the pause, a shop, the phone
// booth) come from the game and go back to it; the results come from the game
// and go back when their time is up; the menus, the movies, the credits and the
// bonus game reach one another.
const std::vector<Transition> kTable = {
    {E::Play, {S::Splash, S::MainMenu, S::Loading, S::Paused, S::Shop, S::PhoneBooth, S::LevelEnd, S::Movie,
               S::MiniGame, S::InGame, S::MissionComplete, S::MissionFailed}, S::InGame},
    {E::Pause, {S::InGame}, S::Paused},
    {E::MissionWon, {S::InGame}, S::MissionComplete},
    {E::MissionLost, {S::InGame}, S::MissionFailed},
    {E::ResultOver, {S::MissionComplete, S::MissionFailed}, S::InGame},
    {E::OpenShop, {S::InGame}, S::Shop},
    {E::OpenPhoneBooth, {S::InGame}, S::PhoneBooth},
    {E::LevelOver, {S::InGame, S::MissionComplete}, S::LevelEnd},
    {E::ShowCredits, {S::InGame, S::MissionComplete, S::MainMenu, S::LevelEnd, S::Paused}, S::Credits},
    {E::ToMainMenu, {S::Splash, S::Credits, S::Movie, S::MiniGame, S::InGame, S::MissionComplete, S::LevelEnd,
                     S::Paused, S::MainMenu}, S::MainMenu},
    {E::PlayMovie, {S::MainMenu, S::InGame, S::Splash, S::Movie, S::LevelEnd}, S::Movie},
    {E::OpenMiniGame, {S::MainMenu, S::Splash, S::InGame}, S::MiniGame},
    {E::ShowSplash, {S::Movie, S::Splash}, S::Splash},
};
} // namespace

const char* ToString(GameState state)
{
	static const char* const kNames[] = {"Splash", "MainMenu", "Loading", "InGame", "Paused", "MissionComplete",
	                                     "MissionFailed", "Credits", "Movie", "PhoneBooth", "Shop", "LevelEnd",
	                                     "MiniGame"};
	return kNames[static_cast<int>(state)];
}

const char* ToString(GameEvent event)
{
	static const char* const kNames[] = {"Play",         "Pause",          "MissionWon", "MissionLost",
	                                     "ResultOver",   "OpenShop",       "OpenPhoneBooth", "LevelOver",
	                                     "ShowCredits",  "ToMainMenu",     "PlayMovie",  "OpenMiniGame",
	                                     "ShowSplash"};
	return kNames[static_cast<int>(event)];
}

bool GameStateMachine::IsAny(std::initializer_list<GameState> states) const
{
	return std::find(states.begin(), states.end(), _state) != states.end();
}

bool GameStateMachine::Fire(GameEvent event)
{
	for (const Transition& transition : kTable)
		if (transition.event == event)
		{
			if (std::find(transition.from.begin(), transition.from.end(), _state) == transition.from.end())
				break;
			if (_state != transition.to)
				Log::Debug("Game: {} -> {} ({})", ToString(_state), ToString(transition.to), ToString(event));
			_state = transition.to;
			// A result's clock starts with it.
			_resultTime = 0.0;
			return true;
		}
	Log::Warn("Game: {} refused in state {}", ToString(event), ToString(_state));
	return false;
}

bool GameStateMachine::UpdateResult(double dt)
{
	if (!IsAny({GameState::MissionComplete, GameState::MissionFailed}))
		return false;
	_resultTime += dt;
	return _resultTime > kResultSeconds;
}

} // namespace Donut
