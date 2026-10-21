// Copyright 2019-2020 the donut authors. See AUTHORS.md

// Game's autopilot (--autopilot): the player put onto each objective in turn.

#include "Game.h"

#include "AI/TrafficManager.h"
#include "Character.h"
#include "Render/imgui/imgui.h"
#include "Scripting/ScriptEngine.h"

#include "Core/Log.h"

#include <algorithm>
#include <string>

namespace Donut
{

namespace
{
// How far above the ground something to pick up must be for the autopilot to
// put the car down at its height rather than on the ground.
constexpr float kHighPickup = 3.0f;
// How near a traffic car, and a mission's car, the autopilot may put the
// player's car down: beside a car, still within reach of what lies by it.
constexpr float kTrafficClearance = 6.0f;
constexpr float kVehicleClearance = 3.0f;
} // namespace

bool Game::clearOfTraffic(const Vector3& at) const
{
	// The mission's cars too, but the player's own: a truck rammed to a stop
	// stood where what it dropped lay, and the car put down on it was thrown
	// off, every time, until the stage ran out (level 4 m1).
	for (const auto& vehicle : _scriptEngine->GetMissionVehicles())
		if (vehicle.get() != _activeVehicle && (vehicle->GetPosition() - at).Length() < kVehicleClearance)
			return false;
	if (!_trafficManager || _scriptEngine->IsTrafficSuppressed())
		return true;
	for (const auto& car : _trafficManager->GetCars())
		if ((car.position - at).Length() < kTrafficClearance)
			return false;
	return true;
}

Vector3 Game::clearSpotNear(const Vector3& at) const
{
	// The autopilot's car is put down where no traffic car is: dropped inside
	// one's box, it was thrown out at 100 m/s and more, wrecked and off the
	// map (level 6 m1, level 3 m3). Beside it, then, a little further each
	// time.
	if (clearOfTraffic(at))
		return at;
	for (const float away : {3.0f, 5.0f, 8.0f, 12.0f})
		for (const Vector3& side :
		     {Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f)})
		{
			const Vector3 spot = at + side * away;
			float ground = 0.0f;
			if (clearOfTraffic(spot) && _worldPhysics->FindGroundHeight(spot, 5.0f, 5.0f, ground, true))
				return Vector3(spot.X, std::max(spot.Y, ground), spot.Z);
		}
	return at;
}

