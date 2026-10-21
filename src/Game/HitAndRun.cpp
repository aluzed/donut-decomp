#include "HitAndRun.h"

#include "AI/PathGraph.h"
#include "AI/RaceOpponent.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Vehicle.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace Donut
{

namespace
{
// The police come from this far off, along the roads.
constexpr float kSpawnDistance = 60.0f;
// Caught: the police car this close, the player this slow, for this long.
constexpr float kBustDistance = 6.0f;
constexpr float kBustDistanceOnFoot = 15.0f;
constexpr float kBustSpeedKmh = 8.0f;
constexpr double kBustSeconds = 1.0;
// Lost: every police car this far behind for this long.
constexpr float kLostDistance = 150.0f;
constexpr double kLostSeconds = 5.0;
// The cars after the first come this much later each: not the game's figure
// (SetChaseSpawnRate is only ever commented out), enough to come from apart.
constexpr double kNextCopSeconds = 4.0;
// Not come within reach after this long, they give up.
constexpr double kComingSeconds = 30.0;
} // namespace

HitAndRun::HitAndRun(std::string copModel, float decay, int cars)
    : _copModel(std::move(copModel)), _decay(decay), _cars(std::clamp(cars, 1, 4))
{
}

HitAndRun::~HitAndRun()
{
	clear();
}

void HitAndRun::clear()
{
	// A driver holds its car by reference: it goes first.
	for (auto& cop : _cops)
		cop.driver.reset();
	_cops.clear();
}

std::vector<Vehicle*> HitAndRun::GetCops() const
{
	std::vector<Vehicle*> cars;
	for (const auto& cop : _cops)
		cars.push_back(cop.car.get());
	return cars;
}

float HitAndRun::NearestCop(const Vector3& position) const
{
	float nearest = 1.0e9f;
	for (const auto& cop : _cops)
		nearest = std::min(nearest, (cop.car->GetPosition() - position).Length());
	return nearest;
}

void HitAndRun::Add(float points, const char* why)
{
	const float before = _meter;
	_meter = std::min(100.0f, _meter + points);
	Log::Info("HitAndRun: {} (+{:.0f}), meter {:.0f} -> {:.0f}", why, points, before, _meter);
}

void HitAndRun::Reset()
{
	clear();
	_meter = 0.0f;
	_caughtFor = 0.0;
	_lostFor = 0.0;
}

void HitAndRun::addCop(const Vector3& player, WorldPhysics& physics, const PathGraph& graph)
{
	// A road some way off, in a direction of its own each time.
	static std::mt19937 rng(std::random_device {}());
	const float angle = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(rng);
	const Vector3 wanted = player + Vector3(std::sin(angle), 0.0f, std::cos(angle)) * kSpawnDistance;
	const int node = graph.FindNearestRoadNode(wanted);
	const Vector3 spot = node >= 0 ? graph.GetNodes()[node].position : wanted;

	std::string model = "art/cars/" + _copModel + ".p3d";
	if (!FileSystem::exists(model))
		model = "art/cars/cPolice.p3d";
	Cop cop;
	cop.car = std::make_unique<Vehicle>(fmt::format("hit_and_run_{}_{}", _copModel, _cops.size()));
	cop.car->LoadModel(model);
	cop.car->CreatePhysicsBody(physics, spot);
	const Vector3 toPlayer = player - spot;
	cop.car->Teleport(spot, Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z), 0.0f)));
	Log::Info("HitAndRun: police car {} of {}, '{}' from ({:.0f}, {:.0f}, {:.0f})", _cops.size() + 1, _cars, _copModel,
	          spot.X, spot.Y, spot.Z);
	_cops.push_back(std::move(cop));
}

HitAndRun::Event HitAndRun::Update(double dt, const Vector3& player, float playerSpeedKmh, bool onFoot,
                                   WorldPhysics& physics, const PathGraph& graph)
{
	// Full sends the police, before it drains: filled and drained in the same
	// frame, it never read 100.
	// A car's position and model follow its physics body only through Update;
	// without it the car was seen, and measured, where it had last teleported.
	for (auto& cop : _cops)
		cop.car->Update(dt);

	const bool full = _meter >= 100.0f;
	// It drains while no police car is in sight.
	if (NearestCop(player) > 60.0f)
		_meter = std::max(0.0f, _meter - _decay * static_cast<float>(dt));

	if (_cops.empty())
	{
		if (!full)
			return Event::None;
		_chasingFor = 0.0;
		_caughtFor = 0.0;
		_lostFor = 0.0;
		_closedIn = false;
		addCop(player, physics, graph);
		return Event::ChaseStarted;
	}
	// The others, one after another, while the meter still calls for them.
	_chasingFor += dt;
	if (static_cast<int>(_cops.size()) < _cars && _meter > 0.0f &&
	    _chasingFor > kNextCopSeconds * static_cast<double>(_cops.size()))
		addCop(player, physics, graph);

	// Each driven along the roads to the player, the route drawn again as the
	// player moves on, as a mission's chasing car is.
	for (auto& cop : _cops)
	{
		cop.sinceRoute += dt;
		if (!cop.driver || ((player - cop.routedTo).Length() > 20.0f && cop.sinceRoute > 2.0))
		{
			std::vector<Vector3> route = graph.FindRoute(cop.car->GetPosition(), player);
			if (route.empty())
				route.push_back(cop.car->GetPosition());
			route.push_back(player);
			if (!cop.driver)
				cop.driver = std::make_unique<RaceOpponent>(*cop.car, std::move(route), true);
			else
				cop.driver->SetCircuit(std::move(route));
			cop.routedTo = player;
			cop.sinceRoute = 0.0;
		}
		cop.driver->Update(dt, -1.0f);
	}

	const float distance = NearestCop(player);
	const float reach = onFoot ? kBustDistanceOnFoot : kBustDistance;
	_caughtFor = distance < reach && std::abs(playerSpeedKmh) < kBustSpeedKmh ? _caughtFor + dt : 0.0;
	// Shaken off once they have closed in; before that, they are on their
	// way (a car can only come from the nearest road, 200 m off at times).
	_closedIn = _closedIn || distance < kLostDistance;
	_lostFor = distance > kLostDistance ? _lostFor + dt : 0.0;
	if (!_closedIn && _chasingFor < kComingSeconds)
		_lostFor = 0.0;

	if (_caughtFor > kBustSeconds)
	{
		Log::Info("HitAndRun: busted");
		return Event::Busted;
	}
	// Shaken off, or the meter has run dry while the cars are still some way off.
	if (_lostFor > kLostSeconds || (_meter <= 0.0f && distance > 40.0f))
	{
		Log::Info("HitAndRun: the police have given up, {:.0f}m behind", distance);
		clear();
		return Event::Evaded;
	}
	return Event::None;
}

} // namespace Donut
