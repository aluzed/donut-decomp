// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/RaceOpponent.h"

#include "Core/Log.h"
#include "Vehicle.h"

#include <cmath>
#include <cstddef>

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
constexpr float kProgressToBoost = 1.0f; // per lap of deficit

// A car that covers less than kStuckTravel metres in kStuckWindow seconds is
// wedged against something. The old test -- instantaneous speed under 2km/h and
// a frame step under 2cm -- almost never fired: a car grinding along a wall at
// 1-3km/h kept resetting it, and the opponent sat there for the whole race.
constexpr double kStuckWindowSeconds = 3.0;
constexpr float kStuckTravelMetres = 4.0f;
// How long to reverse out for, and how many failed attempts before we give up
// and put the car back on the circuit.
constexpr double kReverseSeconds = 1.5;
constexpr float kReverseThrottle = -0.8f;
constexpr int kReverseAttempts = 3;

// Throttle floor: below this the car cannot overcome its own friction and simply
// stalls on the spot.
constexpr float kThrottleMin = 0.55f;

// Speed control. The circuit turns street corners with waypoints 8-16m apart,
// and the opponent used to arrive at them doing 57km/h, miss by 14m and drive
// on into open country. Look this far along the route, add up how much it
// bends, and aim for a speed between the two below.
constexpr float kLookaheadMetres = 35.0f;
constexpr std::size_t kLookaheadWaypoints = 8;
constexpr float kStraightSpeedKmh = 60.0f;
constexpr float kCornerSpeedKmh = 22.0f;
// Total bend over the lookahead, in radians, that calls for kCornerSpeedKmh.
constexpr float kFullBendRadians = 1.6f;
// How far over the target speed the car may run before it brakes rather than
// simply lifting off.
constexpr float kBrakeMarginKmh = 6.0f;
constexpr float kSpeedBandKmh = 12.0f;

// How far to the side of the leg it is driving the car may drift before it is
// treated as having lost the route. Overshooting a corner leaves the next
// waypoint behind and to the side, and seeking it in a straight line drives
// across gardens and eventually off the map -- the opponent reached waypoint 11
// and then drove 100m into open country. Past this, rejoin at the nearest point.
constexpr float kStrayDistance = 30.0f;
// How far ahead the rejoin looks. Wide enough to skip a corner it has cut,
// narrow enough that it cannot jump most of a lap.
constexpr std::size_t kRejoinWindow = 12;

// How far below its own waypoint the car has to be before we call it lost. The
// circuit runs over bridges and dips, so this has to clear the level's own
// relief; anything past it means the car is off the map and falling.
constexpr float kFallenBelowTarget = 25.0f;
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
	_path.SnapToNearest(_vehicle.GetPosition());
}

