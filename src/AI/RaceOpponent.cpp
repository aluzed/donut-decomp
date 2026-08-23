// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/RaceOpponent.h"

#include "Core/Log.h"
#include "Physics/WorldPhysics.h"
#include "Vehicle.h"

#include <fmt/format.h>

#include <cmath>
#include <cstddef>
#include <string>

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
// Two wedges closer together than this are the same wedge, however much driving
// happened in between.
constexpr float kSameWedgeMetres = 15.0f;
// Longest a car may go without reaching a waypoint before it is moved on. The
// circuit's longest leg is 46m, which is eight seconds even at a crawl.
constexpr double kNoProgressSeconds = 12.0;
// How far past the place it gave up the car is put when backing out has failed
// that many times -- far enough to be clear of whatever was holding it.
constexpr float kUnwedgeClearanceMetres = 30.0f;

// Throttle floor: below this the car cannot overcome its own friction and simply
// stalls on the spot.
constexpr float kThrottleMin = 0.55f;

// Speed control. Aim at the speed the route *ahead* can be taken at, and get
// there before the corner rather than during it.
//
// The old rule added up how much the next 35m of route bent and lerped between a
// fast and a slow speed on the total. That conflates two different things -- a
// long sweeping curve and a right-angle street corner bend by the same number of
// radians -- and it says nothing about when to start braking, so the car arrived
// at the corner already needing to be slow. Raising engine power made it worse,
// not better: at 3.0 m/s^2 the opponent got 18 waypoints round instead of 70.
//
// What replaces it is the standard two-part speed profile:
//   1. every corner has a speed its radius and the available grip allow,
//      v = sqrt(a_lat * r);
//   2. that speed is carried *back* along the route to here through the braking
//      the car can actually do, v_here = sqrt(v_corner^2 + 2 * a_brake * s).
// The lowest of those over the look-ahead is the answer, so the brakes come on
// exactly as early as the corner requires -- and they come on earlier by
// themselves when the car is quicker, which is what lets the engine grow.

// Lateral acceleration the planner is willing to use. The tyres are set to
// frictionSlip 10.5 and would hold more, but the chassis is a box on soft
// suspension and leans onto two wheels well before the tyres let go.
constexpr float kLateralAccelMss = 4.5f;
// Braking the planner counts on. Deliberately under what the car can do, so
// arriving a little hot still makes the corner.
constexpr float kBrakeDecelMss = 5.0f;

// How much route to plan over. Far enough to see the next corner from the far
// end of a straight at full speed, short enough not to brake for a bend two
// junctions away.
constexpr float kProfileMetres = 100.0f;
constexpr std::size_t kProfileWaypoints = 24;

// Radius below which a bend is treated as the tightest the car can physically
// take: the steering is capped at 0.5 rad over a 3.6m wheelbase, which is a
// turning circle of about 6.6m. Planning for anything tighter is planning for a
// corner the car cannot take at any speed.
constexpr float kMinCornerRadius = 7.0f;
// Three waypoints in a line give an infinite radius; cap it so the arithmetic
// stays finite.
constexpr float kStraightRadius = 1.0e4f;

// Speed on a clear straight, and the floor the profile may never go below --
// a car that plans to crawl round a hairpin at 5 km/h never finishes.
constexpr float kStraightSpeedKmh = 100.0f;
constexpr float kCornerSpeedKmh = 20.0f;

// How far over target the car may run before braking rather than lifting off,
// and the speed error that calls for full throttle or full brake.
constexpr float kBrakeMarginKmh = 4.0f;
constexpr float kSpeedBandKmh = 12.0f;

// Pure pursuit. Steer at a point that slides along the route rather than at the
// next waypoint: a waypoint jumps sideways the moment it is captured, and the
// car sawed at the wheel following it. The aim point is a time ahead, so the
// faster the car the further down the road it looks, and the chord it drives
// cuts each bend to its apex on its own.
constexpr float kPursuitSeconds = 1.1f;
constexpr float kPursuitMinMetres = 9.0f;
constexpr float kPursuitMaxMetres = 30.0f;
// Half the width of road the racing line is allowed to use. The chord of a bend
// may stray this far inside the route and no further.
constexpr float kCorridorMetres = 3.0f;

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

