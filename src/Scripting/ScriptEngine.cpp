// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "ScriptEngine.h"
#include "AI/ChaseManager.h"
#include "AI/PathGraph.h"
#include "AI/RaceOpponent.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Core/File.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "UI/GameText.h"
#include "Level.h"
#include "Physics/WorldPhysics.h"
#include "Scripting/Commands.h"
#include "Vehicle.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

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
	_startCamera = _animatedCamera = {};
	_hudShown = true;

	// Mission locators (spawn points, car starts, AI path nodes, race_finish...)
	// live in the mission's own P3D files, which nothing loaded: level.p3d holds
	// the level-wide ones, <id>.p3d the mission's own. Every "locator not found"
	// warning in the log came from here, and AddStageWaypoint could never resolve
	// its argument.
	const std::string missions = "missions/" + _game.GetLevelDirectory() + "/";
	_game.GetLevel().LoadP3D(missions + "level.p3d");
	_game.GetLevel().LoadP3D(missions + id + ".p3d");

	_currentStage = -1;
	_noTraffic = false;
	_zones.clear();

	// Any vehicle still here was never torn down (CleanupMission is what does
	// that). Dropping the unique_ptrs would leave its rigid body and action
	// behind in the physics world, dangling: the first run left a car falling
	// forever, reaching Y = -440.
	_raceOpponents.clear();
	_chasers.clear();
	_raceVehicles.clear();
	_raceCatchups.clear();
	_targetCatchups.clear();
	_finishedAhead.clear();
	_racePosition = 1;
	_playerCar = nullptr;
	_stageScripts.clear();
	_recordingStage = false;
	_resetStage = 0;
	_playerCarLocator.clear();
	_playerStartsInCar = false;
	_npcPositions.clear();
	_stageWaypoints.clear();
	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_vehicleSpawns.clear();
	_countdown.clear();
	_countdownElapsed = 0.0f;
	_stageTimeRemaining = -1.0f;

	const auto times = loadBestTimes();
	const auto best = times.find(bestTimeKey());
	_bestTime = best != times.end() ? best->second : 999.0f;
	if (best != times.end())
		Log::Info("ScriptEngine: best time {:.1f}s", _bestTime);

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
		return;
	}

	// "In car": at the wheel of their own car, there. m0 opens on getting into it
	// and was left standing beside nothing.
	_playerCarLocator = locator;
	_playerStartsInCar = true;
	PlacePlayerCar("current", locator);
	PutPlayerInCar();
}

void ScriptEngine::SetMissionResetPlayerOutCar(const std::string& locator, const std::string& carLocator)
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

	// m0, m2, m3 and bm1 all open on "getin" the player's own car, which nothing
	// had put in the world: they could not get past their first stage.
	if (!carLocator.empty() && _playerCar == nullptr)
	{
		_playerCarLocator = carLocator;
		_playerStartsInCar = false;
		PlacePlayerCar("current", carLocator);
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
	// Completion belongs to gameplay -- see objectiveComplete.
	Log::Info("ScriptEngine: mission '{}' loaded, {} stage(s)", _missionId, _stageScripts.size());
	_game.GetAudioManager().PlayMusicEvent(musicEvent("start"));
	_missionMusicEnded = false;
	_missionElapsed = 0.0f;
	// A transition left from the mission before does not carry over.
	_transition = Transition::None;
	// Its camera move, as its title shows.
	_cameraMove = !_startCamera.file.empty() ? _startCamera : _animatedCamera;
	if (!_stageScripts.empty())
		beginStage(0);
}

void ScriptEngine::beginStage(std::size_t index)
{
	_stageCursor = index;

	// What a stage sets up is its own: the next one starts from a clean slate,
	// apart from the cars, which stay in the world (m1 parks Skinner's car in
	// one stage and races it in the next).
	_raceOpponents.clear();
	_chasers.clear();
	_raceVehicles.clear();
	_raceCatchups.clear();
	_targetCatchups.clear();
	_finishedAhead.clear();
	_racePosition = 1;
	_stageWaypoints.clear();
	_objectiveDistance = 0.0f;
	_followInRange = false;
	_conversation.clear();
	_conversationQueued = false;
	_conversationCams.clear();
	_bestSide.clear();
	_dialogueCharacters = {};
	_hitAndRunOff = false;
	_racePath.clear();
	_checkpoints.clear();
	_currentCheckpoint = 0;
	_currentLap = 0;
	_objectiveType.clear();
	_hasDestination = false;
	_talkToTarget.clear();
	_objectiveElapsed = 0.0f;
	_fmvFile.clear();
	_fmvStopMusic = _fmvStarted = false;
	_objectiveDuration = 0.0f;
	_stageTimeRemaining = -1.0f;
	_initialStageTime = 0.0f;
	_countdown.clear();
	_countdownElapsed = 0.0f;
	_noTraffic = false;
	_announceStageComplete = false;
	_raceDecided = false;
	_conditions.clear();
	_outOfVehicleSeconds = 0.0f;
	_clockCountsUp = false;
	_parTime = 0.0f;
	_stageMessage = -1;
	_coinFee = 0;
	_stageTransition = Transition::None;
	_stageTransitionSeconds = 0.0f;
	_hudIcon.clear();

	for (const auto& statement : _stageScripts[index])
		Commands::RunLine(statement);

	// A destination is driven or walked to like a one-checkpoint, one-lap race,
	// which is what AdvanceCheckpoint already completes.
	if (_hasDestination && _objectiveType != "race")
	{
		_checkpoints = {_destination};
		_totalLaps = 1;
	}

	buildRaceCircuit();

	Log::Info("ScriptEngine: stage {}/{} begins, objective '{}'", index + 1, _stageScripts.size(), _objectiveType);
}

void ScriptEngine::objectiveComplete()
{
	Log::Info("ScriptEngine: stage {}/{} objective '{}' complete{}", _stageCursor + 1, _stageScripts.size(),
	          _objectiveType, _announceStageComplete ? " -- STAGE COMPLETE" : "");
	if (_announceStageComplete)
	{
		_game.GetAudioManager().PlayRaw(SoundGenerator::Chirp(500, 900, 0.25f), 22050, 1, 16);
		++_stagesAnnounced;
	}

	// Closed first, if the stage says so; the next stage once it is.
	if (_stageTransition != Transition::None && _stageTransitionSeconds > 0.0f)
	{
		_transition = _stageTransition;
		_transitionSeconds = _stageTransitionSeconds;
		_transitionElapsed = 0.0f;
		_transitionOpening = false;
		_objectiveType.clear();
		return;
	}
	nextStage();
}

void ScriptEngine::nextStage()
{
	if (_stageCursor + 1 < _stageScripts.size())
		beginStage(_stageCursor + 1);
	else
		missionComplete();
}

void ScriptEngine::SetStageTransition(Transition transition, float seconds)
{
	_stageTransition = transition;
	_stageTransitionSeconds = seconds;
}

ScriptEngine::Transition ScriptEngine::GetTransition(float& closed) const
{
	closed = 0.0f;
	if (_transition == Transition::None)
		return Transition::None;
	const float along = std::clamp(_transitionElapsed / _transitionSeconds, 0.0f, 1.0f);
	closed = _transitionOpening ? 1.0f - along : along;
	return _transition;
}

