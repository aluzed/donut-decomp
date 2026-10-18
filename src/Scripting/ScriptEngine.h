// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Level.h"
#include "Vehicle.h"

#include "AI/ChaseManager.h"
#include "AI/RaceOpponent.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Game;
class Level;
class Vehicle;
class ChaseManager;

class ScriptEngine
{
public:
	ScriptEngine(Game& game): _game(game) {}
	~ScriptEngine();

	void SelectMission(const std::string& id);
	void SetMissionResetPlayerInCar(const std::string& locator);
	void SetMissionResetPlayerOutCar(const std::string& locator);
	void SetDynaLoadData(const std::string& zones);

	void AddStage(int index);
	void CloseStage();
	void CloseMission();
	void buildRaceCircuit();
	// Raycasts the routed circuit against the level and reports every point a car
	// could not drive through. Call once, after _racePath is composed.
	void validateCircuit() const;
	void applyCarScript(Vehicle& vehicle);
	// Moves a race car that had no start locator onto the routed circuit.
	void placeRaceCarOnCircuit();
	void SetStageTime(float seconds);
	void Update(double dt);
	void CleanupMission();

	void AddCharacter(const std::string& name, const std::string& locator);
	void AddNPC(const std::string& name, const std::string& locator, const std::string& type = "");
	void PlacePlayerCar(const std::string& car, const std::string& locator);
	// Seats the player in the car PlacePlayerCar placed.
	void PutPlayerInCar();
	// Inside a race objective, each collectible is a checkpoint the player has to
	// drive through, in order (sr2: "carsphere" at sr2_waypoint1..8).
	void AddCollectible(const std::string& locator, const std::string& type);
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
	void UpdateAI(double dt);
	void ResetBestTime();
	void SaveGameState();
	void LoadGameState();
	void SetPresentationBitmap(const std::string& path);
	void EnableTutorialMode(bool enable);
	void CreateChaseManager(const std::string& copCar, const std::string& aiScript, int count);

	void CloseObjective();
	void SetObjTargetVehicle(const std::string& target);

	void RunFile(const std::string& path);

	bool IsMissionActive() const { return _missionActive; }
	float GetStageTimeRemaining() const { return _stageTimeRemaining; }
	float GetInitialStageTime() const { return _initialStageTime; }
	// The race start: StartCountdown opens a sequence, AddToCountdownSequence adds
	// a step ("3" for 800ms, "PSYCHE" for 500ms, "GO" ...). Text of the step on
	// screen now, empty once the sequence has run.
	void StartCountdown(const std::string& name);
	void AddToCountdownSequence(const std::string& text, int milliseconds);
	std::string GetCountdownText() const;
	// True while the cars are held on the line: until the "GO" step, or the last
	// step when there is no "GO".
	bool IsRaceHeld() const;
	// The player's place in the race, 1 for the lead, out of `field` cars. False
	// when there is nobody to race against.
	bool GetRacePosition(int& position, int& field) const;
	// NoTrafficForStage: the played stage runs with the streets clear (every
	// street race asks for it).
	void NoTrafficForStage();
	bool IsTrafficSuppressed() const { return _missionActive && _noTraffic; }
	const std::string& GetObjectiveType() const { return _objectiveType; }
	// The player's start as the mission placed it, if it did.
	bool GetPlayerStart(Vector3& position) const
	{
		position = _playerStart;
		return _hasPlayerStart;
	}
	const Vector3& GetAIPosition() const { return _aiPosition; }
	const Quaternion& GetAIRotation() const { return _aiRotation; }
	ChaseManager* GetChaseManager() const { return _chaseManager.get(); }
	const std::vector<Vector3>& GetCheckpoints() const { return _checkpoints; }
	int GetCurrentCheckpoint() const { return _currentCheckpoint; }
	int GetCurrentLap() const { return _currentLap; }
	int GetTotalLaps() const { return _totalLaps; }
	void SetRaceLaps(int laps);
	// Objective set-up commands.
	void SetDestination(const std::string& locator, const std::string& type);
	void SetTalkToTarget(const std::string& npc);
	void SetDurationTime(float seconds);
	// Brings a car declared earlier into this stage's action ("race", "evade",
	// "target" race against the player).
	void ActivateVehicle(const std::string& name, const std::string& behaviour);
	// The script's ShowStageComplete(): announce the stage when it is done. It
	// does not complete anything itself.
	void AnnounceStageComplete();
	// Where the current objective wants the player: the next checkpoint or
	// destination, or whoever they are to talk to. For --autopilot.
	bool GetObjectiveTarget(Vector3& position) const;
	bool ObjectiveWantsCar() const { return _objectiveType == "getin"; }
	// A trigger volume the player has just entered or left (Level::UpdateTriggers).
	void OnTrigger(const Level::Trigger& trigger, bool entered);
	// SetStageAIRaceCatchupParams(vehicle, distance, ahead, level, behind). The
	// game's own race scripts pass e.g. (80, 0.6, 1.0, 2.0): read as a distance in
	// metres and the speed factor with the AI that far ahead, level, and that far
	// behind -- an interpretation, the data carries no names.
	void SetStageAIRaceCatchupParams(const std::string& vehicle, float distance, float ahead, float level,
	                                 float behind);
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
	struct CountdownStep
	{
		std::string text;
		float seconds;
	};
	// The mission's countdown; empty means the default, a bare "GO!".
	std::vector<CountdownStep> _countdown;
	// Time since the mission started, for walking the countdown.
	float _countdownElapsed = 0.0f;
	// The sequence in force: the mission's own or the default.
	const std::vector<CountdownStep>& countdownSteps() const;
	float countdownLength() const;
	// How far the player is round the race, in laps, from the checkpoints passed.
	float playerRaceLaps() const;
	std::vector<std::unique_ptr<Vehicle>> _missionVehicles;
	Vehicle* _activeVehicle = nullptr;
	// The player's car for this mission, when the mission places one.
	Vehicle* _playerCar = nullptr;
	// Builds a mission car at `pos`, tuned from its script, facing the way the
	// locator does when it says. The caller decides what the car is for.
	Vehicle& spawnVehicle(const std::string& car, const std::string& locator, const Vector3& pos);

