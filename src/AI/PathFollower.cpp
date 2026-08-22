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
	if (delta.LengthSquared() > radius * radius)
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