void ScriptEngine::updateObjective(double dt)
{
	if (_game.GetState() != GameState::InGame || _objectiveType.empty())
		return;

	_objectiveElapsed += static_cast<float>(dt);
	const Vector3 player = _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();

	// How close the player has to come to whoever they are to talk to.
	constexpr float kTalkToRange = 4.0f;
	// There is no dialogue system: give the scene a moment, then move on.
	constexpr float kDialogueSeconds = 3.0f;

	if (_objectiveType == "getin")
	{
		if (_game.GetPlayerVehicle() != nullptr)
			objectiveComplete();
	}
	else if (_objectiveType == "talkto")
	{
		const auto npc = _npcPositions.find(_talkToTarget);
		if (npc == _npcPositions.end())
		{
			Log::Warn("ScriptEngine: nobody called '{}' to talk to, skipping", _talkToTarget);
			objectiveComplete();
		}
		else if ((npc->second - player).Length() < kTalkToRange)
			objectiveComplete();
	}
	else if (_objectiveType == "dialogue")
	{
		// The conversation itself, line after line, from the game's recorded
		// voices; over when the last line has been said. Without them, a pause.
		auto& audio = _game.GetAudioManager();
		if (!_conversationQueued)
		{
			_conversationQueued = true;
			_conversationLineBase = audio.GetDialogueLinesStarted();
			const auto lines = conversationLines();
			for (const auto& line : lines)
				audio.QueueDialogue(line, AudioManager::DialoguePriority::Gameplay);
			Log::Info("ScriptEngine: conversation '{}', {} lines", _conversation, lines.size());
			if (lines.empty())
				_conversation.clear();
		}
		if (_conversation.empty() ? _objectiveElapsed >= kDialogueSeconds
		                          : (_objectiveElapsed >= 0.5 && !audio.IsDialoguePlaying()))
			objectiveComplete();
	}
	else if (_objectiveType == "delivery" || _objectiveType == "dump")
	{
		// A dump's items come out of the vehicle being rammed, one a hit, where
		// it is at the time: their locators are placeholders 110m underground.
		if (_objectiveType == "dump" && !_raceOpponents.empty() && !_dumpQueue.empty())
		{
			const Vehicle& truck = _raceOpponents.front()->GetVehicle();
			const float closing = rammingSpeed(dt, truck);
			if (closing > 0.0f)
			{
				Log::Info("ScriptEngine: '{}' rammed at {:.0f} m/s, drops '{}'", truck.GetName(), closing,
				          _dumpQueue.front());
				_pickups.emplace_back(_dumpQueue.front(), truck.GetPosition());
				_dumpQueue.erase(_dumpQueue.begin());
			}
		}

		// Picked up by coming within reach, in whatever order.
		constexpr float kPickupRange = 4.0f;
		for (auto it = _pickups.begin(); it != _pickups.end();)
		{
			if ((it->second - player).Length() < kPickupRange)
			{
				Log::Info("ScriptEngine: picked up '{}', {}/{}", it->first,
				          _pickupsTotal - _pickups.size() - _dumpQueue.size() + 1, _pickupsTotal);
				_game.GetAudioManager().PlayRaw(SoundGenerator::Beep(900, 0.08f), 22050, 1, 16);
				it = _pickups.erase(it);
			}
			else
				++it;
		}
		if (_pickups.empty() && _dumpQueue.empty())
		{
			if (_pickupsTotal == 0)
				Log::Warn("ScriptEngine: '{}' objective with nothing found to pick up, skipping", _objectiveType);
			objectiveComplete();
		}
	}
	else if (_objectiveType == "destroy")
	{
		if (_raceOpponents.empty())
		{
			Log::Warn("ScriptEngine: nothing to destroy, skipping");
			objectiveComplete();
			return;
		}

		constexpr float kDamagePerClosingSpeed = 3.0f;
		const Vehicle& target = _raceOpponents.front()->GetVehicle();
		const float closing = rammingSpeed(dt, target);
		if (closing > 0.0f)
		{
			_targetHealth -= closing * kDamagePerClosingSpeed;
			Log::Info("ScriptEngine: '{}' rammed at {:.0f} m/s, health {:.0f}", target.GetName(), closing,
			          _targetHealth > 0.0f ? _targetHealth : 0.0f);
			if (_targetHealth <= 0.0f)
			{
				Log::Info("ScriptEngine: '{}' destroyed", target.GetName());
				_raceOpponents.clear();
				objectiveComplete();
			}
		}
	}
	else if (_objectiveType == "follow")
	{
		// Met in UpdateAI, when the target arrives. With nobody to follow it
		// cannot be met at all.
		if (_raceOpponents.empty())
		{
			Log::Warn("ScriptEngine: nothing to follow, skipping");
			objectiveComplete();
		}
	}
	else if (_objectiveType == "coins")
	{
		// Paying to enter: met the moment the purse holds the fee, which is then
		// taken. Until then the player goes and picks up the level's coins.
		// --autopilot does not collect coins, so it is let in for free.
		if (_game.SpendCoins(_coinFee))
		{
			Log::Info("ScriptEngine: paid the {}-coin fee, {} left", _coinFee, _game.GetCoins());
			objectiveComplete();
		}
		else if (_game.IsAutopilot())
		{
			Log::Info("ScriptEngine: coin fee of {} waived for --autopilot", _coinFee);
			objectiveComplete();
		}
	}
	else if (_objectiveType == "losetail")
	{
		// Shaking off the car on the player's tail: met once it is the stage's
		// distance behind. The stage clock is what fails it.
		const Vehicle* tail = chasingTarget();
		const Vector3 player =
		    _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();
		const float needed = _objectiveDistance > 0.0f ? _objectiveDistance : 150.0f;
		if (tail == nullptr)
		{
			Log::Warn("ScriptEngine: nobody on the player's tail, skipping");
			objectiveComplete();
		}
		else if ((tail->GetPosition() - player).Length() > needed)
		{
			Log::Info("ScriptEngine: '{}' shaken off, {:.0f}m behind", tail->GetName(), (tail->GetPosition() - player).Length());
			objectiveComplete();
		}
	}
	else if (_objectiveType == "gooutside")
	{
		if (_game.GetInterior().empty())
			objectiveComplete();
	}
	else if (_objectiveType == "buycar")
	{
		// The car the next mission needs, bought with coins at the price
		// rewards.mfk sets; owned already, nothing to buy.
		if (_game.BuyCar(_objectiveArgument))
			objectiveComplete();
	}
	else if (_objectiveType == "buyskin")
	{
		// An outfit the next mission needs, bought and worn.
		if (_game.BuySkin(_objectiveArgument))
			objectiveComplete();
	}
	else if (_objectiveType == "fmv")
	{
		// Its movie, over the game, which waits for it; none to play (no
		// player or file, an unattended run), on at once.
		if (!_fmvStarted)
		{
			_fmvStarted = true;
			if (!_game.PlayStoryMovie(_fmvFile, _fmvStopMusic))
				objectiveComplete();
		}
		else if (!_game.IsMoviePlaying())
			objectiveComplete();
	}
	else if (_objectiveType == "timer")
	{
		if (_objectiveElapsed >= _objectiveDuration)
			objectiveComplete();
	}
	else if (_objectiveType == "interior" && !_playerInterior.empty() && _playerInterior == _interiorTarget)
	{
		// Already inside: gone in during the stage before (level 4 m5 walks up
		// to the Kwik-E-Mart's door and through it before this stage begins).
		objectiveComplete();
	}
	else if (_objectiveType == "destroyboss")
	{
		// The barrel driven into the saucer's beam: up it goes and blows up
		// against the hull. The stage's "keepbarrel" counts the barrels
		// carried; the fourth (level 7's last mission) brings it down.
		if (!_game.HasBoss())
		{
			Log::Warn("ScriptEngine: no saucer for 'destroyboss', skipping");
			objectiveComplete();
		}
		else if (_game.IsPlayerInBossBeam())
		{
			int barrels = 0;
			for (const auto& condition : _conditions)
				if (condition.type == "keepbarrel")
					barrels = std::max(barrels, condition.count);
			_game.HitBoss(barrels >= 4);
			objectiveComplete();
		}
	}
	else if (_objectiveType == "race" || _objectiveType == "goto" || _objectiveType == "interior" ||
	         _objectiveType == "pickupitem")
	{
		// Met through the checkpoints (AdvanceCheckpoint). One with nowhere to go
		// cannot be met at all, so do not let it hold the mission up.
		if (_checkpoints.empty())
		{
			Log::Warn("ScriptEngine: '{}' objective with no destination, skipping", _objectiveType);
			objectiveComplete();
		}
	}
	else
	{
		Log::Warn("ScriptEngine: '{}' objectives are not implemented, skipping", _objectiveType);
		objectiveComplete();
	}
}

