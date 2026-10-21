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
	// `open`: the route runs start to finish rather than round a loop. The car
	// stops once it reaches the end, which counts as its one lap.
	// `joinAhead`: it joins the circuit at the nearest point ahead of it, not
	// the nearest.
	RaceOpponent(Vehicle& vehicle, std::vector<Vector3> circuit, bool open = false, bool joinAhead = false);

	// `playerProgress` is how far the player is round the race *in laps*; pass a
	// negative value to disable rubber-banding.
	void Update(double dt, float playerProgress);

	// The mission's own route is only known once the .con has finished declaring
	// its waypoints, which happens after the vehicle is created.
	void SetCircuit(std::vector<Vector3> circuit);

	// Sits on the line, brakes on, during the countdown.
	void Hold();

	float Progress() const { return _path.Progress(); }
	float ProgressLaps() const { return _path.ProgressLaps(); }
	int Laps() const { return _path.Laps(); }
	const Vehicle& GetVehicle() const { return _vehicle; }

	// The circuit it drives, from where it joined it.
	const std::vector<Vector3>& GetCircuit() const { return _path.GetWaypoints(); }
	// A boost on top of its own (the bonus game's nitro): its engine's pull and
	// top speed times `boost` while it lasts; 1 for none.
	void SetExtraBoost(float boost) { _extraBoost = boost; }
	// Floored, whatever the road ahead: a ramp to clear (the bonus game's
	// jumps).
	void SetFloored(bool floored) { _floored = floored; }

	// Last throttle scale applied, for HUD/debug readouts.
	float BoostScale() const { return _boost; }

	// A mission's rubber-banding, from SetStageAIRaceCatchupParams. The car's
	// boost runs from `ahead` (player `distanceMetres` or more behind it) through
	// `level` (side by side) to `behind` (player that far ahead of it). Until a
	// mission sets these the opponent uses the built-in per-lap rubber band.
	struct Catchup
	{
		float distanceMetres;
		float ahead;
		float level;
		float behind;
	};
	void SetCatchup(const Catchup& catchup);

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
	// Takes `circuit` re-indexed to start at the point nearest the car.
	void startFromHere(std::vector<Vector3> circuit);
	// Counts a wedge at `position`, and puts the car back on the circuit past it
	// once it has wedged in the same spot too often. True when it did.
	bool wedgedAgain(const Vector3& position);
	// What the car is pressed against, for the log line that reports it stuck.
	std::string describeObstacle() const;
	// Puts the car back on the circuit facing the next waypoint. With a non-zero
	// clearance it first walks the route forward until the waypoint is that far
	// from where the car gave up, so it is not dropped back into the same trap.
	void respawnOnCircuit(float clearanceMetres);

	Vehicle& _vehicle;
	PathFollower _path;

	float _boost = 1.0f;
	float _extraBoost = 1.0f;
	bool _floored = false;
	double _stuckTimer = 0.0;
	float _stuckTravel = 0.0f;
	double _reverseTimer = 0.0;
	// How long the car has sat still while turning round (see kTurnStallSeconds).
	double _turnStallTimer = 0.0;
	double _noProgressTimer = 0.0;
	int _wedgeRepeats = 0;
	Vector3 _lastWedgePosition = Vector3(1.0e6f, 0.0f, 1.0e6f);
	float _lastWedgeProgress = 0.0f;
	double _logTimer = 0.0;
	bool _open = false;
	bool _joinAhead = false;
	bool _hasCatchup = false;
	Catchup _catchup {};
	// Length of the circuit once round, to turn a deficit in laps into metres.
	float _circuitMetres = 0.0f;
	// Time since the current lap began, and the lap it is timing.
	double _lapTimer = 0.0;
	int _timedLap = 0;
	Vector3 _lastPosition;
};

} // namespace Donut
