// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Vehicle.h"

#include <string>
#include <vector>

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
	void CleanupMission();

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
	void UpdateAI(double dt);
	void ResetBestTime();
	void SetPresentationBitmap(const std::string& path);
	void EnableTutorialMode(bool enable);
	void CreateChaseManager(const std::string& copCar, const std::string& aiScript, int count);

	void CloseObjective();
	void SetObjTargetVehicle(const std::string& target);

	void RunFile(const std::string& path);

	bool IsMissionActive() const { return _missionActive; }
	float GetStageTimeRemaining() const { return _stageTimeRemaining; }
	float GetInitialStageTime() const { return _initialStageTime; }
	float GetGoTimer() const { return _goTimer; }
	const std::string& GetObjectiveType() const { return _objectiveType; }
	const Vector3& GetAIPosition() const { return _aiPosition; }
	const Quaternion& GetAIRotation() const { return _aiRotation; }
	const std::vector<Vector3>& GetCheckpoints() const { return _checkpoints; }
	int GetCurrentCheckpoint() const { return _currentCheckpoint; }
	int GetCurrentLap() const { return _currentLap; }
	int GetTotalLaps() const { return _totalLaps; }
	float GetBestTime() const { return _bestTime; }
	bool IsNewRecord() const { return _isNewRecord; }
	void AdvanceCheckpoint();
	std::vector<std::unique_ptr<Vehicle>>& GetMissionVehicles() { return _missionVehicles; }

private:
	Game& _game;
	bool _missionActive = false;
	std::string _missionId;
	std::string _playerLocator;
	std::vector<std::string> _zones;
	int _currentStage = -1;
	float _stageTimeRemaining = -1.0f;
	float _initialStageTime = 0.0f;
	float _goTimer = 1.5f;
	std::vector<std::unique_ptr<Vehicle>> _missionVehicles;
	Vehicle* _activeVehicle = nullptr;

	std::string _objectiveType;
	std::string _objectiveTarget;
	std::vector<Vector3> _checkpoints;
	int _currentCheckpoint = 0;
	int _currentLap = 0;
	int _totalLaps = 2;
	float _bestTime = 999.0f;
	bool _isNewRecord = false;
	Vector3 _aiPosition;
	Quaternion _aiRotation;
	int _aiCheckpoint = 0;
	float _aiSpeed = 12.0f;
};

} // namespace Donut
