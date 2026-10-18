#include "HitAndRun.h"

#include "AI/PathGraph.h"
#include "AI/RaceOpponent.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Vehicle.h"

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
// Lost: the police car this far behind for this long.
constexpr float kLostDistance = 150.0f;
constexpr double kLostSeconds = 5.0;
} // namespace

HitAndRun::HitAndRun(std::string copModel, float decay): _copModel(std::move(copModel)), _decay(decay) {}

HitAndRun::~HitAndRun()
{
	// The driver holds the car by reference: it goes first.
	_driver.reset();
	_cop.reset();
}

void HitAndRun::Add(float points, const char* why)
{
	const float before = _meter;
	_meter = std::min(100.0f, _meter + points);
	Log::Info("HitAndRun: {} (+{:.0f}), meter {:.0f} -> {:.0f}", why, points, before, _meter);
}

void HitAndRun::Reset()
{
	_driver.reset();
	_cop.reset();
	_meter = 0.0f;
	_caughtFor = 0.0;
	_lostFor = 0.0;
}

void HitAndRun::startChase(const Vector3& player, WorldPhysics& physics, const PathGraph& graph)
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
	_cop = std::make_unique<Vehicle>("hit_and_run_" + _copModel);
	_cop->LoadModel(model);
	_cop->CreatePhysicsBody(physics, spot);
	const Vector3 toPlayer = player - spot;
	_cop->Teleport(spot, Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z), 0.0f)));
	_driver.reset();
	_sinceRoute = 0.0;
	_caughtFor = 0.0;
	_lostFor = 0.0;
	Log::Info("HitAndRun: the police are coming, '{}' from ({:.0f}, {:.0f}, {:.0f})", _copModel, spot.X, spot.Y, spot.Z);
}

HitAndRun::Event HitAndRun::Update(double dt, const Vector3& player, float playerSpeedKmh, bool onFoot,
                                   WorldPhysics& physics, const PathGraph& graph)
{
	// Full sends the police, before it drains: filled and drained in the same
	// frame, it never read 100.
	// The car's position and model follow its physics body only through Update;
	// without it the car was seen, and measured, where it had last teleported.
	if (_cop)
		_cop->Update(dt);

	const bool full = _meter >= 100.0f;
	// It drains while no police car is in sight.
	if (!_cop || (_cop->GetPosition() - player).Length() > 60.0f)
		_meter = std::max(0.0f, _meter - _decay * static_cast<float>(dt));

	if (!_cop)
	{
		if (!full)
			return Event::None;
		startChase(player, physics, graph);
		return Event::ChaseStarted;
	}

	// Driven along the roads to the player, the route drawn again as the player
	// moves on, as a mission's chasing car is.
	_sinceRoute += dt;
	if (!_driver || ((player - _routedTo).Length() > 20.0f && _sinceRoute > 2.0))
	{
		std::vector<Vector3> route = graph.FindRoute(_cop->GetPosition(), player);
		if (route.empty())
			route.push_back(_cop->GetPosition());
		route.push_back(player);
		if (!_driver)
			_driver = std::make_unique<RaceOpponent>(*_cop, std::move(route), true);
		else
			_driver->SetCircuit(std::move(route));
		_routedTo = player;
		_sinceRoute = 0.0;
	}
	_driver->Update(dt, -1.0f);

	const float distance = (_cop->GetPosition() - player).Length();
	const float reach = onFoot ? kBustDistanceOnFoot : kBustDistance;
	_caughtFor = distance < reach && std::abs(playerSpeedKmh) < kBustSpeedKmh ? _caughtFor + dt : 0.0;
	_lostFor = distance > kLostDistance ? _lostFor + dt : 0.0;

	if (_caughtFor > kBustSeconds)
	{
		Log::Info("HitAndRun: busted");
		return Event::Busted;
	}
	// Shaken off, or the meter has run dry while the car is still some way off.
	if (_lostFor > kLostSeconds || (_meter <= 0.0f && distance > 40.0f))
	{
		Log::Info("HitAndRun: the police have given up, {:.0f}m behind", distance);
		_driver.reset();
		_cop.reset();
		return Event::Evaded;
	}
	return Event::None;
}

} // namespace Donut
