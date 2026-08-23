// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "AI/PathFollower.h"
#include "Core/Math/Vector3.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Donut
{

class Vehicle;

// Drives a real Vehicle around a circuit through Vehicle::ApplyInput, so the
// opponent obeys the same physics as the player instead of sliding along on a
// kinematic integration.
class RaceOpponent
{
public:
	RaceOpponent(Vehicle& vehicle, std::vector<Vector3> circuit);

	// `playerProgress` is how far the player is round the race *in laps*; pass a
	// negative value to disable rubber-banding.
	void Update(double dt, float playerProgress);

	// The mission's own route is only known once the .con has finished declaring
	// its waypoints, which happens after the vehicle is created.
	void SetCircuit(std::vector<Vector3> circuit);

	float Progress() const { return _path.Progress(); }
	float ProgressLaps() const { return _path.ProgressLaps(); }
	int Laps() const { return _path.Laps(); }
	const Vehicle& GetVehicle() const { return _vehicle; }

	// Last throttle scale applied, for HUD/debug readouts.
	float BoostScale() const { return _boost; }

private:
	// Speed the route ahead can be taken at. Every corner within the look-ahead
	// gets a grip limit from its radius, and each of those is carried back to here
	// through the braking distance needed to meet it; the lowest wins. `boost`
	// raises the straight-line ambition only -- rubber-banding must not talk the
	// car into a corner faster than its tyres will hold.
	float targetSpeedKmh(float boost) const;
	// Tightest bend within `metres` of route ahead, which sets both how hard the
	// car may corner and how far ahead it dares aim.
	float tightestRadiusAhead(float metres) const;
	// Radius of the bend at the waypoint `offset` places along the route, from the
	// circle through it and its two neighbours. Huge when they are in line.
	float cornerRadius(std::ptrdiff_t offset) const;
	// What the car is pressed against, for the log line that reports it stuck.
	std::string describeObstacle() const;
	// Puts the car back on the circuit facing the next waypoint. With a non-zero
	// clearance it first walks the route forward until the waypoint is that far
	// from where the car gave up, so it is not dropped back into the same trap.
	void respawnOnCircuit(float clearanceMetres);

	Vehicle& _vehicle;
	PathFollower _path;

	float _boost = 1.0f;
	double _stuckTimer = 0.0;
	float _stuckTravel = 0.0f;
	double _reverseTimer = 0.0;
	double _noProgressTimer = 0.0;
	int _wedgeRepeats = 0;
	Vector3 _lastWedgePosition = Vector3(1.0e6f, 0.0f, 1.0e6f);
	double _logTimer = 0.0;
	Vector3 _lastPosition;
};

} // namespace Donut