float ScriptEngine::rammingSpeed(double dt, const Vehicle& target)
{
	// There is no damage model for cars, so a ram is judged from the gap between
	// the player's car and the target: touching (under 5m between centres) while
	// closing faster than 4 m/s is a hit, at most one every half second.
	constexpr float kContactGap = 5.0f;
	constexpr float kHitClosingSpeed = 4.0f;
	// Faster than any car closes is a car being moved, not driven: a teleport
	// read as a 40 m/s hit.
	constexpr float kMaxClosingSpeed = 25.0f;

	_rammingCooldown -= static_cast<float>(dt);
	const Vehicle* car = _game.GetPlayerVehicle();
	if (car == nullptr || dt <= 0.0)
	{
		_lastTargetGap = -1.0f;
		return 0.0f;
	}

	const float gap = (target.GetPosition() - car->GetPosition()).Length();
	const float closing = _lastTargetGap < 0.0f ? 0.0f : (_lastTargetGap - gap) / static_cast<float>(dt);
	_lastTargetGap = gap;
	if (gap >= kContactGap || closing <= kHitClosingSpeed || closing > kMaxClosingSpeed || _rammingCooldown > 0.0f)
		return 0.0f;

	_rammingCooldown = 0.5f;
	_game.AddShake(0.6f);
	return closing;
}

void ScriptEngine::SetDestination(const std::string& locator, const std::string& type)
{
	// An interior is reached through its entrance, whose locator carries the
	// interior's name: SpringfieldElementary -> SpringfieldElementaryEntry.
	Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (_objectiveType == "interior")
	{
		_interiorTarget = locator;
		if (pos == Vector3::Zero)
			pos = _game.GetLevel().GetLocatorPosition(locator + "Entry");
	}
	if (pos == Vector3::Zero)
	{
		Log::Warn("ScriptEngine: destination '{}' not found", locator);
		return;
	}
	_destination = pos;
	_hasDestination = true;
	Log::Info("ScriptEngine: destination '{}' ({}) at ({:.1f}, {:.1f}, {:.1f})", locator, type, pos.X, pos.Y, pos.Z);
}

void ScriptEngine::SetPickupTarget(const std::string& prop)
{
	const auto found = _stateProps.find(prop);
	if (found == _stateProps.end())
	{
		Log::Warn("ScriptEngine: pickup target '{}' was not placed", prop);
		return;
	}
	SetDestination(found->second, "pickup");
}

void ScriptEngine::SetTalkToTarget(const std::string& npc)
{
	_talkToTarget = npc;
}

void ScriptEngine::SetDurationTime(float seconds)
{
	_objectiveDuration = seconds;
}

void ScriptEngine::SetFMVInfo(const std::string& file, const std::string& option)
{
	_fmvFile = file;
	_fmvStopMusic = option == "stopmusic";
}

void ScriptEngine::ActivateVehicle(const std::string& name, const std::string& behaviour)
{
	for (auto& vehicle : _missionVehicles)
	{
		if (vehicle->GetName() != name)
			continue;
		if (behaviour == "race" || behaviour == "evade" || behaviour == "target")
			_raceVehicles.push_back(vehicle.get());
		if (behaviour == "chase")
		{
			_chasers.push_back({vehicle.get()});
			// The game's chase script (M1Chase.con and the like) has had the car
			// on the player's heels during the stage before; nothing here runs
			// it, so level 2 m1's Skinner set off 477m away, already "lost".
			// Bring a car that far off to the road 40m behind the player.
			const Vector3 player =
			    _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();
			if ((vehicle->GetPosition() - player).Length() > 80.0f)
			{
				Vector3 back = vehicle->GetPosition() - player;
				back.Y = 0.0f;
				back.Normalize();
				const auto& graph = _game.GetPathGraph();
				const int node = graph.FindNearestRoadNode(player + back * 40.0f);
				const Vector3 spot = node >= 0 ? graph.GetNodes()[node].position : player + back * 40.0f;
				const Vector3 toPlayer = player - spot;
				vehicle->Teleport(spot, Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z), 0.0f)));
				Log::Info("ScriptEngine: '{}' brought onto the player's tail", name);
			}
		}
		Log::Info("ScriptEngine: vehicle '{}' activated as '{}'", name, behaviour);
		return;
	}
	Log::Warn("ScriptEngine: no vehicle '{}' to activate", name);
}

bool ScriptEngine::GetObjectiveTarget(Vector3& position) const
{
	if (_currentCheckpoint >= 0 && _currentCheckpoint < static_cast<int>(_checkpoints.size()))
	{
		position = _checkpoints[_currentCheckpoint];
		return true;
	}
	if ((_objectiveType == "follow" || _objectiveType == "destroy" ||
	     (_objectiveType == "dump" && _pickups.empty())) &&
	    !_raceOpponents.empty())
	{
		position = _raceOpponents.front()->GetVehicle().GetPosition();
		return true;
	}
	// Under the saucer, in its beam.
	if (_objectiveType == "destroyboss" && _game.GetBossGround(position))
		return true;
	// Somewhere on the roads well beyond the tail's reach, straight away from it.
	if (const Vehicle* tail = _objectiveType == "losetail" ? chasingTarget() : nullptr)
	{
		const Vector3 player =
		    _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();
		Vector3 away = player - tail->GetPosition();
		away.Y = 0.0f;
		if (away.LengthSquared() < 0.01f)
			away = Vector3(0.0f, 0.0f, 1.0f);
		away.Normalize();
		const float reach = (_objectiveDistance > 0.0f ? _objectiveDistance : 150.0f) + 60.0f;
		const auto& graph = _game.GetPathGraph();
		const int node = graph.FindNearestRoadNode(player + away * reach);
		position = node >= 0 ? graph.GetNodes()[node].position : player + away * reach;
		return true;
	}
	if ((_objectiveType == "delivery" || _objectiveType == "dump") && !_pickups.empty())
	{
		// The nearest one: they can be taken in any order.
		const Vector3 player = _game.GetPlayerPosition();
		position = _pickups.front().second;
		for (const auto& pickup : _pickups)
			if ((pickup.second - player).Length() < (position - player).Length())
				position = pickup.second;
		return true;
	}
	const auto npc = _npcPositions.find(_talkToTarget);
	if (_objectiveType == "talkto" && npc != _npcPositions.end())
	{
		position = npc->second;
		return true;
	}
	return false;
}