void RaceOpponent::Update(double dt, float playerProgress)
{
	if (_path.Empty())
		return;

	const Vector3 position = _vehicle.GetPosition();

	// A car that leaves the road falls forever, and the race is over with nothing
	// on screen to explain it: one missed corner sent the opponent to Y = -45000.
	// Put it back on the circuit and let it carry on.
	if (position.Y < _path.Target().Y - kFallenBelowTarget)
	{
		Log::Warn("RaceOpponent: '{}' fell to y={:.0f}, putting it back on the circuit at waypoint {}",
		          _vehicle.GetName(), position.Y, _path.Index());
		respawnOnCircuit();
		return;
	}

	_path.Advance(position, kWaypointRadius);

	const float offRoute = _path.CrossTrackDistance(position);
	if (offRoute > kStrayDistance)
	{
		const std::size_t was = _path.Index();
		_path.SnapToNearestAhead(position, kRejoinWindow);
		if (_path.Index() != was)
			Log::Debug("RaceOpponent: '{}' was {:.0f}m to the side of leg {}, rejoining at {}", _vehicle.GetName(),
			           offRoute, was, _path.Index());
	}

	const Vector3 target = _path.Target();
	const float steer = Steering::Seek(position, target, _vehicle.GetRotation(), 1.0f);

	// Rubber-banding: scale the throttle by how far behind the player we are.
	// Both sides are in laps; the opponent's circuit and the player's checkpoint
	// list have different lengths, so raw indices are not comparable.
	if (playerProgress >= 0.0f)
	{
		const float deficit = playerProgress - _path.ProgressLaps();
		_boost = 1.0f + deficit * kProgressToBoost;
		if (_boost < kBoostMin) _boost = kBoostMin;
		if (_boost > kBoostMax) _boost = kBoostMax;
	}
	else
	{
		_boost = 1.0f;
	}

	// Drive to a speed, not to a throttle position. Scaling the throttle by the
	// steering angle (the previous rule) only reacts once the car is already in
	// the corner, far too late to make it: aim for the speed the *next* stretch of
	// route can be taken at, and brake if we are over it. Steering::ArrivalSpeed
	// is no use here either -- it eases to zero at every waypoint, which is right
	// for a destination and wrong for a circuit.
	const float speed = _vehicle.GetSpeedKmh();
	const float targetSpeed = cornerSpeedKmh() * _boost;

	float throttle = (targetSpeed - speed) / kSpeedBandKmh;
	if (throttle > 1.0f) throttle = 1.0f;
	if (throttle < kThrottleMin) throttle = kThrottleMin; // below this it stalls

	float brake = (speed - targetSpeed - kBrakeMarginKmh) / kSpeedBandKmh;
	if (brake > 1.0f) brake = 1.0f;
	if (brake < 0.0f) brake = 0.0f;
	if (brake > 0.0f) throttle = 0.0f;

	Vector3 travelled = position - _lastPosition;
	travelled.Y = 0.0f;
	_lastPosition = position;

	if (_reverseTimer > 0.0)
	{
		// Back out with opposite lock; the next frames re-seek the waypoint.
		_reverseTimer -= dt;
		_vehicle.ApplyInput(kReverseThrottle, -steer, 0.0f, 1.0f);
		return;
	}

	_stuckTimer += dt;
	_stuckTravel += travelled.Length();
	if (_stuckTimer >= kStuckWindowSeconds)
	{
		if (_stuckTravel < kStuckTravelMetres)
		{
			if (++_reverseAttempts > kReverseAttempts)
			{
				Log::Warn("RaceOpponent: '{}' wedged at ({:.1f}, {:.1f}, {:.1f}) after {} attempts to back out, "
				          "putting it back on the circuit",
				          _vehicle.GetName(), position.X, position.Y, position.Z, kReverseAttempts);
				_reverseAttempts = 0;
				respawnOnCircuit();
				return;
			}

			Log::Debug("RaceOpponent: '{}' moved {:.1f}m in {:.0f}s at ({:.1f}, {:.1f}, {:.1f}), backing out",
			           _vehicle.GetName(), _stuckTravel, kStuckWindowSeconds, position.X, position.Y, position.Z);
			_reverseTimer = kReverseSeconds;
		}
		else
		{
			_reverseAttempts = 0;
		}

		_stuckTimer = 0.0;
		_stuckTravel = 0.0f;
	}

	_vehicle.ApplyInput(throttle, steer, brake, _boost);

	// Whether the opponent is making progress is the whole question for AI-RACE,
	// and a car wedged against a wall looks identical to one driving from the
	// outside. Report where it is, what it is aiming at, and how fast.
	_logTimer += dt;
	if (_logTimer >= 5.0)
	{
		_logTimer = 0.0;
		Log::Debug("RaceOpponent: lap {} waypoint {}/{} at ({:.1f}, {:.1f}, {:.1f}), {:.0f} of {:.0f} km/h, target "
		           "({:.1f}, {:.1f}, {:.1f}) {:.1f}m away, throttle {:.2f} brake {:.2f} steer {:.2f} boost {:.2f}",
		           _path.Laps(), _path.Index(), _path.Count(), position.X, position.Y, position.Z, speed, targetSpeed,
		           target.X, target.Y, target.Z, (target - position).Length(), throttle, brake, steer, _boost);
	}
}

float RaceOpponent::cornerSpeedKmh() const
{
	Vector3 heading = _path.Target() - _vehicle.GetPosition();
	heading.Y = 0.0f;
	if (heading.LengthSquared() < 0.01f)
		return kStraightSpeedKmh;
	heading.Normalize();

	Vector3 from = _path.Target();
	float distance = 0.0f, bend = 0.0f;
	for (std::size_t i = 1; i <= kLookaheadWaypoints && distance < kLookaheadMetres; ++i)
	{
		Vector3 leg = _path.Peek(i) - from;
		leg.Y = 0.0f;
		const float length = leg.Length();
		if (length < 0.01f)
			continue;

		leg.Normalize();
		float dot = heading.X * leg.X + heading.Z * leg.Z;
		if (dot > 1.0f) dot = 1.0f;
		if (dot < -1.0f) dot = -1.0f;
		bend += std::acos(dot);

		distance += length;
		heading = leg;
		from = _path.Peek(i);
	}

	float t = bend / kFullBendRadians;
	if (t > 1.0f) t = 1.0f;
	return kStraightSpeedKmh + (kCornerSpeedKmh - kStraightSpeedKmh) * t;
}

void RaceOpponent::respawnOnCircuit()
{
	const Vector3 here = _path.Target();
	const Vector3 next = _path.Peek(1);

	Vector3 forward = next - here;
	forward.Y = 0.0f;
	Quaternion rotation = _vehicle.GetRotation();
	if (forward.LengthSquared() > 0.01f)
		rotation = Quaternion(Vector3(0.0f, 1.0f, 0.0f), std::atan2(forward.X, forward.Z));

	_vehicle.Teleport(here, rotation);
	_lastPosition = here;
	_stuckTimer = 0.0;
	_stuckTravel = 0.0f;
	_reverseTimer = 0.0;
}

} // namespace Donut
