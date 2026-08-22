// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/RaceOpponent.h"

#include "Core/Log.h"
#include "Vehicle.h"

namespace Donut
{
namespace
{
// Waypoint capture radius. Wider than the player's 5m checkpoint test: the
// opponent only needs to pass near the racing line, not through a trigger.
constexpr float kWaypointRadius = 12.0f;

// Rubber-banding bounds. Behind the player the opponent presses on, ahead it
// eases off, and it never leaves this band so it can neither vanish nor stall.
constexpr float kBoostMin = 0.7f;
constexpr float kBoostMax = 1.6f;
constexpr float kProgressToBoost = 0.15f; // per waypoint of deficit

// If the car has not moved for this long it is wedged; back up to free it.
constexpr double kStuckSeconds = 2.0;
constexpr float kStuckSpeedKmh = 2.0f;
} // namespace

RaceOpponent::RaceOpponent(Vehicle& vehicle, std::vector<Vector3> circuit)
    : _vehicle(vehicle), _path(std::move(circuit)), _lastPosition(vehicle.GetPosition())
{
	Log::Info("RaceOpponent: driving '{}' around {} waypoints", _vehicle.GetName(), _path.Count());
}

void RaceOpponent::Update(double dt, float playerProgress)
{
	if (_path.Empty())
		return;

	const Vector3 position = _vehicle.GetPosition();
	_path.Advance(position, kWaypointRadius);

	const Vector3 target = _path.Target();
	const float steer = Steering::Seek(position, target, _vehicle.GetRotation(), 1.0f);

	// Rubber-banding: scale the throttle by how far behind the player we are.
	if (playerProgress >= 0.0f)
	{
		const float deficit = playerProgress - _path.Progress();
		_boost = 1.0f + deficit * kProgressToBoost;
		if (_boost < kBoostMin) _boost = kBoostMin;
		if (_boost > kBoostMax) _boost = kBoostMax;
	}
	else
	{
		_boost = 1.0f;
	}

	// Ease off into corners so the car does not understeer past the waypoint.
	const float targetSpeed = Steering::ArrivalSpeed(position, target, 1.0f, 15.0f);

	Vector3 travelled = position - _lastPosition;
	travelled.Y = 0.0f;
	_lastPosition = position;

	if (_vehicle.GetSpeedKmh() < kStuckSpeedKmh && travelled.LengthSquared() < 0.0004f)
		_stuckTimer += dt;
	else
		_stuckTimer = 0.0;

	if (_stuckTimer > kStuckSeconds)
	{
		// Reverse out with opposite lock; the next frames re-seek the waypoint.
		_vehicle.ApplyInput(0.0f, -steer, 1.0f, 1.0f);
		if (_stuckTimer > kStuckSeconds * 2.0)
			_stuckTimer = 0.0;
		return;
	}

	_vehicle.ApplyInput(targetSpeed, steer, 0.0f, _boost);
}

} // namespace Donut
