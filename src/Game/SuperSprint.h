#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"
#include "Level.h"

#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Donut
{

class Mesh;
class PathGraph;
class RaceOpponent;
class Vehicle;
class WorldPhysics;
namespace GL
{
class ShaderProgram;
}

// The bonus game (the game's SuperSprint): four cars round one of seven small
// tracks (art/b01.p3d to B07.p3d), seen from far above, for so many laps.
//
// A track's own file holds its world, roads and camera (sprint_camShape); its
// data file (b0Ndata.p3d) its locators:
// - car1 to car4, the grid, facing the way the track runs (car starts);
// - its checkpoints (event locators, event 2), each with its place round the
//   lap in its second data word: "finish" is 1, the others follow. Run the
//   other way round, they are met in the opposite order;
// - the AI's waypoints, WPR01... one way round, WPL01... the other;
// - its pickups (action locators): "Nitro", a boost to use when its holder
//   likes; "Wrench", the car mended;
// - the places a car is lost (event 4, "death": off the track's edge, into
//   the water), where it is put back at the last checkpoint it passed;
// - its jumps (event 0x30): the run-up to a ramp, which the computer takes
//   floored.
// A car that is wrecked, stuck or fallen is put back there too.
class SuperSprint
{
public:
	static constexpr int kRacers = 4;
	static constexpr int kTracks = 7;

	// Who drives each car: a player (0 to 3), or the computer (-1); as who,
	// and in what.
	struct Entry
	{
		int player = -1;
		std::string character = "homer";
		std::string car = "famil_v";
	};
	struct Setup
	{
		int track = 1;
		bool reverse = false;
		int laps = 3;
		std::array<Entry, kRacers> entries;
	};
	// A player's hands on the wheel, this frame.
	struct Controls
	{
		float gas = 0.0f;
		float brake = 0.0f;
		float steer = 0.0f;
		// Pressed this frame.
		bool nitro = false;
	};

	// The files of track `track` (1 to 7), its world then its data, for the
	// level to load before the race is made.
	static std::vector<std::string> TrackFiles(int track);

	// The track loaded in `level`, its collision in `physics`, its roads in
	// `roads`.
	SuperSprint(Level& level, WorldPhysics& physics, const PathGraph& roads, const Setup& setup);
	~SuperSprint();

	enum class Phase
	{
		Countdown,
		Racing,
		// Everyone over the line, or the players all over it and a while
		// given the others.
		Finished,
	};
	Phase GetPhase() const { return _phase; }
	// The countdown's seconds left, 3 to 0.
	double GetCountdown() const { return _countdown; }
	double GetRaceTime() const { return _raceTime; }

	void Update(double dt, const std::array<Controls, kRacers>& controls);
	// The cars and the pickups.
	void Draw(const Matrix4x4& viewProj, GL::ShaderProgram& worldShader, float time);

	struct Racer
	{
		Entry entry;
		std::unique_ptr<Vehicle> vehicle;
		std::unique_ptr<RaceOpponent> ai;
		// Laps done, and the checkpoint looked for next (its place in the
		// lap's order, 0 the finish).
		int laps = 0;
		int next = 1;
		// Where it stands in the race, 1 to 4.
		int place = 1;
		bool finished = false;
		double totalTime = 0.0;
		double lapStartedAt = 0.0;
		double bestLap = -1.0;
		int nitros = 0;
		// Burning one, for so much longer.
		double nitroFor = 0.0;
		// 1 whole, 0 wrecked.
		float health = 1.0f;
		// Being put back, for so much longer; where it goes.
		double lostFor = 0.0;
		// How far round, in checkpoints: laps times their count, plus those
		// passed, plus the way to the next.
		float progress = 0.0f;
		// The last checkpoint it passed, where it is put back, facing on.
		Vector3 safePosition;
		Quaternion safeRotation;
		double stuckFor = 0.0;
		Vector3 stuckAt;
		// Where it was last put back from, stuck.
		Vector3 lastStuckAt = Vector3(1.0e6f, 0.0f, 0.0f);
		// When it was last put back.
		double putBackAt = -100.0;
		Vector3 lastVelocity;
	};
	const std::vector<Racer>& GetRacers() const { return _racers; }
	int GetLaps() const { return _setup.laps; }
	const Setup& GetSetup() const { return _setup; }

	// The track's camera, far off with a long lens (sprint_camShape): where it
	// stands, the way it looks, its up, its field of view (radians, across).
	struct Camera
	{
		Vector3 position = Vector3(739.8f, 1132.7f, -1737.8f);
		Vector3 look = Vector3(-0.337f, -0.512f, 0.790f);
		Vector3 up = Vector3(0.0f, 0.84f, 0.54f);
		float fov = 0.087f;
		float nearPlane = 100.0f, farPlane = 4000.0f;
	};
	const Camera& GetCamera() const { return _camera; }

	// What happened this frame, to be heard.
	struct Events
	{
		std::vector<int> lapsDone, nitrosUsed, pickedNitro, pickedWrench, lost;
		bool started = false;
		// The countdown's second just begun (3, 2, 1), 0 for none.
		int countdownTick = 0;
	};
	const Events& GetEvents() const { return _events; }

private:
	struct Checkpoint
	{
		std::string locator;
		int order = 0;
		Vector3 position;
	};
	struct Pickup
	{
		Vector3 position;
		bool nitro = false;
		// Gone, back in so much time.
		double goneFor = 0.0;
	};

	Level& _level;
	WorldPhysics& _physics;
	Setup _setup;
	Camera _camera;
	Phase _phase = Phase::Countdown;
	double _countdown = 3.0;
	double _raceTime = 0.0;
	double _finishedFor = 0.0;
	std::vector<Racer> _racers;
	// In the order they are met, the finish first.
	std::vector<Checkpoint> _checkpoints;
	std::vector<Pickup> _pickups;
	std::vector<std::string> _lostZones;
	// Its ramps' run-ups: the computer floored through them.
	std::vector<std::string> _jumps;
	float _lowest = 0.0f;
	Events _events;
	std::set<Mesh*> _committed;

	void readTrack(int track);
	void readCamera(int track);
	std::vector<Vector3> aiCircuit(const PathGraph& roads, int track) const;
	bool inside(const std::string& locator, const Vector3& position) const;
	bool overLostZone(const Vector3& position) const;
	void updateRacer(Racer& racer, int index, double dt, const Controls& controls, float leaderProgress);
	void putBack(Racer& racer);
	void rank();
	Mesh* committed(const std::string& name);
};

} // namespace Donut