void ScriptEngine::AddCondition(const std::string& type, const std::string& argument)
{
	Condition condition {type};
	if (!argument.empty())
		condition.count = std::atoi(argument.c_str());
	_conditions.push_back(condition);
}

void ScriptEngine::SetCondTime(int milliseconds)
{
	if (!_conditions.empty())
		_conditions.back().seconds = static_cast<float>(milliseconds) / 1000.0f;
}

void ScriptEngine::SetFollowDistances(float minimum, float maximum)
{
	if (!_conditions.empty())
		_conditions.back().maxDistance = maximum;
}

void ScriptEngine::SetCondMinHealth(float health)
{
	if (!_conditions.empty())
		_conditions.back().minHealth = health;
}

void ScriptEngine::updateConditions(double dt)
{
	if (_game.GetState() != GameState::InGame || _raceDecided)
		return;

	const bool inCar = _game.GetPlayerVehicle() != nullptr;
	const Vector3 player = inCar ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();
	_outOfVehicleSeconds = inCar ? 0.0f : _outOfVehicleSeconds + static_cast<float>(dt);

	for (const auto& condition : _conditions)
	{
		// Only once the player has a car to be out of: the countdown seats them,
		// and a stage that has not given them one cannot ask them to stay in it.
		if (condition.type == "outofvehicle" && _playerCar != nullptr && condition.seconds > 0.0f &&
		    _outOfVehicleSeconds > condition.seconds)
		{
			missionFailed(fmt::format("out of the car for more than {:.0f}s", condition.seconds),
			              "MISSION_FAILED_PLAYER_OUT_OF_VEHICLE_HINT1");
			return;
		}
		// The car is only lost once the player has had it in range: level 5 m4's
		// Wiggum sets off from his own spot, 300m from where the stage before
		// left the player, and the mission failed the moment it began.
		const float followed = _raceOpponents.empty()
		                           ? 0.0f
		                           : (_raceOpponents.front()->GetVehicle().GetPosition() - player).Length();
		if (condition.type == "followdistance" && condition.maxDistance > 0.0f && followed <= condition.maxDistance)
			_followInRange = true;
		if (condition.type == "followdistance" && condition.maxDistance > 0.0f && !_raceOpponents.empty() &&
		    _followInRange && followed > condition.maxDistance)
		{
			missionFailed(fmt::format("lost the car being followed (over {:.0f}m)", condition.maxDistance),
			              "MISSION_FAILED_FOLLOW_DISTANCE_HINT1");
			return;
		}
		// The car's health is a 0-100 meter; the scripts give a fraction.
		if (condition.type == "damage" && inCar && _game.GetCarHealth() <= condition.minHealth * 100.0f)
		{
			missionFailed("the car is wrecked", "MISSION_FAILED_VEHICLE_DAMAGE_HINT1");
			return;
		}
	}
}

void ScriptEngine::missionFailed(const std::string& reason, const std::string& hint)
{
	_failureHint = hint;
	_raceDecided = true;
	_stageTimeRemaining = -1.0f;
	Log::Info("ScriptEngine: mission failed - {}", reason);
	_game.AddShake(2.0f);
	_game.GetAudioManager().PlayRaw(SoundGenerator::Beep(150, 0.5f), 22050, 1, 16);
	_game.SayLine("w_mfail");
	// The mission's ending, then the level's music.
	auto& audio = _game.GetAudioManager();
	audio.SetMusicBase("OF_explore_mission");
	const bool outOfCar = hint == "MISSION_FAILED_PLAYER_OUT_OF_VEHICLE_HINT1";
	if (!(outOfCar && audio.PlayMusicEvent(musicEvent("get_out_of_car"), true)) &&
	    !audio.PlayMusicEvent(musicEvent("lose"), true))
		audio.ResumeMusicBase();
	_missionMusicEnded = true;
	_game.Fire(GameEvent::MissionLost);
}

std::string ScriptEngine::musicEvent(const std::string& what) const
{
	// m1, m1sd (the drive to it), sr1, bm1.
	const std::string& id = _missionId;
	const auto digits = [&](std::size_t from, std::size_t to) {
		return to > from && to <= id.size() &&
		       std::all_of(id.begin() + from, id.begin() + to, [](unsigned char c) { return std::isdigit(c); });
	};
	if (id.size() > 3 && id.compare(id.size() - 2, 2, "sd") == 0)
		return what == "get_out_of_car" ? "Sunday_Drive_Get_out_of_Car" : "Sunday_Drive_" + what;
	if (id.size() > 1 && id[0] == 'm' && digits(1, id.size()))
		return "M" + id.substr(1) + "_" + what;
	if (id.size() > 2 && id.compare(0, 2, "sr") == 0 && digits(2, id.size()))
		return "StreetRace0" + id.substr(2) + "_" + what;
	if (id.size() > 2 && id.compare(0, 2, "bm") == 0 && digits(2, id.size()))
		return "Bonus_" + what;
	return {};
}

void ScriptEngine::ResetToHere()
{
	_resetStage = _stageCursor;
}

bool ScriptEngine::RetryFromCheckpoint()
{
	if (!_missionActive || _stageScripts.empty())
		return false;

	Log::Info("ScriptEngine: retrying '{}' from stage {}/{}", _missionId, _resetStage + 1, _stageScripts.size());
	_game.GetAudioManager().PlayMusicEvent(musicEvent("start"));
	_missionMusicEnded = false;
	if (_hasPlayerStart)
		_game.SetPlayerPosition(_playerStart);
	// Every other car goes back to where it was declared. A stage replayed
	// only re-declares its own: m5's van, declared the stage before the
	// checkpoint, stayed wherever the failed chase had left it, more than the
	// follow distance away, and every retry failed on the spot.
	for (auto& vehicle : _missionVehicles)
	{
		const auto spawn = _vehicleSpawns.find(vehicle->GetName());
		if (vehicle.get() != _playerCar && spawn != _vehicleSpawns.end())
			vehicle->Teleport(spawn->second.first, spawn->second.second);
	}
	if (_playerCar != nullptr && !_playerCarLocator.empty())
	{
		PlacePlayerCar(_playerCar->GetName(), _playerCarLocator);
		if (_playerStartsInCar)
			PutPlayerInCar();
	}
	beginStage(_resetStage);
	return true;
}

std::string ScriptEngine::GetFailureHint() const
{
	return _failureHint.empty() ? std::string() : _game.GetTextBible().Get(_failureHint);
}

std::string ScriptEngine::GetFailureReason() const
{
	// Each hint is filed under its failure: MISSION_FAILED_TIME_OUT_HINT1 is
	// advice on MISSION_FAILED_TIME_OUT.
	const std::size_t hint = _failureHint.rfind("_HINT");
	if (hint == std::string::npos)
		return {};
	return _game.GetTextBible().Get(_failureHint.substr(0, hint));
}

