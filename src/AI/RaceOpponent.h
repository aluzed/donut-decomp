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

	// `playerProgress` is the player's PathFollower::Progress()-style position on
	// the same circuit; pass a negative value to disable rubber-banding.
	void Update(double dt, float playerProgress);

	float Progress() const { return _path.Progress(); }
	int Laps() const { return _path.Laps(); }
	const Vehicle& GetVehicle() const { return _vehicle; }

	// Last throttle scale applied, for HUD/debug readouts.
	float BoostScale() const { return _boost; }

private:
	Vehicle& _vehicle;
	PathFollower _path;

	float _boost = 1.0f;
	double _stuckTimer = 0.0;
	Vector3 _lastPosition;
};

} // namespace Donut
