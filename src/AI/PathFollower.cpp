// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/PathFollower.h"

namespace Donut
{
namespace Steering
{

float Seek(const Vector3& position, const Vector3& target, const Quaternion& rotation, float maxSteer)
{
	Vector3 desired = target - position;
	desired.Y = 0.0f;
	if (desired.LengthSquared() < 0.01f)
		return 0.0f;

	desired.Normalize();

	const Vector3 forward = rotation * Vector3::Forward;
	const float dot = forward.X * desired.X + forward.Z * desired.Z;

	// Sign convention: positive steering must turn the car toward +X in its own
	// frame, because that is what btRaycastVehicle::setSteeringValue does -- it
	// rotates the front wheels about +Y, taking forward (0,0,1) to (sin, 0, cos).
	// This cross product was the other way round, so the opponent steered away
	// from every waypoint: the error grew instead of closing and it spiralled off
	// the road into the nearest fence. Wrong only in the sign, which is why it
	// still looked like it was trying.
	const float cross = forward.Z * desired.X - forward.X * desired.Z;

	// Proportional near the heading, saturated beyond it, and full lock when the
	// target is behind us so the agent commits to a turn instead of dithering.
	float steer = cross * 2.0f;
	if (steer > 1.0f) steer = 1.0f;
	if (steer < -1.0f) steer = -1.0f;
	if (dot < 0.0f)
		steer = cross >= 0.0f ? 1.0f : -1.0f;

	return steer * maxSteer;
}

float ArrivalSpeed(const Vector3& position, const Vector3& target, float maxSpeed, float slowDistance)
{
	Vector3 delta = target - position;
	delta.Y = 0.0f;
	const float dist = delta.Length();

	if (dist < 0.5f)
		return 0.0f;
	if (slowDistance > 0.0f && dist < slowDistance)
		return maxSpeed * (dist / slowDistance);
	return maxSpeed;
}

} // namespace Steering

Vector3 PathFollower::PointAhead(const Vector3& from, float distance) const
{
	if (_waypoints.empty())
		return from;

	Vector3 previous = from;
	float remaining = distance;

	// Walk the route from where the agent is, spending `distance` metres of it.
	for (std::size_t step = 0; step < _waypoints.size(); ++step)
	{
		const Vector3& waypoint = _waypoints[(_index + step) % _waypoints.size()];

		Vector3 leg = waypoint - previous;
		leg.Y = 0.0f;
		const float length = leg.Length();

		if (length >= remaining && length > 0.001f)
			return previous + leg * (remaining / length);

		remaining -= length;
		previous = waypoint;
	}

	// The whole loop is shorter than the look-ahead: aim at the far end of it.
	return previous;
}

void PathFollower::SetWaypoints(std::vector<Vector3> waypoints)
{
	_waypoints = std::move(waypoints);
	Reset();
}

float PathFollower::CrossTrackDistance(const Vector3& position) const
{
	if (_waypoints.empty())
		return 0.0f;

	const Vector3& target = _waypoints[_index];
	const Vector3& previous = _waypoints[(_index + _waypoints.size() - 1) % _waypoints.size()];

	Vector3 leg = target - previous;
	leg.Y = 0.0f;
	Vector3 offset = position - previous;
	offset.Y = 0.0f;

	const float legLengthSq = leg.LengthSquared();
	if (legLengthSq < 0.01f)
		return offset.Length();

	// Where along the leg we are, clamped to its ends so the distance is to the
	// segment and not to the infinite line through it.
	float t = (offset.X * leg.X + offset.Z * leg.Z) / legLengthSq;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	return (offset - leg * t).Length();
}

void PathFollower::SnapToNearest(const Vector3& position)
{
	if (_waypoints.empty())
		return;

	std::size_t best = 0;
	float bestDist = (_waypoints[0] - position).LengthSquared();
	for (std::size_t i = 1; i < _waypoints.size(); ++i)
	{
		const float d = (_waypoints[i] - position).LengthSquared();
		if (d < bestDist)
		{
			bestDist = d;
			best = i;
		}
	}

	_index = best;
}

void PathFollower::SnapToNearestAhead(const Vector3& position, std::size_t window)
{
	if (_waypoints.empty())
		return;

	const std::size_t span = window < _waypoints.size() ? window : _waypoints.size();
	std::size_t bestStep = 0;
	float bestDist = (_waypoints[_index] - position).LengthSquared();
	for (std::size_t step = 1; step < span; ++step)
	{
		const std::size_t i = (_index + step) % _waypoints.size();
		const float d = (_waypoints[i] - position).LengthSquared();
		if (d < bestDist)
		{
			bestDist = d;
			bestStep = step;
		}
	}

	if (_index + bestStep >= _waypoints.size())
		++_laps;
	_index = (_index + bestStep) % _waypoints.size();
}

void PathFollower::Skip()
{
	if (_waypoints.empty())
		return;

	++_index;
	if (_index >= _waypoints.size())
	{
		_index = 0;
		++_laps;
	}
}

void PathFollower::Reset()
{
	_index = 0;
	_laps = 0;
}

bool PathFollower::Advance(const Vector3& position, float radius)
{
	if (_waypoints.empty())
		return false;

	Vector3 delta = _waypoints[_index] - position;
	delta.Y = 0.0f; // waypoints sit on the road surface, the agent above it

	bool reached = delta.LengthSquared() <= radius * radius;

	// A car that takes a corner wide misses the radius entirely: the race
	// opponent came within 14m of waypoint 1, never captured it, and drove on in
	// a straight line until it left the map. Also count the waypoint as reached
	// once the agent is past it -- past the plane through it, square to the leg it
	// arrived on -- so overshooting costs a wide line, not the whole race.
	if (!reached)
	{
		const std::size_t previous = (_index + _waypoints.size() - 1) % _waypoints.size();
		Vector3 leg = _waypoints[_index] - _waypoints[previous];
		leg.Y = 0.0f;
		if (leg.LengthSquared() > 0.01f && (delta.X * leg.X + delta.Z * leg.Z) < 0.0f)
			reached = true;
	}

	if (!reached)
		return false;

	++_index;
	if (_index >= _waypoints.size())
	{
		_index = 0;
		++_laps;
	}
	return true;
}

} // namespace Donut
