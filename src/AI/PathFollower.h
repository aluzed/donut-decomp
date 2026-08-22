// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"

#include <cstddef>
#include <vector>

namespace Donut
{

// Steering primitives shared by every path-following agent. TrafficManager grew
// its own file-static copies of these; they live here so a second agent does not
// have to grow a third.
namespace Steering
{
// Signed steering in [-maxSteer, maxSteer] to turn `rotation` toward `target`.
// Positive turns one way, negative the other; the sign convention is whatever
// Vehicle::ApplyInput expects for its steer argument.
float Seek(const Vector3& position, const Vector3& target, const Quaternion& rotation, float maxSteer);

// Target speed that eases off inside `slowDistance` of the target.
float ArrivalSpeed(const Vector3& position, const Vector3& target, float maxSpeed, float slowDistance);
} // namespace Steering

// Walks an agent through a closed loop of waypoints. Holds no agent state beyond
// the index, so the caller decides what to do with the target.
class PathFollower
{
public:
	PathFollower() = default;
	explicit PathFollower(std::vector<Vector3> waypoints): _waypoints(std::move(waypoints)) {}

	void SetWaypoints(std::vector<Vector3> waypoints);
	bool Empty() const { return _waypoints.empty(); }
	std::size_t Count() const { return _waypoints.size(); }

	const Vector3& Target() const { return _waypoints[_index]; }
	std::size_t Index() const { return _index; }
	int Laps() const { return _laps; }

	// Advances to the next waypoint when `position` is within `radius` of the
	// current one, wrapping around and counting a lap. Returns true on advance.
	bool Advance(const Vector3& position, float radius);

	// Progress along the loop as a monotonic float (laps * count + index), for
	// comparing two agents' positions in the race.
	float Progress() const { return static_cast<float>(_laps) * static_cast<float>(_waypoints.size()) + _index; }

	void Reset();

private:
	std::vector<Vector3> _waypoints;
	std::size_t _index = 0;
	int _laps = 0;
};

} // namespace Donut
