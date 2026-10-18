// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "ScriptEngine.h"
#include "AI/ChaseManager.h"
#include "AI/PathGraph.h"
#include "AI/RaceOpponent.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "Level.h"
#include "Physics/WorldPhysics.h"
#include "Scripting/Commands.h"
#include "Vehicle.h"

#include <cctype>
#include <cmath>
#include <fstream>

namespace Donut
{

// Out of line so RaceOpponent only needs to be complete here, not in every
// translation unit that destroys a ScriptEngine.
ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::SelectMission(const std::string& id)
{
	if (_missionActive)
		return;

	_missionActive = true;
	_missionId = id;

	// Mission locators (spawn points, car starts, AI path nodes, race_finish...)
	// live in the mission's own P3D files, which nothing loaded: level.p3d holds
	// the level-wide ones, <id>.p3d the mission's own. Every "locator not found"
	// warning in the log came from here, and AddStageWaypoint could never resolve
	// its argument.
	// TODO: level01 is hard-coded because the engine only loads level 1 so far.
	_game.GetLevel().LoadP3D("missions/level01/level.p3d");
	_game.GetLevel().LoadP3D("missions/level01/" + id + ".p3d");

	_currentStage = -1;
	_playedStage = -1;
	_noTraffic = false;
	_zones.clear();

	// Any vehicle still here was never torn down (CleanupMission is what does
	// that). Dropping the unique_ptrs would leave its rigid body and action
	// behind in the physics world, dangling: the first run left a car falling
	// forever, reaching Y = -440.
	_raceOpponents.clear();
	_raceVehicles.clear();
	_raceCatchups.clear();
	_playerCar = nullptr;
	_stageWaypoints.clear();
	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_countdown.clear();
	_countdownElapsed = 0.0f;
	_stageTimeRemaining = -1.0f;

	std::ifstream saveFile("donut_save.dat");
	if (saveFile.good())
	{
		saveFile >> _bestTime;
		saveFile.ignore(1024, '\n');
		Log::Info("ScriptEngine: loaded best time {:.1f}s", _bestTime);
	}

	Log::Info("ScriptEngine: mission {} started", id);
}

void ScriptEngine::SetMissionResetPlayerInCar(const std::string& locator)
{
	_playerLocator = locator;
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos != Vector3::Zero)
	{
		_game.SetPlayerPosition(pos);
		_playerStart = pos;
		_hasPlayerStart = true;
		Log::Info("ScriptEngine: player placed at locator '{}' ({:.1f}, {:.1f}, {:.1f})", locator, pos.X, pos.Y, pos.Z);
	}
	else
	{
		Log::Warn("ScriptEngine: locator '{}' not found!", locator);
	}
}

void ScriptEngine::SetMissionResetPlayerOutCar(const std::string& locator)
{
	_playerLocator = locator;
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos != Vector3::Zero)
	{
		_game.SetPlayerPosition(pos);
		_playerStart = pos;
		_hasPlayerStart = true;
		Log::Info("ScriptEngine: player placed (out of car) at '{}' ({:.1f}, {:.1f}, {:.1f})", locator, pos.X, pos.Y, pos.Z);
	}
}

void ScriptEngine::SetDynaLoadData(const std::string& zones)
{
	_zones.push_back(zones);
	_game.GetLevel().DynaLoadData(zones);
}

void ScriptEngine::AddStage(int index)
{
	_currentStage = index;
	if (_playedStage < 0)
		_playedStage = index;
	Log::Info("ScriptEngine: stage {} begin", index);
}

void ScriptEngine::CloseStage()
{
	// Ends the stage *declaration* in the .con file, not the stage itself. The
	// script is a description of the mission; the stage it just described is the
	// one the player is about to attempt.
	Log::Info("ScriptEngine: stage {} declared", _currentStage);
}

void ScriptEngine::CloseMission()
{
	// Likewise: this closes the mission definition block, so the mission is now
	// set up and running. It used to set GameState::MissionComplete here, which
	// meant every .con finished its own mission the instant it finished parsing:
	// the race announced "STAGE COMPLETE! Time: 0.0s", the 5s retry timer fired,
	// the script re-ran, and it looped forever with no vehicle ever reachable.
	// Completion belongs to gameplay -- see AdvanceCheckpoint/ShowStageComplete.
	buildRaceCircuit();

	Log::Info("ScriptEngine: mission '{}' loaded, stage {} active", _missionId, _currentStage);
}

void ScriptEngine::CleanupMission()
{
	_raceOpponents.clear();
	_raceVehicles.clear();
	_raceCatchups.clear();
	_playerCar = nullptr;
	_racePath.clear();
	_stageWaypoints.clear();

	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_activeVehicle = nullptr;
	_chaseManager.reset();
	_objectiveType.clear();
	_objectiveTarget.clear();
	_stageTimeRemaining = -1.0f;
	_countdown.clear();
	_countdownElapsed = 0.0f;

	// Tearing the mission down is what clears the active flag; CloseMission used
	// to do it as a side effect of wrongly completing the mission, and without
	// this SelectMission's `if (_missionActive) return` would refuse to reload.
	_missionActive = false;
	_currentStage = -1;
	_playedStage = -1;
	_noTraffic = false;

	Log::Info("ScriptEngine: mission cleaned up");
}

