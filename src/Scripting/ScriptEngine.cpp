// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "ScriptEngine.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "Level.h"
#include "Scripting/Commands.h"
#include "Vehicle.h"

namespace Donut
{

void ScriptEngine::SelectMission(const std::string& id)
{
	_missionActive = true;
	_missionId = id;
	_currentStage = -1;
	_zones.clear();
	_missionVehicles.clear();
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
		_game.LoadModel("homer", "homer");
		Log::Info("ScriptEngine: player placed at locator '{}' ({:.1f}, {:.1f}, {:.1f})", locator, pos.X, pos.Y, pos.Z);
	}
}

void ScriptEngine::SetMissionResetPlayerOutCar(const std::string& locator)
{
	_playerLocator = locator;
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

void ScriptEngine::SetStageTime(float seconds)
{
	Log::Info("ScriptEngine: stage time = {}s", seconds);
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
	Log::Info("ScriptEngine: init player vehicle '{}' at '{}' ({:.1f}, {:.1f}, {:.1f}) role='{}' ai='{}'",
	          car, locator, pos.X, pos.Y, pos.Z, role, aiScript);

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
	Log::Info("ScriptEngine: objective type = '{}'", type);
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
	Log::Info("ScriptEngine: stage complete!");
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
