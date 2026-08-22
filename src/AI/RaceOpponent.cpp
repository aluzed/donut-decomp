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
constexpr float kWaypointRadius = 8.0f;

// Rubber-banding bounds. Behind the player the opponent presses on, ahead it
// eases off, and it never leaves this band so it can neither vanish nor stall.
constexpr float kBoostMin = 0.85f;
constexpr float kBoostMax = 1.6f;
constexpr float kProgressToBoost = 0.15f; // per waypoint of deficit

// If the car has not moved for this long it is wedged; back up to free it.
constexpr float kThrottleMin = 0.55f;

constexpr double kStuckSeconds = 2.0;
constexpr float kStuckSpeedKmh = 2.0f;
} // namespace

RaceOpponent::RaceOpponent(Vehicle& vehicle, std::vector<Vector3> circuit)
    : _vehicle(vehicle), _path(std::move(circuit)), _lastPosition(vehicle.GetPosition())
{
	Log::Info("RaceOpponent: driving '{}' around {} waypoints", _vehicle.GetName(), _path.Count());
}

void RaceOpponent::SetCircuit(std::vector<Vector3> circuit)
{
	Log::Info("RaceOpponent: circuit replaced, {} waypoints", circuit.size());
	_path.SetWaypoints(std::move(circuit));
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

	// Throttle from how hard we are turning, not from distance to the waypoint.
	// Steering::ArrivalSpeed eases to zero on approach, which is right for a
	// destination and wrong for a circuit: the opponent slowed to a stop at every
	// waypoint it passed through. Never drop below kThrottleMin or the car cannot
	// overcome its own friction and simply stalls on the spot.
	const float turn = steer < 0.0f ? -steer : steer;
	float throttle = 1.0f - turn * 0.4f;
	if (throttle < kThrottleMin) throttle = kThrottleMin;

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

	_vehicle.ApplyInput(throttle, steer, 0.0f, _boost);
}

} // namespace Donut
