#pragma once

#include "Core/Math/Vector3.h"

#include <memory>
#include <string>

namespace Donut
{

class PathGraph;
class RaceOpponent;
class Vehicle;
class WorldPhysics;

// The game's Hit & Run: a meter that fills as the player runs people down and
// rams the traffic, and drains on its own. Full, it sends a police car after
// the player; caught standing, the player is busted. Get far enough away, or
// let the meter drain, and the police give up.
class HitAndRun
{
public:
	// `copModel` is the level's chase car (CreateChaseManager's first argument,
	// "cPolice"); `decay` the meter drained per second (SetHitAndRunDecay).
	HitAndRun(std::string copModel, float decay);
	~HitAndRun();

	enum class Event
	{
		None,
		ChaseStarted,
		Busted,
		Evaded,
	};

	// Adds to the meter, 0-100.
	void Add(float points, const char* why);
	// `onFoot`: the player is walking, and a police car cannot follow them off
	// the road; it busts them from further away.
	Event Update(double dt, const Vector3& player, float playerSpeedKmh, bool onFoot, WorldPhysics& physics,
	             const PathGraph& graph);
	// Back to nothing: no meter, no police car.
	void Reset();

	float GetMeter() const { return _meter; }
	bool IsChasing() const { return _cop != nullptr; }
	Vehicle* GetCop() const { return _cop.get(); }

private:
	void startChase(const Vector3& player, WorldPhysics& physics, const PathGraph& graph);

	std::string _copModel;
	float _decay;
	float _meter = 0.0f;

	std::unique_ptr<Vehicle> _cop;
	std::unique_ptr<RaceOpponent> _driver;
	Vector3 _routedTo;
	double _sinceRoute = 0.0;
	double _caughtFor = 0.0;
	double _lostFor = 0.0;
};

} // namespace Donut