void ScriptEngine::SetStageTime(float seconds)
{
	if (!declaringPlayedStage())
	{
		Log::Debug("ScriptEngine: stage {} time {}s set aside, stage {} is the one played", _currentStage, seconds,
		           _playedStage);
		return;
	}
	_stageTimeRemaining = seconds;
	_initialStageTime = seconds;
	Log::Info("ScriptEngine: stage time = {}s", seconds);
}

void ScriptEngine::Update(double dt)
{
	// The stage clock starts when the countdown has run out.
	if (_countdownElapsed < countdownLength())
	{
		_countdownElapsed += static_cast<float>(dt);
		return;
	}

	if (!_missionActive || _stageTimeRemaining <= 0.0f) return;

	if (_chaseManager)
	{
		Vector3 playerPos = _game.GetPlayerPosition();
		_chaseManager->Update(dt, playerPos);
		if (_chaseManager->IsBusted())
		{
			Log::Info("ScriptEngine: BUSTED!");
			_game.SetState(GameState::MissionFailed);
			return;
		}
	}

	_stageTimeRemaining -= static_cast<float>(dt);
	if (_stageTimeRemaining <= 0.0f)
	{
		_stageTimeRemaining = 0.0f;
		Log::Info("ScriptEngine: stage timer expired - mission failed!");
		_game.AddShake(2.0f);
		_game.GetAudioManager().PlayRaw(SoundGenerator::Beep(150, 0.5f), 22050, 1, 16);
		_game.SetState(GameState::MissionFailed);
	}
}

void ScriptEngine::AddCharacter(const std::string& name, const std::string& locator)
{
	Log::Info("ScriptEngine: add character '{}' at '{}'", name, locator);
	if (name == "homer")
		_game.LoadModel("homer", "homer");
}

void ScriptEngine::AddNPC(const std::string& name, const std::string& locator, const std::string& type)
{
	Log::Info("ScriptEngine: add NPC '{}' at '{}' type='{}'", name, locator, type);
}

void ScriptEngine::PlacePlayerCar(const std::string& car, const std::string& locator)
{
	const Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: player car locator '{}' not found", locator);
		return;
	}

	// "current" is whatever the player drives at the time. There is no garage
	// yet, so it is the family sedan the game starts the player in.
	const std::string model = car == "current" ? "famil_v" : car;
	Log::Info("ScriptEngine: place player car '{}' ('{}') at '{}' ({:.1f}, {:.1f}, {:.1f})", car, model, locator,
	          pos.X, pos.Y, pos.Z);
	_playerCar = &spawnVehicle(model, locator, pos);
}

void ScriptEngine::AddCollectible(const std::string& locator, const std::string& type)
{
	if (_objectiveType != "race" || !declaringPlayedStage())
	{
		Log::Debug("ScriptEngine: collectible '{}' ({}) outside a race, not handled", locator, type);
		return;
	}

	const Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: race checkpoint '{}' not found", locator);
		return;
	}

	// The first one replaces the six points AddObjective spread over the level's
	// longest path: a mission that names its checkpoints means those.
	if (!_checkpointsFromCollectibles)
	{
		_checkpoints.clear();
		_checkpointsFromCollectibles = true;
	}
	_checkpoints.push_back(pos);
	Log::Info("ScriptEngine: race checkpoint {} '{}' at ({:.1f}, {:.1f}, {:.1f})", _checkpoints.size(), locator,
	          pos.X, pos.Y, pos.Z);
}

void ScriptEngine::PutPlayerInCar()
{
	if (_playerCar == nullptr)
	{
		Log::Warn("ScriptEngine: PutMFPlayerInCar with no player car placed");
		return;
	}
	_game.PutPlayerInVehicle(_playerCar);
}

Vehicle& ScriptEngine::spawnVehicle(const std::string& car, const std::string& locator, const Vector3& pos)
{
	auto vehicle = std::make_unique<Vehicle>(car);
	std::string modelPath = "art/cars/" + car + ".p3d";
	if (!FileSystem::exists(modelPath))
		modelPath = "art/cars/famil_v.p3d";
	vehicle->LoadModel(modelPath);
	applyCarScript(*vehicle);
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);

	// Every car used to spawn facing +Z whatever its locator said, so sr2's grid
	// sat across the street it had to start down.
	float heading = 0.0f;
	if (_game.GetLevel().GetLocatorHeading(locator, heading))
		vehicle->Teleport(pos, Quaternion(Vector3(0.0f, 1.0f, 0.0f), heading));

	_activeVehicle = vehicle.get();
	_missionVehicles.push_back(std::move(vehicle));
	return *_missionVehicles.back();
}

