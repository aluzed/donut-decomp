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
	// The player starts on foot at `locator`, with their own car parked at
	// `carLocator` -- the car a mission's first "getin" asks them into.
	void SetMissionResetPlayerOutCar(const std::string& locator, const std::string& carLocator = "");
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
	void AddObjective(const std::string& type, const std::string& argument = "");
	void UpdateAI(double dt);
	void ResetBestTime();
	void SaveGameState();
	void LoadGameState();
	void SetPresentationBitmap(const std::string& path);
	// The mission's briefing picture (dynaload/images/mis01_01.p3d), as its
	// script names it; empty when it names none.
	const std::string& GetPresentationBitmap() const { return _presentationBitmap; }
	void EnableTutorialMode(bool enable);
	void CreateChaseManager(const std::string& copCar, const std::string& aiScript, int count);

	void CloseObjective();
	void SetObjTargetVehicle(const std::string& target);
	// How far a "losetail" objective wants the chasing car left behind, in metres.
	void SetObjDistance(float metres) { _objectiveDistance = metres; }
	void SetConversation(const std::string& name) { _conversation = name; }
	// The street races switch the police off for their stage.
	void DisableHitAndRun() { _hitAndRunOff = true; }
	bool IsHitAndRunOff() const { return _missionActive && _hitAndRunOff; }
	// Fails the mission under way, if there is one still undecided.
	void FailMission(const std::string& reason, const std::string& hint)
	{
		if (_missionActive && !_raceDecided)
			missionFailed(reason, hint);
	}

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
	// The objective as the HUD shows it: the game's own line for the stage
	// ("COLLECT LISA'S SCIENCE PROJECT") or else "Objective: <type>", and how
	// far along a delivery is ("... 3/9").
	std::string GetObjectiveText() const;
	// The stage's objective icon (SetHUDIcon: "apu", "kwik"...), the name of
	// a picture of msnicons/; empty when none.
	const std::string& GetHUDIcon() const { return _hudIcon; }
	// The mission's name and summary from the phrase table ("S-M-R-T", "RACE
	// PRINCIPAL SKINNER TO THE SCHOOL..."), empty when it has none.
	std::string GetMissionTitle(bool info = false) const;
	// The advice for the last failure, in the game's words.
	std::string GetFailureHint() const;
	// What went wrong, in the game's words ("You destroyed your vehicle.").
	std::string GetFailureReason() const;
	// How long the mission has taken, measured.
	float GetMissionTime() const { return _missionElapsed; }
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
	// A prop to be picked up (level 7's bomb barrel), standing at a locator:
	// a "pickupitem" objective is met by reaching it, as a "goto" is.
	void AddCollectibleStateProp(const std::string& prop, const std::string& locator) { _stateProps[prop] = locator; }
	void SetPickupTarget(const std::string& prop);
	void SetTalkToTarget(const std::string& npc);
	void SetDurationTime(float seconds);
	// The "fmv" stage's movie ("fmv2.rmv"), and whether the music stays off
	// after it ("stopmusic").
	void SetFMVInfo(const std::string& file, const std::string& option);
	// Brings a car declared earlier into this stage's action ("race", "evade",
	// "target" race against the player).
	void ActivateVehicle(const std::string& name, const std::string& behaviour);
	// SetFadeOut / SetIrisWipe: the stage, done, closes the screen over so
	// many seconds (to black, or a circle closing on its middle) before the
	// next one begins, which opens it again; what changes between them (a car
	// swapped) is not seen.
	enum class Transition
	{
		None,
		Fade,
		Iris,
	};
	void SetStageTransition(Transition transition, float seconds);
	// ShowHUD("false"): the mission plays without the HUD (level 7's last, a
	// movie); shown again with the next mission.
	void ShowHUD(bool shown) { _hudShown = shown; }
	bool IsHudShown() const { return _hudShown; }
	// The one under way, and how much of the screen it has closed, 0 to 1.
	Transition GetTransition(float& closed) const;
	// The script's ShowStageComplete(): announce the stage when it is done. It
	// does not complete anything itself.
	void AnnounceStageComplete();
	// How many stages have been announced complete, for the HUD to notice
	// another.
	int GetStagesAnnounced() const { return _stagesAnnounced; }
	// RESET_TO_HERE: a failed mission is retried from this stage.
	void ResetToHere();
	// The stage clock counts up from SetStageTime instead of down (gr1's race
	// against the clock), and SetParTime is the time not to exceed.
	void UseElapsedTime();
	void SetParTime(float seconds);
	// A "coins" objective's entry fee.
	void SetCoinFee(int coins);
	// Whether a finished mission may set a best time. --autopilot turns it off:
	// teleporting through a mission in seconds is not a record.
	void SetRecordsEnabled(bool enabled) { _recordsEnabled = enabled; }
	// The stage's line in the game's phrase table (MISSION_OBJECTIVE_<index>),
	// what the HUD tells the player to do.
	void SetStageMessageIndex(int index) { _stageMessage = index; }
	// After a failure: the player back at the mission's start, and the stage
	// last marked RESET_TO_HERE played again. False when there is no mission to
	// retry, and the caller should reload it instead.
	bool RetryFromCheckpoint();
	// A way to fail the stage. "outofvehicle" (out of the car longer than
	// SetCondTime) and "damage" (the car's health down to SetCondMinHealth) are
	// watched; "timeout" is the stage clock; "race"/"position" are the race's own
	// result. The rest are noted and ignored.
	void AddCondition(const std::string& type, const std::string& argument = "");
	void SetCondTime(int milliseconds);
	void SetCondMinHealth(float health);
	// "followdistance": how far the player may fall behind the car they follow.
	void SetFollowDistances(float minimum, float maximum);
	// Where the current objective wants the player: the next checkpoint or
	// destination, or whoever they are to talk to. For --autopilot.
	bool GetObjectiveTarget(Vector3& position) const;
	bool ObjectiveWantsCar() const { return _objectiveType == "getin"; }
	// A dump's dropped items are lying about to be picked up.
	bool HasPickupsLying() const { return !_pickups.empty(); }
	// The car a "follow", "destroy" or "dump" objective is after, if any.
	const Vehicle* GetFollowTarget() const
	{
		return (_objectiveType == "follow" || _objectiveType == "destroy" || _objectiveType == "dump") &&
		               !_raceOpponents.empty()
		           ? &_raceOpponents.front()->GetVehicle()
		           : nullptr;
	}
	// What GetObjectiveTarget points at, for the HUD.
	std::string GetObjectiveTargetLabel() const;
	// A trigger volume the player has just entered or left (Level::UpdateTriggers).
	void OnTrigger(const Level::Trigger& trigger, bool entered);
	// The player has gone into an interior, or (empty) come back out.
	void OnInteriorChanged(const std::string& interior);
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
	// Who drives a mission car: the stage's driver (AddStageVehicle's last
	// argument), or else the car's own (its script's SetDriver); empty for none.
	std::string GetVehicleDriver(const Vehicle* vehicle) const;
	// The car the mission gave the player (SetMissionResetPlayerInCar/OutCar), or null.
	Vehicle* GetPlayerCar() const { return _playerCar; }
	// The phone booth's car brought to `position`: the player's own moved
	// there, or another put in its place as the player's (the one before is
	// left where it is).
	Vehicle& SummonPlayerCar(const std::string& model, const Vector3& position);

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
	// Where each mission car was last declared, to put it back on a retry.
	std::map<std::string, std::pair<Vector3, Quaternion>> _vehicleSpawns;
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
	// The music script's event for this mission's `what` ("start", "win"):
	// "M1_start", "Sunday_Drive_start", "StreetRace01_win", "Bonus_lose"; empty
	// for a mission it has none for.
	std::string musicEvent(const std::string& what) const;
	// The mission's music has given way to its ending's.
	bool _missionMusicEnded = true;
	// Watches the objectives that are not met by driving through checkpoints.
	void updateObjective(double dt);

	// The current objective's particulars.
	bool _hasDestination = false;
	Vector3 _destination;
	std::string _talkToTarget;
	std::map<std::string, Vector3> _npcPositions;
	float _objectiveElapsed = 0.0f;
	float _objectiveDuration = 0.0f;
	bool _hudShown = true;
	int _stagesAnnounced = 0;
	std::string _hudIcon;
	Transition _stageTransition = Transition::None;
	float _stageTransitionSeconds = 0.0f;
	// Under way: closing (the stage done, the next waiting) or opening.
	Transition _transition = Transition::None;
	float _transitionSeconds = 0.0f, _transitionElapsed = 0.0f;
	bool _transitionOpening = false;
	// The next stage, or the mission complete after the last.
	void nextStage();
	std::string _fmvFile;
	bool _fmvStopMusic = false, _fmvStarted = false;
	bool _announceStageComplete = false;
	// The interior an "interior" objective asks the player into.
	std::string _interiorTarget;
	std::string _playerInterior;
	std::size_t _resetStage = 0;
	bool _clockCountsUp = false;
	float _parTime = 0.0f;
	int _coinFee = 0;
	bool _recordsEnabled = true;
	int _stageMessage = -1;
	// Time since the mission's first stage began, for its completion time. The
	// stage clock cannot give it: it counts down, or up, or is not running.
	float _missionElapsed = 0.0f;
	// Where the mission's reset commands put the player's car, and whether the
	// player starts at its wheel.
	std::string _playerCarLocator;
	bool _playerStartsInCar = false;

	struct Condition
	{
		std::string type;
		float seconds = 0.0f;
		float minHealth = 0.0f;
		float maxDistance = 0.0f;
		// "keepbarrel": how many barrels the player has carried so far.
		int count = 0;
	};
	std::vector<Condition> _conditions;
	// A "delivery": items to pick up, in any order. Taken ones are removed.
	std::vector<std::pair<std::string, Vector3>> _pickups;
	std::size_t _pickupsTotal = 0;
	// A "destroy" target's health, 0-100, and what tracks the player ramming it.
	float _targetHealth = 100.0f;
	float _lastTargetGap = -1.0f;
	float _rammingCooldown = 0.0f;
	// The speed the player's car just rammed `target` at, or 0 for no hit.
	float rammingSpeed(double dt, const Vehicle& target);
	// A "dump"'s items not yet knocked out of the vehicle.
	std::vector<std::string> _dumpQueue;
	// How long the player has been out of their car, for "outofvehicle".
	float _outOfVehicleSeconds = 0.0f;
	void updateConditions(double dt);
	// `hint` names the phrase-table line the failure screen offers as advice.
	void missionFailed(const std::string& reason, const std::string& hint = "");
	std::string _failureHint;
	float _bestTime = 999.0f; // this mission's, 999 when it has none
	// Every mission's best time, by "L<level>:<mission>", kept in donut_save.dat
	// one per line. It was a single number shared by every mission.
	std::map<std::string, float> loadBestTimes() const;
	void saveBestTimes(const std::map<std::string, float>& times) const;
	std::string bestTimeKey() const;
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
	// Cars activated to "chase" the player: driven along the roads to wherever
	// the player is, the route drawn again as the player moves on.
	struct Chaser
	{
		Vehicle* vehicle = nullptr;
		std::unique_ptr<RaceOpponent> driver;
		Vector3 routedTo;
		double sinceRoute = 0.0;
	};
	std::vector<Chaser> _chasers;
	void updateChasers(double dt);
	// The chasing car the objective names, or the first; null when none.
	const Vehicle* chasingTarget() const;
	float _objectiveDistance = 0.0f;
	// AddObjective's second argument: the car a "buycar" wants ("plowk_v").
	std::string _objectiveArgument;
	// The stage's conversation (SetDialogueInfo's third argument, "bladder"),
	// and whether its lines have been queued.
	std::string _conversation;
	bool _conversationQueued = false;
	bool _hitAndRunOff = false;
	// The lines of `_conversation` for this mission, in order.
	std::vector<std::string> conversationLines() const;
	// The player has had the car being followed within the follow distance.
	bool _followInRange = false;
	std::map<const Vehicle*, std::string> _vehicleDrivers;
	std::map<std::string, std::string> _stateProps;
	// Catch-up curves by car name, held until that car's driver exists.
	std::map<std::string, RaceOpponent::Catchup> _raceCatchups;
	// Where the mission puts the player, for when the character is created after
	// the script has run (the first start).
	bool _hasPlayerStart = false;
	Vector3 _playerStart;
	std::unique_ptr<ChaseManager> _chaseManager;
	std::string _presentationBitmap;
};

} // namespace Donut
