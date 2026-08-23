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
	const float cross = forward.X * desired.Z - forward.Z * desired.X;

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

void PathFollower::SetWaypoints(std::vector<Vector3> waypoints)
{
	_waypoints = std::move(waypoints);
	Reset();
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