void ScriptEngine::InitLevelPlayerVehicle(const std::string& car, const std::string& locator,
                                          const std::string& role, const std::string& aiScript)
{
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: locator '{}' not found, spawning vehicle near player", locator);
		pos = _game.GetPlayerPosition() + Vector3(0, 0, 5.0f);
	}

	Log::Info("ScriptEngine: init player vehicle '{}' at ({:.1f}, {:.1f}, {:.1f}) role='{}' ai='{}'",
	          car, pos.X, pos.Y, pos.Z, role, aiScript);

	spawnVehicle(car, locator, pos);
}

void ScriptEngine::AddStageVehicle(const std::string& car, const std::string& locator,
                                   const std::string& behaviour, const std::string& aiScript,
                                   const std::string& driver)
{
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	_raceCarNeedsPlacement = false;
	if (pos == Vector3::Zero)
	{
		// A race car dumped beside the player starts wedged against whatever the
		// player is standing next to, and full steering lock cannot free it. Put
		// it on its own circuit instead; anything else starts the race broken.
		if (behaviour == "race" && !_racePath.empty())
		{
			pos = _racePath.front();
			// This is still only the AI waypoint path, not the routed circuit, and
			// its first point turned out to sit hard against a fence: snake_v spawned
			// with its nose in DONUTFENCE (96..128, -569..-535) and spent the whole
			// race pushing into it at 0 km/h. buildRaceCircuit puts it on the road
			// once the route through the road network is known.
			_raceCarNeedsPlacement = true;
			Log::Warn("ScriptEngine: locator '{}' not found, starting race car '{}' on the circuit", locator, car);
		}
		else
		{
			Log::Warn("ScriptEngine: locator '{}' not found, spawning '{}' near player", locator, car);
			pos = _game.GetPlayerPosition() + Vector3(5.0f, 0, 0);
		}
	}

	Log::Info("ScriptEngine: add stage vehicle '{}' at '{}' ({:.1f}, {:.1f}, {:.1f}) behaviour='{}' ai='{}' driver='{}'",
	          car, locator, pos.X, pos.Y, pos.Z, behaviour, aiScript, driver);

	Vehicle& vehicle = spawnVehicle(car, locator, pos);

	// A "race" stage vehicle is an opponent's. Its driver comes with the circuit,
	// in buildRaceCircuit.
	//
	// "evade" and "target" race too, when the stage is a race: sr3 enters
	// honor_v as "evade" and the bus as "target" alongside a "race" minivan,
	// all three on the same catch-up curves. buildRaceCircuit only gives them
	// drivers for a race objective.
	if (behaviour == "race" || behaviour == "evade" || behaviour == "target")
		_raceVehicles.push_back(&vehicle);
}

// Every car has a tuning script of its own -- scripts/cars/<car>.con, copied off
// the CD with the rest of the scripts -- that sets its mass, engine and top speed
// with the same commands a mission uses on its "current" vehicle. Run it with the
// new car as that vehicle, before its physics body exists, so the body is built
// with the car's own mass. Without it every car in the game was the same car.
void ScriptEngine::applyCarScript(Vehicle& vehicle)
{
	const std::string path = "scripts/cars/" + vehicle.GetName() + ".con";
	if (!FileSystem::exists(path))
	{
		Log::Warn("ScriptEngine: no tuning script for '{}' ({}), it drives on defaults", vehicle.GetName(), path);
		return;
	}

	Vehicle* const previous = _activeVehicle;
	_activeVehicle = &vehicle;
	RunFile(path);
	_activeVehicle = previous;
}

void ScriptEngine::SetMass(float mass) { if (_activeVehicle) _activeVehicle->SetMass(mass); }
void ScriptEngine::SetGasScale(float scale) { if (_activeVehicle) _activeVehicle->SetGasScale(scale); }
void ScriptEngine::SetTopSpeedKmh(float speed) { if (_activeVehicle) _activeVehicle->SetTopSpeedKmh(speed); }
void ScriptEngine::SetTireGrip(float grip) { if (_activeVehicle) _activeVehicle->SetTireGrip(grip); }
void ScriptEngine::SetSuspensionLimit(float limit) { if (_activeVehicle) _activeVehicle->SetSuspensionLimit(limit); }

void ScriptEngine::AddStageWaypoint(const std::string& name)
{
	// These name Locator2 entries in the mission's own P3D (LEVEL-MISSIONP3D),
	// not Path chunks: isolated nodes the route has to be threaded through.
	const Vector3 pos = _game.GetLevel().GetLocatorPosition(name);
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: stage waypoint '{}' not found", name);
		return;
	}

	Log::Info("ScriptEngine: stage waypoint '{}' at ({:.1f}, {:.1f}, {:.1f})", name, pos.X, pos.Y, pos.Z);
	_stageWaypoints.push_back(pos);
}