	std::string _objectiveType;
	std::string _objectiveTarget;
	std::vector<Vector3> _checkpoints;
	int _currentCheckpoint = 0;
	int _currentLap = 0;
	// Laps when the mission does not say; SetRaceLaps overrides it per objective.
	// One, as for sr3, which names no lap count and runs from the grid to a
	// finish line rather than round a loop.
	static constexpr int kDefaultRaceLaps = 1;
	// A single-lap race is run start to finish, not round a circuit.
	bool raceIsOpen() const { return _totalLaps == 1; }
	int _totalLaps = kDefaultRaceLaps;
	// Set once the race has a result -- the player or an opponent home first --
	// so it is declared once. The stage clock cannot say: sr3 has none.
	bool _raceDecided = false;
	// Whether _checkpoints came from the objective's own collectibles rather than
	// the level path AddObjective falls back on.
	bool _checkpointsFromCollectibles = false;
	bool _noTraffic = false;

	// Each stage's own commands, recorded as the script is read and run when the
	// stage begins: a mission is a sequence of stages, and running every stage's
	// commands at load time made the last one declared overwrite the rest.
	// Commands outside any stage run as they are read.
	std::vector<std::vector<std::string>> _stageScripts;
	bool _recordingStage = false;
	std::size_t _stageCursor = 0;
	void beginStage(std::size_t index);
	// The current stage's objective is met: on to the next stage, or the end.
	void objectiveComplete();
	void missionComplete();
	// Watches the objectives that are not met by driving through checkpoints.
	void updateObjective(double dt);

	// The current objective's particulars.
	bool _hasDestination = false;
	Vector3 _destination;
	std::string _talkToTarget;
	std::map<std::string, Vector3> _npcPositions;
	float _objectiveElapsed = 0.0f;
	float _objectiveDuration = 0.0f;
	bool _announceStageComplete = false;
	float _bestTime = 999.0f;
	bool _isNewRecord = false;
	Vector3 _aiPosition;
	Quaternion _aiRotation;
	int _aiCheckpoint = 0;
	float _aiSpeed = 12.0f;
	// Dense road-following waypoints for the AI, resampled from the level path;
	// _checkpoints stays the sparse set the player has to drive through.
	// Route nodes named by AddStageWaypoint, in declaration order.
	std::vector<Vector3> _stageWaypoints;
	bool _raceCarNeedsPlacement = false;
	std::vector<Vector3> _racePath;
	// Every car the stage declares as racing, in declaration order. Their drivers
	// are only created once the circuit exists (CloseMission): the game's scripts
	// declare the cars before the race objective and its waypoints.
	std::vector<Vehicle*> _raceVehicles;
	std::vector<std::unique_ptr<RaceOpponent>> _raceOpponents;
	// Catch-up curves by car name, held until that car's driver exists.
	std::map<std::string, RaceOpponent::Catchup> _raceCatchups;
	// Where the mission puts the player, for when the character is created after
	// the script has run (the first start).
	bool _hasPlayerStart = false;
	Vector3 _playerStart;
	std::unique_ptr<ChaseManager> _chaseManager;
};

} // namespace Donut
