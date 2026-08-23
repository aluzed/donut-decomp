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
	_zones.clear();

	// Any vehicle still here was never torn down (CleanupMission is what does
	// that). Dropping the unique_ptrs would leave its rigid body and action
	// behind in the physics world, dangling: the first run left a car falling
	// forever, reaching Y = -440.
	_raceOpponent.reset();
	_stageWaypoints.clear();
	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_goTimer = 1.5f;
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
	_raceOpponent.reset();
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
	_goTimer = 1.5f;

	// Tearing the mission down is what clears the active flag; CloseMission used
	// to do it as a side effect of wrongly completing the mission, and without
	// this SelectMission's `if (_missionActive) return` would refuse to reload.
	_missionActive = false;
	_currentStage = -1;

	Log::Info("ScriptEngine: mission cleaned up");
}

void ScriptEngine::SetStageTime(float seconds)
{
	_stageTimeRemaining = seconds;
	_initialStageTime = seconds;
	Log::Info("ScriptEngine: stage time = {}s", seconds);
}

void ScriptEngine::Update(double dt)
{
	if (_goTimer > 0.0f)
	{
		_goTimer -= static_cast<float>(dt);
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
	Log::Info("ScriptEngine: place player car '{}' at '{}'", car, locator);
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

	auto vehicle = std::make_unique<Vehicle>(car);
	std::string modelPath = "art/cars/" + car + ".p3d";
	if (!FileSystem::exists(modelPath))
		modelPath = "art/cars/famil_v.p3d";
	vehicle->LoadModel(modelPath);
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);
	_activeVehicle = vehicle.get();

	_missionVehicles.push_back(std::move(vehicle));
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

	auto vehicle = std::make_unique<Vehicle>(car);
	std::string modelPath = "art/cars/" + car + ".p3d";
	if (!FileSystem::exists(modelPath))
		modelPath = "art/cars/famil_v.p3d";
	vehicle->LoadModel(modelPath);
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);
	_activeVehicle = vehicle.get();

	// A "race" stage vehicle is the opponent's; give it a driver following the
	// same circuit the player's checkpoints come from.
	if (behaviour == "race" && !_racePath.empty())
		_raceOpponent = std::make_unique<RaceOpponent>(*vehicle, _racePath);

	_missionVehicles.push_back(std::move(vehicle));
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
	if (_raceOpponent && !_raceCarNeedsPlacement)
		nodes.push_back(_raceOpponent->GetVehicle().GetPosition());
	nodes.insert(nodes.end(), _stageWaypoints.begin(), _stageWaypoints.end());

	const Vector3 finish = _game.GetLevel().GetLocatorPosition("race_finish");
	if (finish != Vector3::Zero)
		nodes.push_back(finish);

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

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const Vector3& from = nodes[i];
		const Vector3& to = nodes[(i + 1) % nodes.size()];

		circuit.push_back(from);

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
	for (std::size_t i = 0; i < _racePath.size(); ++i)
	{
		const float gap = (_racePath[(i + 1) % _racePath.size()] - _racePath[i]).Length();
		total += gap;
		if (gap > longest)
		{
			longest = gap;
			longestAt = i;
		}
	}
	Log::Info("ScriptEngine: circuit is {:.0f}m round, mean gap {:.0f}m, longest {:.0f}m at point {}", total,
	          total / static_cast<float>(_racePath.size()), longest, longestAt);

	// A route is only drivable where there is collision geometry under it. Sample
	// the whole circuit once so a hole shows up here rather than as an opponent
	// falling out of the world.
	int holes = 0;
	for (std::size_t i = 0; i < _racePath.size(); ++i)
	{
		float groundY = 0.0f;
		if (_game.GetWorldPhysics().FindGroundHeight(_racePath[i], 50.0f, 50.0f, groundY))
			continue;

		++holes;
		Log::Warn("ScriptEngine: no ground under circuit point {} ({:.1f}, {:.1f}, {:.1f})", i, _racePath[i].X,
		          _racePath[i].Y, _racePath[i].Z);
	}
	if (holes > 0)
		Log::Warn("ScriptEngine: {} of {} circuit points have no collision under them", holes, _racePath.size());

	// Place first: SetCircuit picks the opponent's starting waypoint from where the
	// car actually is, and it is about to move.
	placeRaceCarOnCircuit();

	if (_raceOpponent)
		_raceOpponent->SetCircuit(_racePath);
}

void ScriptEngine::placeRaceCarOnCircuit()
{
	// Only when the mission's own car-start locator was missing: when the level
	// names one, that is where the car belongs.
	if (!_raceCarNeedsPlacement || !_activeVehicle || _racePath.size() < 3)
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

	_activeVehicle->Teleport(here, rotation);
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
	_objectiveType = type;
	_currentCheckpoint = 0;
	_currentLap = 0;
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

void ScriptEngine::UpdateAI(double dt)
{
	if (!_raceOpponent)
		return;

	// If the player has taken the opponent's car, stop driving it: two inputs on
	// one vehicle in the same frame just fight each other.
	if (_game.GetPlayerVehicle() == &_raceOpponent->GetVehicle())
		return;

	// In laps, so it is comparable with the opponent's own progress around a
	// circuit that has a different number of points than the checkpoint list.
	const float playerLaps =
	    _checkpoints.empty()
	        ? static_cast<float>(_currentLap)
	        : static_cast<float>(_currentLap) + static_cast<float>(_currentCheckpoint) / static_cast<float>(_checkpoints.size());
	_raceOpponent->Update(dt, playerLaps);

	// Keep the legacy readouts pointing at the real car so anything still asking
	// for the AI transform (debug draw, HUD) follows the opponent.
	_aiPosition = _raceOpponent->GetVehicle().GetPosition();
	_aiRotation = _raceOpponent->GetVehicle().GetRotation();
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
	Log::Info("ScriptEngine: objective '{}' completed", _objectiveType);
	_objectiveType.clear();
}

void ScriptEngine::SetObjTargetVehicle(const std::string& target)
{
	_objectiveTarget = target;
	Log::Info("ScriptEngine: objective target vehicle = '{}'", target);
}

void ScriptEngine::ShowStageComplete()
{
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
