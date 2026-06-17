// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <string>
#include <vector>

#include "Vehicle.h"

namespace Donut
{

class Game;
class Level;
class Vehicle;

class ScriptEngine
{
public:
	ScriptEngine(Game& game): _game(game) {}

	void SelectMission(const std::string& id);
	void SetMissionResetPlayerInCar(const std::string& locator);
	void SetMissionResetPlayerOutCar(const std::string& locator);
	void SetDynaLoadData(const std::string& zones);

	void AddStage(int index);
	void CloseStage();
	void CloseMission();
	void SetStageTime(float seconds);
	void Update(double dt);

	void AddCharacter(const std::string& name, const std::string& locator);
	void AddNPC(const std::string& name, const std::string& locator, const std::string& type = "");
	void PlacePlayerCar(const std::string& car, const std::string& locator);
	void InitLevelPlayerVehicle(const std::string& car, const std::string& locator,
	                            const std::string& role, const std::string& aiScript = "");
	void AddStageVehicle(const std::string& car, const std::string& locator,
	                     const std::string& behaviour, const std::string& aiScript,
	                     const std::string& driver = "");

	void AddStageWaypoint(const std::string& path);
	void UsePedGroup(int group);

	void SetMass(float mass);
	void SetGasScale(float scale);
	void SetTopSpeedKmh(float speed);
	void SetTireGrip(float grip);
	void SetSuspensionLimit(float limit);

	void SetHUDIcon(const std::string& icon);
	void AddObjective(const std::string& type);
	void ShowStageComplete();
	void SetPresentationBitmap(const std::string& path);
	void EnableTutorialMode(bool enable);
	void CreateChaseManager(const std::string& copCar, const std::string& aiScript, int count);

	void CloseObjective();
	void SetObjTargetVehicle(const std::string& target);

	void RunFile(const std::string& path);

	bool IsMissionActive() const { return _missionActive; }
	float GetStageTimeRemaining() const { return _stageTimeRemaining; }
	std::vector<std::unique_ptr<Vehicle>>& GetMissionVehicles() { return _missionVehicles; }

private:
	Game& _game;
	bool _missionActive = false;
	std::string _missionId;
	std::string _playerLocator;
	std::vector<std::string> _zones;
	int _currentStage = -1;
	float _stageTimeRemaining = -1.0f;
	std::vector<std::unique_ptr<Vehicle>> _missionVehicles;
	Vehicle* _activeVehicle = nullptr;

	std::string _objectiveType;
	std::string _objectiveTarget;
};

} // namespace Donut