// Obstacle probe used when the car reports itself stuck: start clear of its own
// 2.2m-half-length chassis, at bumper height, and look a car's length ahead.
constexpr float kObstacleRayStart = 2.4f;
constexpr float kObstacleRayHeight = 0.3f;
constexpr float kObstacleRayRange = 6.0f;
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
		respawnOnCircuit(0.0f);
		return;
	}

	// Progress is reaching waypoints, not covering ground. A car bouncing off the
	// same wall reverses and drives at it again every three seconds, which is
	// eight metres on the odometer and looks exactly like driving to the travel
	// test below -- the opponent spent the last twenty seconds of the race doing
	// it at (46, 1.4, -626) without ever being declared stuck. If no waypoint has
	// fallen in this long, it is not racing, whatever the odometer says.
	if (_path.Advance(position, kWaypointRadius))
	{
		_noProgressTimer = 0.0;
	}
	else
	{
		_noProgressTimer += dt;
		if (_noProgressTimer > kNoProgressSeconds)
		{
			Log::Warn("RaceOpponent: '{}' has not reached a waypoint in {:.0f}s at ({:.1f}, {:.1f}, {:.1f}){}",
			          _vehicle.GetName(), kNoProgressSeconds, position.X, position.Y, position.Z, describeObstacle());
			respawnOnCircuit(kUnwedgeClearanceMetres);
			return;
		}
	}

	const float offRoute = _path.CrossTrackDistance(position);
	if (offRoute > kStrayDistance)
	{
		const std::size_t was = _path.Index();
		_path.SnapToNearestAhead(position, kRejoinWindow);
		if (_path.Index() != was)
			Log::Debug("RaceOpponent: '{}' was {:.0f}m to the side of leg {}, rejoining at {}", _vehicle.GetName(),
			           offRoute, was, _path.Index());
	}

	const float speed = _vehicle.GetSpeedKmh();

	// Steer at a point that slides along the route, not at the next waypoint.
	float pursuit = std::fabs(speed) / 3.6f * kPursuitSeconds;
	if (pursuit < kPursuitMinMetres) pursuit = kPursuitMinMetres;
	if (pursuit > kPursuitMaxMetres) pursuit = kPursuitMaxMetres;

	// ...but never so far ahead that the chord to the aim point leaves the road.
	// Pure pursuit cuts a bend by roughly L^2/8R, so the aim distance that keeps
	// that inside half a road width is sqrt(8 * R * corridor). Without this the
	// car looked 30m through a right-angle street corner and drove across the
	// pavement into the building on the inside of it.
	const float radius = tightestRadiusAhead(pursuit);
	const float corridorLimit = std::sqrt(8.0f * radius * kCorridorMetres);
	if (pursuit > corridorLimit) pursuit = corridorLimit;
	if (pursuit < kPursuitMinMetres) pursuit = kPursuitMinMetres;

	const Vector3 aim = _path.PointAhead(position, pursuit);
	const float steer = Steering::Seek(position, aim, _vehicle.GetRotation(), 1.0f);

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

	// Drive to the speed the route ahead allows, not to a throttle position.
	const float targetSpeed = targetSpeedKmh(_boost);
	const float error = targetSpeed - speed;

	// Three states, not two. The old rule clamped the throttle up to kThrottleMin
	// even while over target, so between "too fast to accelerate" and "fast enough
	// to brake" the car kept its foot down and carried the excess into the corner.
	float throttle = 0.0f;
	float brake = 0.0f;
	if (error > 0.0f)
	{
		throttle = error / kSpeedBandKmh;
		if (throttle > 1.0f) throttle = 1.0f;
		if (throttle < kThrottleMin) throttle = kThrottleMin; // below this it stalls
	}
	else if (error < -kBrakeMarginKmh)
	{
		brake = (-error - kBrakeMarginKmh) / kSpeedBandKmh;
		if (brake > 1.0f) brake = 1.0f;
	}
	// Between the two the car coasts: off the throttle, off the brakes.

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
			// Count how often the car has been stuck *in this spot*, not how many
			// windows in a row it managed it. At (231, 4.2, -336) it backed out,
			// drove off, came straight back and wedged again, over and over: every
			// escape reset a consecutive counter, so the give-up never arrived and
			// the race was spent shuttling in and out of the same hole 14 times.
			Vector3 fromLastWedge = position - _lastWedgePosition;
			fromLastWedge.Y = 0.0f;
			if (fromLastWedge.Length() < kSameWedgeMetres)
				++_wedgeRepeats;
			else
				_wedgeRepeats = 1;
			_lastWedgePosition = position;

			if (_wedgeRepeats > kReverseAttempts)
			{
				Log::Warn("RaceOpponent: '{}' wedged at ({:.1f}, {:.1f}, {:.1f}) for the {}th time{}",
				          _vehicle.GetName(), position.X, position.Y, position.Z, _wedgeRepeats, describeObstacle());
				_wedgeRepeats = 0;
				respawnOnCircuit(kUnwedgeClearanceMetres);
				return;
			}

			Log::Debug("RaceOpponent: '{}' moved {:.1f}m in {:.0f}s at ({:.1f}, {:.1f}, {:.1f}), backing out{}",
			           _vehicle.GetName(), _stuckTravel, kStuckWindowSeconds, position.X, position.Y, position.Z,
			           describeObstacle());
			_reverseTimer = kReverseSeconds;
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
		Log::Debug("RaceOpponent: lap {} waypoint {}/{} at ({:.1f}, {:.1f}, {:.1f}), {:.0f} of {:.0f} km/h, aiming "
		           "({:.1f}, {:.1f}, {:.1f}) {:.0f}m ahead, throttle {:.2f} brake {:.2f} steer {:.2f} boost {:.2f}",
		           _path.Laps(), _path.Index(), _path.Count(), position.X, position.Y, position.Z, speed, targetSpeed,
		           aim.X, aim.Y, aim.Z, pursuit, throttle, brake, steer, _boost);
	}
}