std::string ScriptEngine::GetMissionTitle(bool info) const
{
	// The phrase table numbers each level's missions the same way: the story's
	// mN as N (level 1's tutorial is m0), then the street races 8-10, the wager
	// race 11 and the bonus mission 12.
	static const std::map<std::string, int> kRaces {{"sr1", 8}, {"sr2", 9}, {"sr3", 10}, {"gr1", 11}, {"bm1", 12}};
	int number = -1;
	if (const auto race = kRaces.find(_missionId); race != kRaces.end())
		number = race->second;
	else if (_missionId.size() > 1 && _missionId[0] == 'm' &&
	         std::all_of(_missionId.begin() + 1, _missionId.end(), [](unsigned char c) { return std::isdigit(c); }))
		number = std::atoi(_missionId.c_str() + 1);
	if (number < 0)
		return {};
	return _game.GetTextBible().Get(
	    fmt::format("MISSION_{}_L{}_M{}", info ? "INFO" : "TITLE", _game.GetLevelNumber(), number));
}

std::string ScriptEngine::GetObjectiveText() const
{
	// What the game itself says ("COLLECT LISA'S SCIENCE PROJECT"), when the
	// stage names its line and the phrase table has it; the objective's type
	// otherwise.
	if (_objectiveType.empty())
		return {};
	// Without the game's own line, a plain one in the game's language; nothing
	// for a stage that asks nothing of the player (a conversation, a pause). It
	// read "Objective: dialogue", and then English under a French game.
	static const std::set<std::string> kPlain {"goto",     "getin",    "talkto", "interior", "gooutside",
	                                           "race",     "follow",   "destroy", "dump",    "delivery",
	                                           "losetail", "coins",    "buycar",  "buyskin"};
	std::string text = kPlain.count(_objectiveType)
	                       ? GameText("objective." + _objectiveType, _game.GetLanguage())
	                       : std::string();
	if (_stageMessage >= 0)
	{
		const std::string& phrase = _game.GetTextBible().Get(fmt::format("MISSION_OBJECTIVE_{}", _stageMessage));
		if (!phrase.empty())
			text = phrase;
	}
	if ((_objectiveType == "delivery" || _objectiveType == "dump") && _pickupsTotal > 0)
		text += fmt::format(" {}/{}", _pickupsTotal - _pickups.size() - _dumpQueue.size(), _pickupsTotal);
	if (_objectiveType == "buyskin" && _game.SkinPrice(_objectiveArgument) > 0)
		text += fmt::format(" {}/{}", std::min(_game.GetCoins(), _game.SkinPrice(_objectiveArgument)),
		                    _game.SkinPrice(_objectiveArgument));
	if (_objectiveType == "buycar" && _game.CarPrice(_objectiveArgument) > 0)
		text += fmt::format(" {}/{}", std::min(_game.GetCoins(), _game.CarPrice(_objectiveArgument)),
		                    _game.CarPrice(_objectiveArgument));
	if (_objectiveType == "coins" && _coinFee > 0)
		text += fmt::format(" {}/{}", std::min(_game.GetCoins(), _coinFee), _coinFee);
	if (_objectiveType == "destroy")
		text += fmt::format(" ({:.0f}%)", _targetHealth > 0.0f ? _targetHealth : 0.0f);
	return text;
}

void ScriptEngine::UseElapsedTime()
{
	_clockCountsUp = true;
}

void ScriptEngine::SetParTime(float seconds)
{
	_parTime = seconds;
}

void ScriptEngine::SetCoinFee(int coins)
{
	_coinFee = coins;
}

std::string ScriptEngine::GetObjectiveTargetLabel() const
{
	const char language = _game.GetLanguage();
	if (_objectiveType == "race")
		return GameText("target.checkpoint", language);
	if (_objectiveType == "dump" && _pickups.empty())
		return GameText("target.target", language);
	if (_objectiveType == "delivery" || _objectiveType == "dump")
		return GameText("target.item", language);
	if (_objectiveType == "follow" || _objectiveType == "destroy")
		return GameText("target.target", language);
	if (_objectiveType == "talkto")
		return fmt::format(fmt::runtime(GameText("target.talkto", language)), _talkToTarget);
	if (_objectiveType == "destroyboss")
		return GameText("target.target", language);
	return GameText("target.destination", language);
}

void ScriptEngine::AnnounceStageComplete()
{
	_announceStageComplete = true;
}

void ScriptEngine::CleanupMission()
{
	_game.ClearNpcs();
	_stageScripts.clear();
	_recordingStage = false;
	_resetStage = 0;
	_playerCarLocator.clear();
	_playerStartsInCar = false;
	_npcPositions.clear();
	_raceOpponents.clear();
	_chasers.clear();
	_raceVehicles.clear();
	_raceCatchups.clear();
	_targetCatchups.clear();
	_finishedAhead.clear();
	_racePosition = 1;
	_playerCar = nullptr;
	_racePath.clear();
	_stageWaypoints.clear();

	for (auto& v : _missionVehicles)
		v->DestroyPhysics(_game.GetWorldPhysics());
	_missionVehicles.clear();
	_vehicleSpawns.clear();
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
	_noTraffic = false;
	// A mission given up: the level's music, at once.
	if (!_missionMusicEnded)
	{
		_game.GetAudioManager().SetMusicBase("OF_explore_mission");
		_game.GetAudioManager().ResumeMusicBase();
		_missionMusicEnded = true;
	}

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
	// A stage's transition: the script waits while the screen closes, the
	// next stage begins when it is shut, and it opens again in half a second.
	if (_transition != Transition::None)
	{
		_transitionElapsed += static_cast<float>(dt);
		if (!_transitionOpening && _transitionElapsed >= _transitionSeconds)
		{
			_transitionOpening = true;
			_transitionElapsed = 0.0f;
			_transitionSeconds = 0.5f;
			nextStage();
		}
		else if (_transitionOpening && _transitionElapsed >= _transitionSeconds)
			_transition = Transition::None;
		if (!_transitionOpening)
			return;
	}

	// The stage clock starts when the countdown has run out.
	if (_countdownElapsed < countdownLength())
	{
		_countdownElapsed += static_cast<float>(dt);
		return;
	}

	if (!_missionActive)
		return;

	if (_game.GetState() == GameState::InGame)
		_missionElapsed += static_cast<float>(dt);
	updateObjective(dt);
	updateConditions(dt);

	if (_stageTimeRemaining <= 0.0f)
		return;

	if (_chaseManager)
	{
		Vector3 playerPos = _game.GetPlayerPosition();
		_chaseManager->Update(dt, playerPos);
		if (_chaseManager->IsBusted())
		{
			missionFailed("busted", "MISSION_FAILED_HIT_N_RUN_HINT1");
			return;
		}
	}

	if (_clockCountsUp)
	{
		_stageTimeRemaining += static_cast<float>(dt);
		if (_parTime > 0.0f && _stageTimeRemaining > _parTime)
			missionFailed(fmt::format("over the par time of {:.0f}s", _parTime), "MISSION_FAILED_TIME_OUT_HINT1");
		return;
	}

	// The last ten seconds have music of their own (level 1 m4's).
	const bool tenSeconds = _stageTimeRemaining > 10.0f;
	_stageTimeRemaining -= static_cast<float>(dt);
	if (tenSeconds && _stageTimeRemaining <= 10.0f && _stageTimeRemaining > 0.0f)
		_game.GetAudioManager().PlayMusicEvent(musicEvent("10sec_to_go"), true);
	if (_stageTimeRemaining <= 0.0f)
	{
		missionFailed("stage timer expired", "MISSION_FAILED_TIME_OUT_HINT1");
	}
}

