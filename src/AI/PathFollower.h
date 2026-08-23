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
	// The waypoint `ahead` places further along the loop, wrapping around.
	const Vector3& Peek(std::size_t ahead) const { return _waypoints[(_index + ahead) % _waypoints.size()]; }
	// The same for a signed offset, so a corner's radius can be measured from the
	// waypoint before it as well as the one after.
	const Vector3& PeekSigned(std::ptrdiff_t offset) const
	{
		const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(_waypoints.size());
		std::ptrdiff_t i = (static_cast<std::ptrdiff_t>(_index) + offset) % n;
		if (i < 0) i += n;
		return _waypoints[static_cast<std::size_t>(i)];
	}

	// The point `distance` metres along the route, measured from `from` through
	// the current waypoint and on around the loop. This is the aim point for pure
	// pursuit: steering at the next waypoint instead makes the agent swing wide,
	// snap back and saw at the wheel, because the waypoint jumps sideways the
	// instant it is captured. An aim point that slides along the route is
	// continuous, and it cuts the corner to the apex on its own -- the chord of a
	// bend lies inside it.
	Vector3 PointAhead(const Vector3& from, float distance) const;
	std::size_t Index() const { return _index; }
	int Laps() const { return _laps; }

	// Advances to the next waypoint when `position` is within `radius` of the
	// current one, wrapping around and counting a lap. Returns true on advance.
	bool Advance(const Vector3& position, float radius);

	// Distance from `position` to the leg being driven -- the segment from the
	// previous waypoint to the current one. Distance to the waypoint itself is no
	// use as an off-route test: it is legitimately large at the start of a long
	// leg, and a threshold under the longest leg makes the agent rejoin the route
	// it is already on, over and over.
	float CrossTrackDistance(const Vector3& position) const;

	// Starts from the waypoint nearest `position` instead of index 0. An agent
	// placed part-way along the loop otherwise aims at waypoint 0 behind it and
	// drives away from the circuit at full lock.
	void SnapToNearest(const Vector3& position);

	// The same, but never picks a waypoint already behind: it searches the next
	// `window` waypoints only, counting a lap if it wraps. Rejoining a route must
	// not go backwards -- the nearest waypoint to a car that has just overshot a
	// corner is the one it came from, and re-targeting it undoes the progress the
	// pass-the-plane test in Advance had just made, forever.
	void SnapToNearestAhead(const Vector3& position, std::size_t window);

	// Steps to the next waypoint unconditionally, counting a lap on wrap. Used to
	// walk past a stretch of circuit the agent has proved it cannot drive.
	void Skip();

	// Progress along the loop as a monotonic float (laps * count + index), for
	// comparing two agents' positions in the race.
	float Progress() const { return static_cast<float>(_laps) * static_cast<float>(_waypoints.size()) + _index; }

	// The same thing in laps, so it can be compared with an agent running a
	// different number of waypoints -- the opponent's circuit has 30 points where
	// the player's has 6 checkpoints, and comparing the raw counts made the
	// opponent permanently "ahead" by a factor of five.
	float ProgressLaps() const
	{
		if (_waypoints.empty())
			return static_cast<float>(_laps);
		return static_cast<float>(_laps) + static_cast<float>(_index) / static_cast<float>(_waypoints.size());
	}

	void Reset();

private:
	std::vector<Vector3> _waypoints;
	std::size_t _index = 0;
	int _laps = 0;
};

} // namespace Donut