void ScriptEngine::buildRaceCircuit()
{
	// M1race.con declares a single AddStageWaypoint, so a stage waypoint is a
	// destination rather than one vertex of a polyline. Compose the route from
	// what the mission actually gives us: where the opponent starts, the
	// waypoints it must reach, and the finish line if the mission names one.
	std::vector<Vector3> nodes;
	// Only when the car is where the mission put it. If its start locator was
	// missing it is sitting on a fallback guess, and anchoring the circuit there
	// bakes that guess in as a corner of the route -- it left a 97m leg running
	// back into the fence the fallback had parked against.
	//
	// And only when the mission gives too few waypoints to make a loop of their
	// own. sr2 names eight; adding the grid to them put a detour through the
	// parked opponents into every lap.
	//
	// A race run start to finish (sr3) starts from the grid whatever.
	if (!_raceVehicles.empty() && !_raceCarNeedsPlacement && (_stageWaypoints.size() < 2 || raceIsOpen()))
		nodes.push_back(_raceVehicles.front()->GetPosition());
	nodes.insert(nodes.end(), _stageWaypoints.begin(), _stageWaypoints.end());

	// The finish line, likewise, only completes a route the waypoints cannot
	// close on their own. On sr2 it sits just past the last waypoint, on the
	// far carriageway, and threading it in turned the end of every lap into two
	// hairpins.
	const Vector3 finish = _game.GetLevel().GetLocatorPosition("race_finish");
	if (finish != Vector3::Zero && _stageWaypoints.size() < 2)
		nodes.push_back(finish);

	// Locators are not all on the ground: sr2_waypoint3 hangs 2.5m above the
	// road. Each node becomes a point of the circuit as it is, so stand them on
	// the surface the way PathGraph stands its road nodes.
	for (Vector3& node : nodes)
	{
		float groundY = 0.0f;
		if (_game.GetWorldPhysics().FindGroundHeight(node, 3.0f, 3.0f, groundY, true))
			node.Y = groundY;
	}

	if (nodes.size() < 2)
		return;

	// Thread them through the level's road network: on their own they are points
	// hundreds of metres apart, and driving straight at them goes through
	// buildings.
	const auto& graph = _game.GetPathGraph();
	std::vector<Vector3> circuit;

	// Each race node is a locator, not a graph node: it is routed from whatever
	// road node happens to be nearest. Report that anchor -- its index, how far
	// off it sits and which component it lands in -- because a leg that collapses
	// does so either because the two anchors are in different components or
	// because they are the same node.
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const int anchor = graph.FindNearestNode(nodes[i]);
		const float offset = anchor < 0 ? -1.0f : (graph.GetNodes()[anchor].position - nodes[i]).Length();
		Log::Info("ScriptEngine: race node {} at ({:.1f}, {:.1f}, {:.1f}) -> graph node {} ({:.1f}m away, component {})",
		          i, nodes[i].X, nodes[i].Y, nodes[i].Z, anchor, offset, graph.GetComponent(anchor));
	}

	// Round the loop, or for a race run start to finish, up to the last node
	// and no further: sr3's route came out 4966m with a 765m leg back from the
	// finish to the start that nobody drives.
	const std::size_t legs = raceIsOpen() ? nodes.size() - 1 : nodes.size();
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const Vector3& from = nodes[i];
		const Vector3& to = nodes[(i + 1) % nodes.size()];

		circuit.push_back(from);
		if (i >= legs)
			break;

		const auto leg = graph.FindRoute(from, to);
		Log::Info("ScriptEngine: leg {}->{} spans {:.1f}m as the crow flies, routed through {} nodes", i,
		          (i + 1) % nodes.size(), (to - from).Length(), leg.size());
		if (leg.empty())
		{
			Log::Warn("ScriptEngine: no road route between race nodes {} and {}", i, (i + 1) % nodes.size());
			continue;
		}

		// skip the leg's own endpoints, they duplicate the waypoints
		for (size_t k = 1; k + 1 < leg.size(); ++k)
			circuit.push_back(leg[k]);
	}

	if (circuit.size() < 2)
		return;

	_racePath = std::move(circuit);
	Log::Info("ScriptEngine: race circuit routed through {} points from {} race nodes", _racePath.size(),
	          nodes.size());

	// The gaps matter more than the count: the opponent drives straight at the
	// next point, so a long hop is a straight line through whatever stands
	// between them.
	float total = 0.0f, longest = 0.0f;
	std::size_t longestAt = 0;
	const std::size_t gaps = raceIsOpen() ? _racePath.size() - 1 : _racePath.size();
	for (std::size_t i = 0; i < gaps; ++i)
	{
		const float gap = (_racePath[(i + 1) % _racePath.size()] - _racePath[i]).Length();
		total += gap;
		if (gap > longest)
		{
			longest = gap;
			longestAt = i;
		}
	}
	Log::Info("ScriptEngine: {} is {:.0f}m, mean gap {:.0f}m, longest {:.0f}m at point {}",
	          raceIsOpen() ? "route" : "circuit", total, total / static_cast<float>(gaps), longest, longestAt);

	validateCircuit();

	// Place first: SetCircuit picks the opponent's starting waypoint from where the
	// car actually is, and it is about to move.
	placeRaceCarOnCircuit();

	if (_objectiveType != "race")
		return;
	for (Vehicle* vehicle : _raceVehicles)
	{
		auto opponent = std::make_unique<RaceOpponent>(*vehicle, _racePath, raceIsOpen());
		const auto catchup = _raceCatchups.find(vehicle->GetName());
		if (catchup != _raceCatchups.end())
			opponent->SetCatchup(catchup->second);
		_raceOpponents.push_back(std::move(opponent));
	}
}