void ScriptEngine::AddCharacter(const std::string& name, const std::string& locator)
{
	Log::Info("ScriptEngine: add character '{}' at '{}'", name, locator);
	// Where to find them for a "talkto". There is no NPC to walk about yet, so
	// they are where the mission puts them.
	const Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
	if (pos != Vector3::Zero)
	{
		_npcPositions[name] = pos;
		if (name != "homer")
			_game.PlaceNpc(name, pos);
	}
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
	// yet, so it is the car the level starts the player in.
	const std::string model = car == "current" ? _game.GetPlayerCarModel() : car;
	Log::Info("ScriptEngine: place player car '{}' ('{}') at '{}' ({:.1f}, {:.1f}, {:.1f})", car, model, locator,
	          pos.X, pos.Y, pos.Z);

	// The same car moved, not a second one built: a mission resets the player
	// into their car, then a stage places it on the grid.
	if (_playerCar != nullptr && _playerCar->GetName() == model)
	{
		float heading = 0.0f;
		const bool faces = _game.GetLevel().GetLocatorHeading(locator, heading);
		_playerCar->Teleport(pos, faces ? Quaternion(Vector3(0.0f, 1.0f, 0.0f), heading) : _playerCar->GetRotation());
		return;
	}
	_playerCar = &spawnVehicle(model, locator, pos);
}

Vehicle& ScriptEngine::SummonPlayerCar(const std::string& model, const Vector3& position)
{
	if (_playerCar != nullptr && _playerCar->GetName() == model)
	{
		_playerCar->Teleport(position, _playerCar->GetRotation());
		return *_playerCar;
	}
	_playerCar = &spawnVehicle(model, "", position);
	return *_playerCar;
}

