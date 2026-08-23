// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "AI/PathFollower.h"
#include "Core/Math/Vector3.h"

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
	// Speed the route ahead can be taken at: fast on a straight, slow into a bend.
	float cornerSpeedKmh() const;
	// Puts the car back on the circuit at its current waypoint, facing the next.
	void respawnOnCircuit();

	Vehicle& _vehicle;
	PathFollower _path;

	float _boost = 1.0f;
	double _stuckTimer = 0.0;
	float _stuckTravel = 0.0f;
	double _reverseTimer = 0.0;
	int _reverseAttempts = 0;
	double _logTimer = 0.0;
	Vector3 _lastPosition;
};

} // namespace Donut