// A routed circuit is a list of road-graph positions, and nothing so far asked
// whether a car can get from one to the next. Three ways it cannot, each needing
// its own probe:
//
//   hole     nothing under the point at all -- the opponent drives off the world
//   buried   there is a surface, but *above* the point: the route runs under the
//            road deck it is meant to sit on and the car noses into the
//            underside. A downward ray alone misses this, because it reports a
//            hit either way -- the sign of the gap is the tell.
//   wall     both points are fine and the straight line between them is not.
//
// Reporting them here, once, turns "the AI gets stuck somewhere" into a list of
// coordinates, which is the diagnosis the route fix needs (AI-PATH).
void ScriptEngine::validateCircuit() const
{
	if (_racePath.size() < 2)
		return;

	const WorldPhysics& physics = _game.GetWorldPhysics();

	// A point this far below the surface is under it, not on it. Road tiles are not
	// perfectly flat and the node sits at the tile centre, so a few centimetres of
	// slack is normal; half a metre is not.
	constexpr float buriedBy = 0.5f;
	// Room between a road and a deck over it for the deck to be a bridge: more
	// than a car's height.
	constexpr float kDeckClearance = 3.0f;
	// Above this, the point hangs in the air rather than resting on a road.
	constexpr float floatingBy = 2.0f;
	// Where a bumper sweeps: clear of the road surface and a kerb, low enough that
	// a car meets whatever it hits.
	constexpr float bumperHeight = 0.5f;
	// A hit whose normal is this flat is a wall rather than the road's own slope.
	// A horizontal ray always meets the road on a gradient, so the normal is what
	// separates "steep hill" from "side of a building".
	constexpr float wallNormalY = 0.5f;

	int holes = 0, buried = 0, floating = 0, walls = 0, gaps = 0;

	for (std::size_t i = 0; i < _racePath.size(); ++i)
	{
		const Vector3& here = _racePath[i];

		float groundY = 0.0f;
		if (!physics.FindGroundHeight(here, 50.0f, 50.0f, groundY, true))
		{
			++holes;
			Log::Warn("ScriptEngine: circuit point {} ({:.1f}, {:.1f}, {:.1f}) has no ground under it", i, here.X,
			          here.Y, here.Z);
			continue;
		}

		const float gap = groundY - here.Y;
		// A surface far overhead is a bridge or an overpass, not a buried point
		// -- sr3 runs under several, 12 to 25m up. It is only a defect when there
		// is no road under the point itself.
		float underY = 0.0f;
		const bool roadUnder =
		    physics.FindGroundHeight(here, buriedBy, buriedBy, underY, true) && groundY - underY > kDeckClearance;
		if (gap > buriedBy && !roadUnder)
		{
			++buried;
			Log::Warn("ScriptEngine: circuit point {} ({:.1f}, {:.1f}, {:.1f}) sits {:.1f}m under the surface at "
			          "{:.1f} -- the route runs below the deck here",
			          i, here.X, here.Y, here.Z, gap, groundY);
		}
		else if (gap < -floatingBy)
		{
			++floating;
			Log::Warn("ScriptEngine: circuit point {} ({:.1f}, {:.1f}, {:.1f}) floats {:.1f}m above the surface at "
			          "{:.1f}",
			          i, here.X, here.Y, here.Z, -gap, groundY);
		}
	}

	// Between consecutive points, at bumper height -- and along the way, because
	// the points average 13m apart and a car drives the gap, not the ends. The
	// first run of this check reported no holes at all while the opponent still
	// fell to y=-22: the floor it fell through was between two sampled points,
	// never under one.
	const std::size_t legs = raceIsOpen() ? _racePath.size() - 1 : _racePath.size();
	for (std::size_t i = 0; i < legs; ++i)
	{
		const Vector3& here = _racePath[i];
		const Vector3& next = _racePath[(i + 1) % _racePath.size()];

		Vector3 along = next - here;
		const float span = along.Length();
		if (span < 0.01f)
			continue;
		along = along / span;

		// Shorter than a car, so nothing car-sized hides between two samples.
		constexpr float strideMetres = 2.0f;
		const int strides = static_cast<int>(span / strideMetres);
		for (int step = 1; step < strides; ++step)
		{
			const Vector3 at = here + along * (static_cast<float>(step) * strideMetres);
			float midY = 0.0f;
			if (physics.FindGroundHeight(at, 50.0f, 50.0f, midY, true))
				continue;
			++gaps;
			Log::Warn("ScriptEngine: circuit leg {}->{} crosses a hole at ({:.1f}, {:.1f}, {:.1f}), {:.0f}m along", i,
			          (i + 1) % _racePath.size(), at.X, at.Y, at.Z, static_cast<float>(step) * strideMetres);
			break; // one report per leg is enough to locate it
		}

		Vector3 hit, normal;
		const Vector3 from(here.X, here.Y + bumperHeight, here.Z);
		if (!physics.CastRay(from, along, span, hit, normal, true))
			continue;
		if (std::abs(normal.Y) > wallNormalY)
			continue;

		++walls;
		Log::Warn("ScriptEngine: circuit leg {}->{} is blocked at ({:.1f}, {:.1f}, {:.1f}), {:.1f}m along, normal "
		          "({:.2f}, {:.2f}, {:.2f})",
		          i, (i + 1) % _racePath.size(), hit.X, hit.Y, hit.Z, (hit - from).Length(), normal.X, normal.Y,
		          normal.Z);
	}

	if (holes || buried || floating || walls || gaps)
		Log::Warn("ScriptEngine: circuit is not drivable end to end -- {} hole(s) at points, {} leg(s) crossing a "
		          "hole, {} under the deck, {} floating, {} blocked leg(s), of {} points",
		          holes, gaps, buried, floating, walls, _racePath.size());
	else
		Log::Info("ScriptEngine: circuit validates -- {} points, each on a surface with a clear leg to the next",
		          _racePath.size());
}