void ScriptEngine::AddCollectible(const std::string& locator, const std::string& type)
{
	// A "dump"'s items fall out of a vehicle the player rams (m6's ten
	// Bonestorm games): queued here, dropped one a hit in updateObjective.
	if (_objectiveType == "dump")
	{
		_dumpQueue.push_back(locator);
		_pickupsTotal = _dumpQueue.size();
		return;
	}
	if (_objectiveType == "delivery")
	{
		const Vector3 pos = _game.GetLevel().GetLocatorPosition(locator);
		if (pos == Vector3::Zero)
		{
			Log::Warn("ScriptEngine: delivery item '{}' not found", locator);
			return;
		}
		_pickups.emplace_back(locator, pos);
		_pickupsTotal = _pickups.size();
		return;
	}

	if (_objectiveType != "race")
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
	float heading = 0.0f;
	const bool faces = _game.GetLevel().GetLocatorHeading(locator, heading);
	_vehicleSpawns[car] = {pos, faces ? Quaternion(Vector3(0.0f, 1.0f, 0.0f), heading) : Quaternion::Identity};

	// A stage played again (a retry) declares its cars again: move the one that
	// is already there rather than build a second.
	for (auto& existing : _missionVehicles)
	{
		if (existing->GetName() != car)
			continue;
		existing->Teleport(pos, faces ? Quaternion(Vector3(0.0f, 1.0f, 0.0f), heading) : existing->GetRotation());
		_activeVehicle = existing.get();
		return *existing;
	}

	auto vehicle = std::make_unique<Vehicle>(car);
	std::string modelPath = "art/cars/" + car + ".p3d";
	if (!FileSystem::exists(modelPath))
		modelPath = "art/cars/famil_v.p3d";
	vehicle->LoadModel(modelPath);
	applyCarScript(*vehicle);
	vehicle->CreatePhysicsBody(_game.GetWorldPhysics(), pos);

	// Every car used to spawn facing +Z whatever its locator said, so sr2's grid
	// sat across the street it had to start down.
	if (faces)
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

	// Its driver, the stage's or the car's own (SetDriver("skinner") in
	// scripts/cars/<car>.con); "none" for an empty seat.
	std::string seated = driver;
	if (seated.empty())
	{
		std::ifstream script("scripts/cars/" + car + ".con");
		for (std::string line; std::getline(script, line);)
			if (const std::size_t at = line.find("SetDriver(\""); at != std::string::npos)
			{
				const std::size_t open = at + 11;
				seated = line.substr(open, line.find('"', open) - open);
				break;
			}
	}
	_vehicleDrivers[&vehicle] = seated == "none" ? std::string() : seated;

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
std::string ScriptEngine::GetVehicleDriver(const Vehicle* vehicle) const
{
	const auto found = _vehicleDrivers.find(vehicle);
	return found != _vehicleDrivers.end() ? found->second : std::string();
}

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
void ScriptEngine::SetCarSetting(const std::string& name, float value) { if (_activeVehicle) _activeVehicle->SetSetting(name, value); }
void ScriptEngine::SetCarDriver(const std::string& driver) { if (_activeVehicle) _activeVehicle->SetDefaultDriver(driver); }

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

	// A "follow" drives its target the same way: m5's van runs its waypoints
	// and the player has to keep up.
	if (_objectiveType != "race" && _objectiveType != "follow" && _objectiveType != "destroy" &&
	    _objectiveType != "dump")
		return;
	for (Vehicle* vehicle : _raceVehicles)
	{
		auto opponent = std::make_unique<RaceOpponent>(*vehicle, _racePath, raceIsOpen());
		const auto catchup = _raceCatchups.find(vehicle->GetName());
		if (catchup != _raceCatchups.end())
			opponent->SetCatchup(catchup->second);
		// A car to be chased, not raced, waits for the player.
		const auto target = _targetCatchups.find(vehicle->GetName());
		if (target != _targetCatchups.end() && _objectiveType != "race")
			opponent->SetTargetCatchup(target->second.first, target->second.second);
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
	_pedGroup = group;
}

void ScriptEngine::SetMaxTraffic(int cars)
{
	Log::Info("ScriptEngine: at most {} traffic cars", cars);
	_maxTraffic = std::max(0, cars);
}

void ScriptEngine::SetHUDIcon(const std::string& icon)
{
	_hudIcon = icon;
}

void ScriptEngine::AddObjective(const std::string& type, const std::string& argument)
{
	_objectiveType = type;
	_objectiveArgument = argument;
	_targetHealth = 100.0f;
	_lastTargetGap = -1.0f;
	_rammingCooldown = 0.0f;
	_pickups.clear();
	_dumpQueue.clear();
	_pickupsTotal = 0;
	_hasDestination = false;
	_talkToTarget.clear();
	_objectiveElapsed = 0.0f;
	_fmvFile.clear();
	_fmvStopMusic = _fmvStarted = false;
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
	_noTraffic = true;
	Log::Info("ScriptEngine: no traffic for this stage");
}

void ScriptEngine::OnInteriorChanged(const std::string& interior)
{
	_playerInterior = interior;
	if (_objectiveType == "interior" && !interior.empty() && interior == _interiorTarget)
		objectiveComplete();
}

void ScriptEngine::SetRaceLaps(int laps)
{
	if (laps < 1)
	{
		Log::Warn("ScriptEngine: SetRaceLaps({}) ignored, keeping {}", laps, _totalLaps);
		return;
	}
	_totalLaps = laps;
	Log::Info("ScriptEngine: race is {} lap(s)", laps);
}

void ScriptEngine::SetConversationCam(int line, const std::string& shot, const std::string& mission)
{
	if (mission.empty())
		_conversationCams[line] = shot;
	else
		_missionConversationCams[mission][line] = shot;
}

void ScriptEngine::SetCamBestSide(const std::string& locator, const std::string& mission)
{
	if (mission.empty())
		_bestSide = locator;
	else
		_missionBestSides[mission] = locator;
}

bool ScriptEngine::GetConversationShot(ConversationShot& shot) const
{
	if (!_missionActive || _objectiveType != "dialogue" || !_conversationQueued || _conversation.empty())
		return false;
	auto& audio = _game.GetAudioManager();
	const int line = audio.GetDialogueLinesStarted() - _conversationLineBase - 1;
	if (line < 0 || !audio.IsDialoguePlaying())
		return false;
	// The stage's own, or the level script's for this mission.
	const std::map<int, std::string>* cams = &_conversationCams;
	if (cams->empty())
	{
		const auto mission = _missionConversationCams.find(_missionId);
		if (mission == _missionConversationCams.end())
			return false;
		cams = &mission->second;
	}
	const auto found = cams->find(line);
	if (found == cams->end())
		return false;
	shot.shot = found->second;
	// The player's character is one of the two; the other is the one to frame.
	const std::string& player = _game.GetPlayerCharacterName();
	shot.other = _dialogueCharacters.first == player ? _dialogueCharacters.second : _dialogueCharacters.first;
	std::string side = _bestSide;
	if (side.empty())
		if (const auto mission = _missionBestSides.find(_missionId); mission != _missionBestSides.end())
			side = mission->second;
	shot.hasBestSide = !side.empty() && _game.GetLevel().HasLocator(side);
	if (shot.hasBestSide)
		shot.bestSide = _game.GetLevel().GetLocatorPosition(side);
	return true;
}

void ScriptEngine::SetConditionPosition(int position)
{
	_racePosition = std::max(1, position);
	Log::Info("ScriptEngine: the race is won in the first {} place(s)", _racePosition);
}

void ScriptEngine::SetVehicleAIParams(const std::string& vehicle, int a, int b)
{
	Log::Info("ScriptEngine: AI params for '{}': {}, {} (not used)", vehicle, a, b);
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
	// And only for a race: a walk to the next door needs no "GO!".
	static const std::vector<CountdownStep> kDefault {{"GO!", 1.5f}};
	static const std::vector<CountdownStep> kNone;
	if (!_countdown.empty())
		return _countdown;
	return _objectiveType == "race" ? kDefault : kNone;
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
	// Only in a race: m6's truck is a target, not a rival, and "2nd / 2" over a
	// delivery made no sense.
	if (_raceOpponents.empty() || _objectiveType != "race")
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

std::vector<std::string> ScriptEngine::conversationLines() const
{
	// "conversations\c_<name>_<n>_<kind>_<speaker>_l<level><mission>.rsd": a story
	// mission mN is "mN", a bonus mission bmN "bN", a street race srN "rN".
	if (_conversation.empty())
		return {};
	std::string mission = _missionId;
	if (mission.rfind("bm", 0) == 0)
		mission = "b" + mission.substr(2);
	else if (mission.rfind("sr", 0) == 0)
		mission = "r" + mission.substr(2);
	// Not every conversation names its mission: level 1 m1's "icecream" is
	// c_icecream_1_convinit_mrg_l1.rsd, the tutorial's end in "_l1t1". The
	// mission's own lines first; failing those, the level's.
	const std::string tail = fmt::format("_l{}{}.rsd", _game.GetLevelNumber(), mission);
	const std::string levelTail = fmt::format("_l{}", _game.GetLevelNumber());
	std::vector<std::pair<int, std::string>> numbered, levelWide;
	for (const auto& name : _game.GetAudioManager().FindSounds("\\c_" + _conversation + "_"))
	{
		const std::size_t at = name.find("\\c_" + _conversation + "_") + 4 + _conversation.size();
		const std::pair<int, std::string> line {std::atoi(name.c_str() + at), name};
		if (name.size() >= tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0)
			numbered.push_back(line);
		else if (const std::size_t last = name.rfind('_'); last != std::string::npos &&
		         name.compare(last, levelTail.size(), levelTail) == 0 &&
		         (last + levelTail.size() == name.size() - 4 || !std::isdigit(static_cast<unsigned char>(name[last + levelTail.size()]))))
			levelWide.push_back(line);
	}
	if (numbered.empty())
		numbered = std::move(levelWide);
	std::sort(numbered.begin(), numbered.end());
	std::vector<std::string> lines;
	for (auto& [n, name] : numbered)
		lines.push_back(std::move(name));
	return lines;
}

const Vehicle* ScriptEngine::chasingTarget() const
{
	for (const auto& chaser : _chasers)
		if (chaser.vehicle->GetName() == _objectiveTarget)
			return chaser.vehicle;
	return _chasers.empty() ? nullptr : _chasers.front().vehicle;
}

void ScriptEngine::updateChasers(double dt)
{
	const Vector3 player = _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();
	for (auto& chaser : _chasers)
	{
		if (_game.GetPlayerVehicle() == chaser.vehicle)
			continue;
		chaser.sinceRoute += dt;
		// A new route when the player has moved on from where the last one
		// ended, at most every two seconds: each one starts the driver afresh.
		const bool stale = (player - chaser.routedTo).Length() > 20.0f && chaser.sinceRoute > 2.0;
		if (!chaser.driver || stale)
		{
			std::vector<Vector3> route = _game.GetPathGraph().FindRoute(chaser.vehicle->GetPosition(), player);
			if (route.empty())
				route.push_back(chaser.vehicle->GetPosition());
			route.push_back(player);
			if (!chaser.driver)
				chaser.driver = std::make_unique<RaceOpponent>(*chaser.vehicle, std::move(route), true);
			else
				chaser.driver->SetCircuit(std::move(route));
			chaser.routedTo = player;
			chaser.sinceRoute = 0.0;
		}
		chaser.driver->Update(dt, -1.0f);
	}
}

void ScriptEngine::UpdateAI(double dt)
{
	// The stage is done while the screen closes: its cars decide nothing (a
	// car followed to its end would else win a race with no one).
	if (_transition != Transition::None && !_transitionOpening)
		return;
	if (!_chasers.empty() && !IsRaceHeld())
		updateChasers(dt);
	if (_raceOpponents.empty())
		return;

	if (IsRaceHeld())
	{
		for (auto& opponent : _raceOpponents)
			opponent->Hold();
		return;
	}

	const float playerLaps = playerRaceLaps();
	const Vector3 player =
	    _game.GetPlayerVehicle() ? _game.GetPlayerVehicle()->GetPosition() : _game.GetPlayerPosition();

	for (auto& opponent : _raceOpponents)
	{
		opponent->SetPlayerPosition(player);
		// If the player has taken this opponent's car, stop driving it: two
		// inputs on one vehicle in the same frame just fight each other.
		if (_game.GetPlayerVehicle() == &opponent->GetVehicle())
			continue;

		opponent->Update(dt, playerLaps);

		// Decided already: the car still at the end of its route must not win or
		// lose it again every frame of the results screen (level 5 m4 "completed"
		// 800 times over).
		if (_raceDecided)
			continue;

		// A race is lost when an opponent finishes it first. Without this the
		// only way to lose was the stage timer, and a player who was simply
		// beaten kept racing an opponent that had long since gone round again.
		// The car being followed has got where it was going: that is the follow
		// done, not a race lost.
		if (_objectiveType == "follow" && opponent->Laps() >= _totalLaps)
		{
			// Followed all the way, or never caught up with at all.
			const bool distanceKept = std::any_of(_conditions.begin(), _conditions.end(),
			                                      [](const auto& c) { return c.type == "followdistance"; });
			if (distanceKept && !_followInRange)
				missionFailed("never caught up with the car to follow", "MISSION_FAILED_FOLLOW_DISTANCE_HINT1");
			else
				objectiveComplete();
			return;
		}

		// A car to be destroyed that reaches the end of its route has got away.
		if (_objectiveType == "destroy" && opponent->Laps() >= _totalLaps)
		{
			missionFailed(fmt::format("'{}' got away", opponent->GetVehicle().GetName()), "MISSION_FAILED_RACE_HINT1");
			return;
		}

		// Lost when as many opponents have finished as the place the race asks
		// for (SetConditionPosition): the first to, for first place.
		if (!_raceDecided && opponent->Laps() >= _totalLaps && _finishedAhead.insert(opponent.get()).second &&
		    static_cast<int>(_finishedAhead.size()) >= _racePosition)
		{
			missionFailed(fmt::format("'{}' finished all {} laps, {} ahead of the player", opponent->GetVehicle().GetName(),
			                          _totalLaps, _finishedAhead.size()),
			              "MISSION_FAILED_POSITION_HINT1");
		}
	}

	// Keep the legacy readouts pointing at a real car so anything still asking
	// for the AI transform (debug draw, HUD) follows the leading opponent's.
	_aiPosition = _raceOpponents.front()->GetVehicle().GetPosition();
	_aiRotation = _raceOpponents.front()->GetVehicle().GetRotation();
}

std::string ScriptEngine::bestTimeKey() const
{
	return fmt::format("L{}:{}", _game.GetLevelNumber(), _missionId);
}

std::map<std::string, float> ScriptEngine::loadBestTimes() const
{
	// The save's "time:<key>" values ("time:L4:m1sd" 3.00291).
	std::map<std::string, float> times;
	for (const auto& [key, value] : _game.GetSave().GetAll("time:"))
		if (const float seconds = static_cast<float>(std::atof(value.c_str())); seconds > 0.0f)
			times[key] = seconds;
	return times;
}

void ScriptEngine::saveBestTimes(const std::map<std::string, float>& times) const
{
	auto& save = _game.GetSave();
	for (const auto& [key, value] : save.GetAll("time:"))
		save.Erase("time:" + key);
	for (const auto& [key, seconds] : times)
		save.Set("time:" + key, fmt::format("{}", seconds));
	save.Save();
}

void ScriptEngine::ResetBestTime()
{
	// This mission's record only; the others are kept.
	auto times = loadBestTimes();
	times.erase(bestTimeKey());
	saveBestTimes(times);
	_bestTime = 999.0f;
	Log::Info("ScriptEngine: best time for {} reset", bestTimeKey());
}

void ScriptEngine::SaveGameState()
{
	if (!_missionId.empty() && _bestTime < 999.0f)
	{
		auto times = loadBestTimes();
		times[bestTimeKey()] = _bestTime;
		saveBestTimes(times);
	}
	Log::Info("ScriptEngine: game saved");
}

void ScriptEngine::LoadGameState()
{
	const auto times = loadBestTimes();
	const auto best = times.find(bestTimeKey());
	_bestTime = best != times.end() ? best->second : 999.0f;
	Log::Info("ScriptEngine: loaded {} best times", times.size());
}

void ScriptEngine::AdvanceCheckpoint()
{
	// An interior's door is only a marker: the objective is met by going in
	// (OnInteriorChanged), on foot. Reaching it used to complete it from the
	// car, and the stage after -- talking to Lisa, inside -- was out of reach.
	// Only a door with nothing behind it completes on arrival.
	if (_objectiveType == "interior" && _game.CanEnterInterior(_interiorTarget))
		return;

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
				objectiveComplete();
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

void ScriptEngine::missionComplete()
{
	// The player's character is pleased with themselves.
	_game.SayLine("w_mvic");
	_raceDecided = true;
	// Measured, not derived from the stage clock: gr1's counts up from 1s, and
	// the old "initial minus remaining" came out at -8.6s -- a new record, saved.
	const float elapsed = _missionElapsed;
	_isNewRecord = false;
	if (_recordsEnabled && elapsed > 0.0f && elapsed < _bestTime)
	{
		_bestTime = elapsed;
		_isNewRecord = true;
		auto times = loadBestTimes();
		times[bestTimeKey()] = _bestTime;
		saveBestTimes(times);
		_game.GetAudioManager().PlayRaw(SoundGenerator::Chirp(600, 1200, 0.5f), 22050, 1, 16);
	}

	// A bonus mission or street race won is kept: the scrapbook counts them.
	if (!_game.IsAutopilot() && (_missionId.compare(0, 2, "bm") == 0 || _missionId.compare(0, 2, "sr") == 0))
	{
		_game.GetSave().Add("won", fmt::format("L{}:{}", _game.GetLevelNumber(), _missionId));
		_game.GetSave().Save();
	}
	Log::Info("ScriptEngine: stage complete! Time: {:.1f}s (best: {}){}", elapsed,
	          _bestTime < 999.0f ? fmt::format("{:.1f}s", _bestTime) : "none", _isNewRecord ? " NEW RECORD!" : "");
	// The mission's ending, then the level's music. A drive to a mission has
	// none: the mission's own takes over.
	auto& audio = _game.GetAudioManager();
	audio.SetMusicBase("OF_explore_mission");
	if (!audio.PlayMusicEvent(musicEvent("win"), true))
		audio.ResumeMusicBase();
	_missionMusicEnded = true;
	_stageTimeRemaining = -1.0f;
	_game.Fire(GameEvent::MissionWon);
}

void ScriptEngine::SetPresentationBitmap(const std::string& path)
{
	Log::Info("ScriptEngine: presentation bitmap = '{}'", path);
	_presentationBitmap = path;
	std::replace(_presentationBitmap.begin(), _presentationBitmap.end(), '\\', '/');
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
	if (!FileSystem::exists(path))
	{
		Log::Warn("Script not found: {}", path);
		return;
	}

	// Statement by statement, as Commands::RunScript splits them, so each can be
	// routed: a stage's are kept for when the stage begins, the rest run now.
	File file;
	file.Open(path, FileMode::Read);
	while (file.Position() < file.Size())
	{
		const std::string line = file.ReadLine();
		std::size_t pos = 0;
		while (pos < line.size())
		{
			const std::size_t end = line.find(");", pos);
			const std::string statement = end == std::string::npos ? line.substr(pos) : line.substr(pos, end - pos + 2);
			pos = end == std::string::npos ? line.size() : end + 2;

			std::string name = statement.substr(0, statement.find('('));
			name.erase(0, name.find_first_not_of(" \t"));
			name.erase(name.find_last_not_of(" \t") + 1);

			if (name == "AddStage")
			{
				_stageScripts.push_back({statement});
				_recordingStage = true;
			}
			else if (_recordingStage)
			{
				_stageScripts.back().push_back(statement);
				if (name == "CloseStage")
					_recordingStage = false;
			}
			else
			{
				Commands::RunLine(statement);
			}
		}
	}
	file.Close();
}

} // namespace Donut
