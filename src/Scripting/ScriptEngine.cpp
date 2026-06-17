// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "ScriptEngine.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "Level.h"
#include "Scripting/Commands.h"
#include "Vehicle.h"

#include <fstream>

namespace Donut
{

void ScriptEngine::SelectMission(const std::string& id)
{
	_missionActive = true;
	_missionId = id;
	_currentStage = -1;
	_zones.clear();
	_missionVehicles.clear();
	_goTimer = 1.5f;
	_stageTimeRemaining = -1.0f;

	std::ifstream saveFile("donut_save.dat");
	if (saveFile.good())
	{
		saveFile >> _bestTime;
		Log::Info("ScriptEngine: loaded best time {:.1f}s", _bestTime);
	}

	Log::Info("ScriptEngine: mission {} started", id);

	std::string scriptPath = "scripts/Missions/level01/" + id + ".con";
	if (FileSystem::exists(scriptPath))
		RunFile(scriptPath);
	else
	{
		scriptPath = "scripts/" + id + ".con";
		if (FileSystem::exists(scriptPath))
			RunFile(scriptPath);
	}
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
	Log::Info("ScriptEngine: stage {} end", _currentStage);
}

void ScriptEngine::CloseMission()
{
	_missionActive = false;
	_currentStage = -1;
	Log::Info("ScriptEngine: mission {} completed", _missionId);
	_game.SetState(GameState::MissionComplete);
}

void ScriptEngine::CleanupMission()
{
	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_activeVehicle = nullptr;
	_objectiveType.clear();
	_objectiveTarget.clear();
	_stageTimeRemaining = -1.0f;
	_goTimer = 1.5f;
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

	_stageTimeRemaining -= static_cast<float>(dt);
	if (_stageTimeRemaining <= 0.0f)
	{
		_stageTimeRemaining = 0.0f;
		Log::Info("ScriptEngine: stage timer expired - mission failed!");
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
	vehicle->LoadModel("art/cars/" + car + ".p3d");
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);
	_activeVehicle = vehicle.get();
	_missionVehicles.push_back(std::move(vehicle));
}

void ScriptEngine::AddStageVehicle(const std::string& car, const std::string& locator,
                                   const std::string& behaviour, const std::string& aiScript,
                                   const std::string& driver)
{
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: locator '{}' not found, spawning '{}' near player", locator, car);
		pos = _game.GetPlayerPosition() + Vector3(5.0f, 0, 0);
	}

	Log::Info("ScriptEngine: add stage vehicle '{}' at '{}' ({:.1f}, {:.1f}, {:.1f}) behaviour='{}' ai='{}' driver='{}'",
	          car, locator, pos.X, pos.Y, pos.Z, behaviour, aiScript, driver);

	auto vehicle = std::make_unique<Vehicle>(car);
	vehicle->LoadModel("art/cars/" + car + ".p3d");
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);
	_activeVehicle = vehicle.get();
	_missionVehicles.push_back(std::move(vehicle));
}

void ScriptEngine::SetMass(float mass) { if (_activeVehicle) _activeVehicle->SetMass(mass); }
void ScriptEngine::SetGasScale(float scale) { if (_activeVehicle) _activeVehicle->SetGasScale(scale); }
void ScriptEngine::SetTopSpeedKmh(float speed) { if (_activeVehicle) _activeVehicle->SetTopSpeedKmh(speed); }
void ScriptEngine::SetTireGrip(float grip) { if (_activeVehicle) _activeVehicle->SetTireGrip(grip); }
void ScriptEngine::SetSuspensionLimit(float limit) { if (_activeVehicle) _activeVehicle->SetSuspensionLimit(limit); }

void ScriptEngine::AddStageWaypoint(const std::string& path)
{
	Log::Info("ScriptEngine: stage waypoint = '{}'", path);
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
	_checkpoints.clear();

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

		Log::Info("ScriptEngine: race circuit with {} checkpoints on path with {} points",
		          _checkpoints.size(), bestPath->points.size());
	}
	else
	{
		Log::Info("ScriptEngine: objective type = '{}'", type);
	}
}

void ScriptEngine::AdvanceCheckpoint()
{
	if (_currentCheckpoint < static_cast<int>(_checkpoints.size()))
	{
		_currentCheckpoint++;
		Log::Info("ScriptEngine: checkpoint {}/{} reached!", _currentCheckpoint, _checkpoints.size());

		AudioManager& audio = _game.GetAudioManager();
		if (_currentCheckpoint >= static_cast<int>(_checkpoints.size()))
		{
			audio.PlayRaw(SoundGenerator::Chirp(400, 1200, 0.3f), 22050, 1, 16);
			ShowStageComplete();
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
	if (elapsed < _bestTime)
	{
		_bestTime = elapsed;
		std::ofstream saveFile("donut_save.dat");
		if (saveFile.good())
			saveFile << _bestTime;
	}

	Log::Info("ScriptEngine: stage complete! Time: {:.1f}s (Best: {:.1f}s)", elapsed, _bestTime);
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
	Log::Info("ScriptEngine: create chase manager '{}' ai='{}' count={}", copCar, aiScript, count);
}

void ScriptEngine::RunFile(const std::string& path)
{
	Log::Info("ScriptEngine: running {}", path);
	Commands::RunScript(path);
}

} // namespace Donut