void ScriptEngine::placeRaceCarOnCircuit()
{
	// Only when the mission's own car-start locator was missing: when the level
	// names one, that is where the car belongs.
	if (!_raceCarNeedsPlacement || _raceVehicles.empty() || _racePath.size() < 3)
		return;

	// Every point of the circuit now comes out of the road graph, so start on the
	// first one, facing the second: a car dropped at full steering lock cannot
	// turn, because it has to be moving first.
	const Vector3& here = _racePath[0];
	const Vector3& next = _racePath[1];

	Vector3 forward = next - here;
	forward.Y = 0.0f;
	Quaternion rotation = Quaternion::Identity;
	if (forward.LengthSquared() > 0.01f)
		rotation = Quaternion(Vector3(0.0f, 1.0f, 0.0f), std::atan2(forward.X, forward.Z));

	_raceVehicles.front()->Teleport(here, rotation);
	_raceCarNeedsPlacement = false;
}

void ScriptEngine::UsePedGroup(int group)
{
	Log::Info("ScriptEngine: using ped group {}", group);
}

void ScriptEngine::SetHUDIcon(const std::string& icon)
{
	Log::Info("ScriptEngine: HUD icon = '{}'", icon);
}

void ScriptEngine::AddObjective(const std::string& type)
{
	// sr2's stage 1 is the dialogue after the race. Read in the same pass as the
	// race, it reset the laps to 2 and cleared all seven checkpoints.
	if (!declaringPlayedStage())
	{
		Log::Info("ScriptEngine: stage {} objective '{}' set aside, stage {} is the one played", _currentStage, type,
		          _playedStage);
		return;
	}

	_objectiveType = type;
	_currentCheckpoint = 0;
	_currentLap = 0;
	_totalLaps = kDefaultRaceLaps;
	_raceDecided = false;
	_checkpointsFromCollectibles = false;
	_checkpoints.clear();
	_racePath.clear();
	_aiCheckpoint = 0;

	if (type == "race")
	{
		const auto& paths = _game.GetLevel().GetPaths();
		const auto* bestPath = &paths[0];
		for (const auto& p : paths)
			if (p.points.size() > bestPath->points.size())
				bestPath = &p;

		int numCheckpoints = std::min(6, static_cast<int>(bestPath->points.size()));
		int step = static_cast<int>(bestPath->points.size()) / numCheckpoints;
		if (step < 1) step = 1;

		for (int i = 0; i < numCheckpoints; ++i)
			_checkpoints.push_back(bestPath->points[i * step]);

		// The AI follows the road, not the player's checkpoints: six points spread
		// over the whole circuit describe no drivable line at all, and an opponent
		// aiming straight at the next one drives into buildings. Resample the path
		// itself so consecutive waypoints are never more than kRaceWaypointSpacing
		// apart.
		constexpr float kRaceWaypointSpacing = 12.0f;
		_racePath.clear();
		const auto& pts = bestPath->points;
		for (size_t i = 0; i < pts.size(); ++i)
		{
			const Vector3& from = pts[i];
			const Vector3& to = pts[(i + 1) % pts.size()];
			_racePath.push_back(from);

			const float span = (to - from).Length();
			const int extra = static_cast<int>(span / kRaceWaypointSpacing);
			for (int k = 1; k < extra; ++k)
				_racePath.push_back(from + (to - from) * (static_cast<float>(k) / extra));
		}

		if (!_checkpoints.empty())
		{
			_aiPosition = _checkpoints[0] + Vector3(10.0f, 0, 0);
			_aiRotation = Quaternion::Identity;
		}

		Log::Info("ScriptEngine: race circuit with {} checkpoints and {} AI waypoints on a path of {} points",
		          _checkpoints.size(), _racePath.size(), bestPath->points.size());
	}
	else
	{
		Log::Info("ScriptEngine: objective type = '{}'", type);
	}
}

