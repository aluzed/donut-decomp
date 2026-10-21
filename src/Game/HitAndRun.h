#pragma once

#include "Core/Math/Vector3.h"

#include <memory>
#include <string>
#include <vector>

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
	// "cPolice"); `decay` the meter drained per second (SetHitAndRunDecay);
	// `cars` how many come after the player (SetNumChaseCars: one on levels 1
	// to 3, two after), one after another.
	HitAndRun(std::string copModel, float decay, int cars = 1);
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
	bool IsChasing() const { return !_cops.empty(); }
	// The police cars after the player.
	std::vector<Vehicle*> GetCops() const;
	// The nearest of them to `position`, how far; a large number with none.
	float NearestCop(const Vector3& position) const;

private:
	struct Cop
	{
		std::unique_ptr<Vehicle> car;
		std::unique_ptr<RaceOpponent> driver;
		Vector3 routedTo;
		double sinceRoute = 0.0;
	};
	// One more police car, from a road some way off the player.
	void addCop(const Vector3& player, WorldPhysics& physics, const PathGraph& graph);
	void clear();

	std::string _copModel;
	float _decay;
	int _cars = 1;
	float _meter = 0.0f;

	std::vector<Cop> _cops;
	// Since the chase began, for the next car's coming.
	double _chasingFor = 0.0;
	double _caughtFor = 0.0;
	double _lostFor = 0.0;
	// They have come within reach since the chase began.
	bool _closedIn = false;
};

} // namespace Donut