float RaceOpponent::cornerRadius(std::ptrdiff_t offset) const
{
	const Vector3& a = _path.PeekSigned(offset - 1);
	const Vector3& b = _path.PeekSigned(offset);
	const Vector3& c = _path.PeekSigned(offset + 1);

	// Radius of the circle through the three points, in the ground plane.
	const float abx = b.X - a.X, abz = b.Z - a.Z;
	const float bcx = c.X - b.X, bcz = c.Z - b.Z;
	const float acx = c.X - a.X, acz = c.Z - a.Z;

	const float ab = std::sqrt(abx * abx + abz * abz);
	const float bc = std::sqrt(bcx * bcx + bcz * bcz);
	const float ac = std::sqrt(acx * acx + acz * acz);

	// Twice the triangle's area, and zero when the three are in line.
	const float area2 = std::fabs(abx * bcz - abz * bcx);
	if (area2 < 1.0e-3f || ab < 0.01f || bc < 0.01f)
		return kStraightRadius;

	const float radius = (ab * bc * ac) / (2.0f * area2);
	return radius > kStraightRadius ? kStraightRadius : radius;
}

float RaceOpponent::tightestRadiusAhead(float metres) const
{
	float tightest = kStraightRadius;

	Vector3 previous = _vehicle.GetPosition();
	float arc = 0.0f;

	for (std::size_t step = 0; step < kProfileWaypoints; ++step)
	{
		Vector3 leg = _path.Peek(step) - previous;
		leg.Y = 0.0f;
		arc += leg.Length();
		previous = _path.Peek(step);

		if (arc > metres)
			break;

		const float radius = cornerRadius(static_cast<std::ptrdiff_t>(step));
		if (radius < tightest)
			tightest = radius;
	}

	return tightest < kMinCornerRadius ? kMinCornerRadius : tightest;
}