void ScriptEngine::OnTrigger(const Level::Trigger& trigger, bool entered)
{
	// Nothing acts on these yet beyond reporting them: the dynamic-zone loaders
	// would drive region streaming (LEVEL-STREAM) and the interior entrances
	// GAME-014. Name the locator and what it carries, so the log says which.
	Log::Debug("ScriptEngine: player {} '{}' (locator '{}', type {}{}{})", entered ? "entered" : "left", trigger.name,
	           trigger.locator, static_cast<uint32_t>(trigger.type), trigger.data.empty() ? "" : ", data '",
	           trigger.data.empty() ? "" : trigger.data + "'");
}

void ScriptEngine::NoTrafficForStage()
{
	if (!declaringPlayedStage())
		return;
	_noTraffic = true;
	Log::Info("ScriptEngine: no traffic for this stage");
}

void ScriptEngine::SetRaceLaps(int laps)
{
	if (!declaringPlayedStage())
		return;
	if (laps < 1)
	{
		Log::Warn("ScriptEngine: SetRaceLaps({}) ignored, keeping {}", laps, _totalLaps);
		return;
	}
	_totalLaps = laps;
	Log::Info("ScriptEngine: race is {} lap(s)", laps);
}

void ScriptEngine::SetStageAIRaceCatchupParams(const std::string& vehicle, float distance, float ahead, float level,
                                               float behind)
{
	// Kept by name: the driver it is for does not exist until the circuit does.
	_raceCatchups[vehicle] = {distance, ahead, level, behind};
}

const std::vector<ScriptEngine::CountdownStep>& ScriptEngine::countdownSteps() const
{
	// What every mission had before scripts could declare their own: "GO!" for
	// 1.5s, holding nobody.
	static const std::vector<CountdownStep> kDefault {{"GO!", 1.5f}};
	return _countdown.empty() ? kDefault : _countdown;
}

float ScriptEngine::countdownLength() const
{
	float total = 0.0f;
	for (const auto& step : countdownSteps())
		total += step.seconds;
	return total;
}

void ScriptEngine::StartCountdown(const std::string& name)
{
	Log::Info("ScriptEngine: countdown '{}'", name);
	_countdown.clear();
	_countdownElapsed = 0.0f;
}

void ScriptEngine::AddToCountdownSequence(const std::string& text, int milliseconds)
{
	_countdown.push_back({text, static_cast<float>(milliseconds) / 1000.0f});
}

std::string ScriptEngine::GetCountdownText() const
{
	float start = 0.0f;
	for (const auto& step : countdownSteps())
	{
		if (_countdownElapsed < start + step.seconds)
			return step.text;
		start += step.seconds;
	}
	return {};
}

bool ScriptEngine::IsRaceHeld() const
{
	// The cars go on "GO" and not after it: sr2 shows it for 400ms, with the
	// race already under way.
	const auto& steps = countdownSteps();
	float start = 0.0f;
	for (std::size_t i = 0; i < steps.size(); ++i)
	{
		std::string upper = steps[i].text;
		for (char& c : upper)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		if (upper.rfind("GO", 0) == 0 || i + 1 == steps.size())
			return _countdownElapsed < start;
		start += steps[i].seconds;
	}
	return false;
}

float ScriptEngine::playerRaceLaps() const
{
	// In laps, so it is comparable with each opponent's own progress around a
	// circuit that has a different number of points than the checkpoint list.
	if (_checkpoints.empty())
		return static_cast<float>(_currentLap);
	return static_cast<float>(_currentLap) +
	       static_cast<float>(_currentCheckpoint) / static_cast<float>(_checkpoints.size());
}

bool ScriptEngine::GetRacePosition(int& position, int& field) const
{
	if (_raceOpponents.empty())
		return false;

	// Checkpoints are coarse -- sr2 has seven a lap -- so the player only gains a
	// place on passing one. Good enough to read at a glance, which is its job.
	const float player = playerRaceLaps();
	position = 1;
	for (const auto& opponent : _raceOpponents)
		if (opponent->ProgressLaps() > player)
			++position;
	field = static_cast<int>(_raceOpponents.size()) + 1;
	return true;
}

void ScriptEngine::UpdateAI(double dt)
{
	if (_raceOpponents.empty())
		return;

	if (IsRaceHeld())
	{
		for (auto& opponent : _raceOpponents)
			opponent->Hold();
		return;
	}

	const float playerLaps = playerRaceLaps();

	for (auto& opponent : _raceOpponents)
	{
		// If the player has taken this opponent's car, stop driving it: two
		// inputs on one vehicle in the same frame just fight each other.
		if (_game.GetPlayerVehicle() == &opponent->GetVehicle())
			continue;

		opponent->Update(dt, playerLaps);

		// A race is lost when an opponent finishes it first. Without this the
		// only way to lose was the stage timer, and a player who was simply
		// beaten kept racing an opponent that had long since gone round again.
		if (!_raceDecided && opponent->Laps() >= _totalLaps)
		{
			_raceDecided = true;
			Log::Info("ScriptEngine: '{}' finished all {} laps first - mission failed!",
			          opponent->GetVehicle().GetName(), _totalLaps);
			_stageTimeRemaining = -1.0f;
			_game.AddShake(2.0f);
			_game.GetAudioManager().PlayRaw(SoundGenerator::Beep(150, 0.5f), 22050, 1, 16);
			_game.SetState(GameState::MissionFailed);
		}
	}

	// Keep the legacy readouts pointing at a real car so anything still asking
	// for the AI transform (debug draw, HUD) follows the leading opponent's.
	_aiPosition = _raceOpponents.front()->GetVehicle().GetPosition();
	_aiRotation = _raceOpponents.front()->GetVehicle().GetRotation();
}