void Game::runAutopilot(double dt)
{
	if (!_scriptEngine->IsMissionActive() || !_states.Is(GameState::InGame))
		return;

	// Every two seconds, not every frame: each step should be seen to land, and
	// a race's checkpoints are taken one at a time.
	_autopilotTimer += dt;
	if (_autopilotTimer < 2.0)
		return;
	_autopilotTimer = 0.0;

	// Chasing, ramming or racing is done at the wheel: a player left on foot by
	// the stage before (a talkto) has their car brought round. Level 4 m1's
	// dump was "rammed" on foot and timed out.
	if (!_inVehicle && _character)
	{
		const std::string& wanted = _scriptEngine->GetObjectiveType();
		Vehicle* car = _scriptEngine->GetPlayerCar();
		if (car && (wanted == "follow" || wanted == "destroy" || wanted == "dump" || wanted == "race"))
		{
			Log::Info("Game: autopilot takes the wheel of '{}'", car->GetName());
			car->Teleport(_character->GetPosition() + Vector3(2.5f, 0.0f, 0.0f), car->GetRotation());
			PutPlayerInVehicle(car);
			return;
		}
	}

	if (_scriptEngine->ObjectiveWantsCar() && !_inVehicle && !_scriptEngine->GetMissionVehicles().empty())
	{
		Log::Info("Game: autopilot gets in '{}'", _scriptEngine->GetMissionVehicles().front()->GetName());
		PutPlayerInVehicle(_scriptEngine->GetMissionVehicles().front().get());
		return;
	}

	// Tailing a car is done from behind it, and only when it is getting away:
	// dropping the player's car onto the target every two seconds rammed it
	// off its route.
	// A car to destroy is met head on: the player's car is parked across its
	// path, 15m ahead, and it drives into it.
	// A dump's truck likewise, until something has dropped out of it to pick up.
	const std::string& objective = _scriptEngine->GetObjectiveType();
	const bool ramming = objective == "destroy" || (objective == "dump" && !_scriptEngine->HasPickupsLying());
	if (const Vehicle* quarry = _scriptEngine->GetFollowTarget(); quarry && ramming && _inVehicle && _activeVehicle)
	{
		const Vector3 ahead = clearSpotNear(quarry->GetPosition() + quarry->GetRotation() * Vector3::Forward * 15.0f);
		Log::Info("Game: autopilot blocks '{}'", quarry->GetName());
		_activeVehicle->Teleport(ahead, quarry->GetRotation() * Quaternion(Vector3(0.0f, 1.0f, 0.0f), 1.5708f));
		return;
	}

	if (const Vehicle* followed = objective == "follow" ? _scriptEngine->GetFollowTarget() : nullptr)
	{
		const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
		// Well inside the tightest follow distance the scripts set (level 4 m1's
		// 120m): the autopilot moves every 2s, and at 75 km/h the car gains 40m
		// between two looks. At 100m it was lost before the next one, and at
		// 60m it still was, now and then, over a long chase.
		if ((followed->GetPosition() - me).Length() < 45.0f)
			return;
		// 25m behind, or closer when there is no road there: m5's van, put back
		// on its route in a sunken lane, had nothing but a drop 25m behind it,
		// and the player's car fell off the map.
		Vector3 behind;
		bool grounded = false;
		for (const float back : {18.0f, 12.0f, 8.0f})
		{
			behind = followed->GetPosition() - followed->GetRotation() * Vector3::Forward * back;
			// Not on top of the traffic: put down inside a traffic car's box, the
			// player's car was thrown out at 55 m/s and off the map, and the
			// car being followed was lost.
			float ground = 0.0f;
			if (clearOfTraffic(behind) && _worldPhysics->FindGroundHeight(behind, 5.0f, 5.0f, ground, true))
			{
				grounded = true;
				break;
			}
		}
		if (!grounded)
			return;
		Log::Info("Game: autopilot catches up with '{}'", followed->GetName());
		if (_inVehicle && _activeVehicle)
			_activeVehicle->Teleport(behind, followed->GetRotation());
		else
			SetPlayerPosition(behind + Vector3(0.0f, 1.0f, 0.0f));
		return;
	}

	// Doors are walked through, not driven into, and people walked up to.
	if ((objective == "interior" || objective == "talkto") && _inVehicle && _activeVehicle)
	{
		Log::Info("Game: autopilot gets out of '{}'", _activeVehicle->GetName());
		_inVehicle = false;
		SetPlayerPosition(_activeVehicle->GetPosition() + Vector3(3.0f, 0.0f, 0.0f));
		_activeVehicle = nullptr;
		return;
	}

	Vector3 target;
	if (!_scriptEngine->GetObjectiveTarget(target))
		return;
	if (_inVehicle && _activeVehicle)
		target = clearSpotNear(target);
	Log::Info("Game: autopilot to ({:.1f}, {:.1f}, {:.1f})", target.X, target.Y, target.Z);
	// Something to pick up high above the ground is taken where it is, at its
	// height: level 4 m3's ketchup hangs 11m above the road, for a jump the
	// autopilot cannot make. One near the ground -- what a truck rammed drops,
	// at the height of its middle -- is driven to on the ground: put down at
	// that height, the car was half in the road, thrown off it, and never
	// came within reach (level 4 m1, the stage timed out).
	bool high = false;
	if (objective == "delivery" || objective == "dump")
	{
		float ground = 0.0f;
		high = !_worldPhysics->FindGroundHeight(target, 1.0f, 30.0f, ground, true) || target.Y - ground > kHighPickup;
	}
	// Someone to talk to is walked up to, not stood on: from the spot itself
	// the conversation's shots framed the two people as one.
	if (objective == "talkto" && !(_inVehicle && _activeVehicle) && _character)
	{
		Vector3 away = _character->GetPosition() - target;
		away.Y = 0.0f;
		target = target + (away.LengthSquared() > 0.01f ? away.Normalized() : Vector3(0.0f, 0.0f, 1.0f)) * 1.5f;
	}
	if (_inVehicle && _activeVehicle)
		_activeVehicle->Teleport(target, _activeVehicle->GetRotation(), !high);
	else
		SetPlayerPosition(target + Vector3(0.0f, 1.0f, 0.0f));
}

} // namespace Donut