float RaceOpponent::targetSpeedKmh(float boost) const
{
	// Rubber-banding raises what the car aims for on a clear road. It does not
	// raise the corner limits below: those are what the tyres will hold, and a
	// car sent into a bend 60% over that leaves the road however far behind it is.
	const float straight = kStraightSpeedKmh * boost;
	float limit = straight;

	Vector3 previous = _vehicle.GetPosition();
	float arc = 0.0f;

	for (std::size_t step = 0; step < kProfileWaypoints; ++step)
	{
		const Vector3& waypoint = _path.Peek(step);

		Vector3 leg = waypoint - previous;
		leg.Y = 0.0f;
		arc += leg.Length();
		previous = waypoint;

		if (arc > kProfileMetres)
			break;

		float radius = cornerRadius(static_cast<std::ptrdiff_t>(step));
		if (radius < kMinCornerRadius)
			radius = kMinCornerRadius;

		// What the grip allows through that corner...
		float corner = std::sqrt(kLateralAccelMss * radius) * 3.6f;
		if (corner > straight)
			corner = straight;

		// ...and the fastest we may be going here and still have shed the
		// difference by the time we reach it.
		const float cornerMs = corner / 3.6f;
		const float allowed = std::sqrt(cornerMs * cornerMs + 2.0f * kBrakeDecelMss * arc) * 3.6f;
		if (allowed < limit)
			limit = allowed;
	}

	return limit < kCornerSpeedKmh ? kCornerSpeedKmh : limit;
}

std::string RaceOpponent::describeObstacle() const
{
	WorldPhysics* physics = _vehicle.GetPhysics();
	if (physics == nullptr)
		return {};

	// Start the ray clear of the car's own chassis (a 2.2m half-length box) and
	// look along the way it is pointing, at about bumper height.
	Vector3 forward = _vehicle.GetRotation() * Vector3::Forward;
	forward.Y = 0.0f;
	if (forward.LengthSquared() < 0.01f)
		return {};
	forward.Normalize();

	const Vector3 position = _vehicle.GetPosition();
	const Vector3 from = position + forward * kObstacleRayStart + Vector3(0.0f, kObstacleRayHeight, 0.0f);

	Vector3 point, normal;
	if (!physics->CastRay(from, forward, kObstacleRayRange, point, normal))
		return ", nothing in front of it";

	// A near-vertical face is a wall the car will never climb; a shallow one is a
	// kerb or a ramp it is merely struggling with.
	const float rise = std::fabs(normal.Y);
	return fmt::format(", blocked {:.1f}m ahead at ({:.1f}, {:.1f}, {:.1f}) by a {} (normal {:.2f}, {:.2f}, {:.2f})",
	                   (point - position).Length(), point.X, point.Y, point.Z, rise < 0.5f ? "wall" : "slope",
	                   normal.X, normal.Y, normal.Z);
}

void RaceOpponent::respawnOnCircuit(float clearanceMetres)
{
	// Putting the car back on the waypoint it is stuck at drops it straight back
	// into whatever stopped it, and it wedges again inside three seconds -- at
	// (231, 4.2, -336) the circuit runs *under* the road deck, and the car spent
	// the whole race there being reversed out and driven back in. When the trap is
	// the route itself, the only way on is past it: walk the waypoints forward
	// until one is clear of where the car gave up, and restart from there.
	if (clearanceMetres > 0.0f)
	{
		const Vector3 gaveUp = _vehicle.GetPosition();
		const std::size_t from = _path.Index();

		for (std::size_t skipped = 0; skipped < _path.Count(); ++skipped)
		{
			Vector3 delta = _path.Target() - gaveUp;
			delta.Y = 0.0f;
			if (delta.Length() >= clearanceMetres)
				break;
			_path.Skip();
		}

		if (_path.Index() != from)
			Log::Warn("RaceOpponent: '{}' skipping waypoints {}..{} -- it cannot get through there",
			          _vehicle.GetName(), from, _path.Index());
	}

	const Vector3 here = _path.Target();
	const Vector3 next = _path.Peek(1);

	Vector3 forward = next - here;
	forward.Y = 0.0f;
	Quaternion rotation = _vehicle.GetRotation();
	if (forward.LengthSquared() > 0.01f)
		rotation = Quaternion(Vector3(0.0f, 1.0f, 0.0f), std::atan2(forward.X, forward.Z));

	_vehicle.Teleport(here, rotation);
	_lastPosition = here;
	_noProgressTimer = 0.0;
	_stuckTimer = 0.0;
	_stuckTravel = 0.0f;
	_reverseTimer = 0.0;
}

} // namespace Donut