void ScriptEngine::ResetBestTime()
{
	_bestTime = 999.0f;
	std::remove("donut_save.dat");
	Log::Info("ScriptEngine: best time reset to default");
}

void ScriptEngine::SaveGameState()
{
	auto& game = Game::GetInstance();
	std::ofstream saveFile("donut_save.dat");
	if (!saveFile.good()) return;

	saveFile << _bestTime << "\n";
	Vector3 pos = game.GetPlayerPosition();
	saveFile << pos.X << " " << pos.Y << " " << pos.Z << "\n";
	Log::Info("ScriptEngine: game saved");
}

void ScriptEngine::LoadGameState()
{
	std::ifstream saveFile("donut_save.dat");
	if (!saveFile.good()) return;

	saveFile >> _bestTime;
	Log::Info("ScriptEngine: loaded best time {:.1f}s", _bestTime);
}

void ScriptEngine::AdvanceCheckpoint()
{
	if (_currentCheckpoint < static_cast<int>(_checkpoints.size()))
	{
		_currentCheckpoint++;
		Log::Info("ScriptEngine: checkpoint {}/{} (lap {}/{}) reached!",
		          _currentCheckpoint, _checkpoints.size(), _currentLap + 1, _totalLaps);

		_game.AddShake(0.3f);
		AudioManager& audio = _game.GetAudioManager();
		if (_currentCheckpoint >= static_cast<int>(_checkpoints.size()))
		{
			_currentLap++;
			if (_currentLap >= _totalLaps)
			{
				audio.PlayRaw(SoundGenerator::Chirp(400, 1200, 0.3f), 22050, 1, 16);
				ShowStageComplete();
				return;
			}
			else
			{
				_currentCheckpoint = 0;
				audio.PlayRaw(SoundGenerator::Beep(800, 0.15f), 22050, 1, 16);
			}
		}
		else
		{
			audio.PlayRaw(SoundGenerator::Beep(600, 0.1f), 22050, 1, 16);
		}
	}
}

void ScriptEngine::CloseObjective()
{
	// Ends the objective's declaration, as CloseStage and CloseMission end
	// theirs; it is not the objective being met. Clearing the type here left
	// every game race script without an objective (and the HUD without its
	// "Objective: race") the moment the block was read.
	Log::Info("ScriptEngine: objective '{}' declared", _objectiveType);
}

void ScriptEngine::SetObjTargetVehicle(const std::string& target)
{
	_objectiveTarget = target;
	Log::Info("ScriptEngine: objective target vehicle = '{}'", target);
}

void ScriptEngine::ShowStageComplete()
{
	_raceDecided = true;
	float elapsed = _initialStageTime - _stageTimeRemaining;
	_isNewRecord = false;
	if (elapsed < _bestTime)
	{
		_bestTime = elapsed;
		_isNewRecord = true;
		std::ofstream saveFile("donut_save.dat");
		if (saveFile.good())
		{
			saveFile << _bestTime << "\n";
			Vector3 pos = _game.GetPlayerPosition();
			saveFile << pos.X << " " << pos.Y << " " << pos.Z << "\n";
		}
		_game.GetAudioManager().PlayRaw(SoundGenerator::Chirp(600, 1200, 0.5f), 22050, 1, 16);
	}

	Log::Info("ScriptEngine: stage complete! Time: {:.1f}s (Best: {:.1f}s){}",
	          elapsed, _bestTime, _isNewRecord ? " NEW RECORD!" : "");
	_stageTimeRemaining = -1.0f;
	_game.SetState(GameState::MissionComplete);
}

void ScriptEngine::SetPresentationBitmap(const std::string& path)
{
	Log::Info("ScriptEngine: presentation bitmap = '{}'", path);
}

void ScriptEngine::EnableTutorialMode(bool enable)
{
	Log::Info("ScriptEngine: tutorial mode = {}", enable);
}

void ScriptEngine::CreateChaseManager(const std::string& copCar, const std::string& aiScript, int count)
{
	_chaseManager = std::make_unique<ChaseManager>(_game.GetWorldPhysics(), copCar, count);
	Log::Info("ScriptEngine: chase manager created '{}' ai='{}' count={}", copCar, aiScript, count);
}

void ScriptEngine::RunFile(const std::string& path)
{
	Log::Info("ScriptEngine: running {}", path);
	Commands::RunScript(path);
}

} // namespace Donut
