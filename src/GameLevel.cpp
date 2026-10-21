// Copyright 2019-2020 the donut authors. See AUTHORS.md

// Game's levels, story and interiors: loading a level, starting its missions,
// going into a building and out.

#include "Game.h"

#include "Audio/AudioManager.h"
#include "Character.h"
#include "Render/SkidMarks.h"
#include "UI/GameHud.h"
#include "Game/GameCamera.h"
#include <fstream>
#include "FrontendProject.h"
#include "Game/CollectibleManager.h"
#include "AI/PathGraph.h"
#include "Game/HitAndRun.h"
#include "AI/PedestrianManager.h"
#include "AI/TrafficManager.h"
#include "P3D/P3DFile.h"
#include "Render/CompositeModel.h"
#include "Game/UfoBoss.h"
#include "Game/WaspManager.h"
#include "Render/imgui/imgui.h"
#include "Scripting/Commands.h"
#include "Scripting/ScriptEngine.h"
#include "Window.h"

namespace Donut
{

namespace
{
// How far from an interior's way out the player is no longer in it: farther
// than any room reaches.
constexpr float kInteriorLeftBehind = 80.0f;
} // namespace

void Game::runMissionScript()
{
	_missionLoadedAt = _elapsedSeconds;
	// Every mission starts with a sound car.
	_health = 100.0f;
	// A mission starts outdoors, wherever the last one left the player.
	_interior.clear();
	if (_level)
		_level->SetDrawOnly({});

	// The game splits each mission in two: <id>l.mfk loads its files and cars,
	// <id>i.mfk sets it up. Given the set-up script, run the loader first.
	const std::string suffix = "i.mfk";
	if (_missionScript.size() > suffix.size() &&
	    _missionScript.compare(_missionScript.size() - suffix.size(), suffix.size(), suffix) == 0)
	{
		const std::string loader = _missionScript.substr(0, _missionScript.size() - suffix.size()) + "l.mfk";
		if (FileSystem::exists(loader))
			_scriptEngine->RunFile(loader);
	}

	if (!FileSystem::exists(_missionScript))
	{
		Log::Warn("Game: mission script '{}' not found", _missionScript);
		return;
	}
	_scriptEngine->RunFile(_missionScript);
}

void Game::startStoryMission(int index)
{
	_storyIndex = index;
	_missionScript = _story[index];
	Log::Info("Game: story mission {}/{}: {}", index + 1, _story.size(), _missionScript);

	if (_inVehicle)
	{
		_inVehicle = false;
		_activeVehicle = nullptr;
	}
	_scriptEngine->CleanupMission();
	if (_collectibleManager) _collectibleManager->SpawnOnPath();
	runMissionScript();

	// The mission placed the player; this runs once the character exists.
	Vector3 start;
	if (_scriptEngine->GetPlayerStart(start))
		SetPlayerPosition(start);
}

Game::StoryProgress Game::loadStoryProgress() const
{
	// "<mission> <level>"; a file written before there were levels holds only
	// the mission, of level 1.
	std::istringstream file(_save.Get("progress"));
	StoryProgress progress;
	if (!(file >> progress.mission) || progress.mission < 0)
		return {};
	if (!(file >> progress.level) || progress.level < 1)
		progress.level = 1;
	return progress;
}

void Game::saveStoryProgress(int level, int next) const
{
	// An unattended run proves the missions play through; it has not won them.
	if (_autopilot)
		return;
	auto& save = const_cast<SaveGame&>(_save);
	save.Set("progress", fmt::format("{} {}", next, level));
	save.Save();
	Log::Info("Game: story progress saved, level {} mission {}", level, next + 1);
}

bool Game::levelAvailable(int level) const
{
	return FileSystem::exists(fmt::format("scripts/level{:02}/level.mfk", level));
}

void Game::showLoadingScreen(int level)
{
	if (!_loadingPage || !_window)
		return;
	// The level's picture, in the place the page leaves blank for it
	// (dynaload/images/loading<n>.p3d: a sprite in tiles).
	const std::string picture = fmt::format("./art/frontend/dynaload/images/loading{}.p3d", level);
	const std::string pictureName = fmt::format("loading{}.png", level);
	if (!_resourceManager->GetTexture(pictureName) && FileSystem::exists(picture))
	{
		const P3D::P3DFile file(picture);
		for (const auto& chunk : file.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::Sprite))
				_resourceManager->LoadTexture(*P3D::Sprite::Load(*chunk));
	}
	if (Texture* texture = _resourceManager->GetTexture(pictureName))
		_loadingPage->SetSpriteTexture("Loading", texture);
	else
		_loadingPage->SetSpriteVisible("Loading", false);
	// One frame of it, drawn before the load holds everything up.
	int width = 0, height = 0;
	SDL_GL_GetDrawableSize(*_window, &width, &height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_loadingPage->Draw(Matrix4x4::MakeOrtho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
	_window->Swap();
	glEnable(GL_DEPTH_TEST);
}

void Game::switchLevel(int level)
{
	Log::Info("Game: leaving level {} for level {}", _levelNumber, level);
	showLoadingScreen(level);
	unloadLevel();
	_levelNumber = level;
	_storyIndex = -1;
	loadLevel();
	LoadModel(_playerCharacter, _playerCharacter);
	if (!_wornSkin.empty())
		BuySkin(_wornSkin);
	_gameCamera->Snap();
	_mini.trackLoaded = false;
}

void Game::unloadLevel()
{
	// Everything that lives in the physics world goes before it: the mission's
	// cars and drivers, the NPCs, the pedestrians and the player.
	_scriptEngine->CleanupMission();
	_inVehicle = false;
	_activeVehicle = nullptr;
	_interior.clear();
	ClearNpcs();
	_ambientNpcs.clear();
	_bonusGivers.clear();
	_carDrivers.clear();
	_hitAndRun.reset();
	_pedestrianManager.reset();
	_trafficManager.reset();
	_collectibleManager.reset();
	_waspManager.reset();
	_ufo.reset();
	_sparks.reset();
	_dust.reset();
	_wheelSprays.clear();
	_dashDust.reset();
	_carSmoke.reset();
	_carSmokeVehicle = nullptr;
	if (_audioManager)
	{
		_audioManager->SetLoop("skid", "");
		_audioManager->SetLoop("siren", "");
	}
	if (_skidMarks)
		_skidMarks->Clear();
	_pathGraph.reset();
	_character.reset();
	_footstepScript.clear();
	_level.reset();
	_worldPhysics = std::make_unique<WorldPhysics>(_lineRenderer.get());
}

void Game::loadLevel()
{
	_level = std::make_unique<Level>();
	// The level's world: its terrain, every zone and road file, and its
	// interiors, which live under the map and are reached through doors. The
	// game streams these in and out as the player moves; they are all loaded
	// here. This was level 1's list, written out.
	for (const std::string& file : levelWorldFiles())
		_level->LoadP3D(file);
	// The radar's map of the level.
	if (_gameHud)
		_gameHud->LoadMap(_levelNumber);
	// The mission art shared by the whole level: the coin, among others.
	_level->LoadP3D(fmt::format("missions/{}/level.p3d", levelDirectory()));
	// The level's effects file (art/l01_fx.p3d) holds the textures its particle
	// effects draw with -- the tire fire's smoke3.bmp -- and was never read: the
	// smoke showed as squares of whatever texture a shader falls back on. Its
	// textures only; its shaders could take the names of the world's.
	if (const std::string fx = fmt::format("art/l{:02}_fx.p3d", _levelNumber); FileSystem::exists(fx))
	{
		const P3D::P3DFile effects(fx);
		for (const auto& chunk : effects.GetRoot().GetChildren())
			if (chunk->GetType() == P3D::ChunkType::Texture)
				_resourceManager->LoadTexture(*P3D::Texture::Load(*chunk));
		// And the shaders the world does not have already (spark3_m, smoke2_m).
		for (const auto& chunk : effects.GetRoot().GetChildren())
			if (chunk->GetType() == P3D::ChunkType::Shader)
				if (auto shader = P3D::Shader::Load(*chunk); !_resourceManager->HasShader(shader->GetName()))
					_resourceManager->LoadShader(*shader);
		_level->LoadHitParticles(effects.GetRoot());
	}
	_sparkFactory = ParticleFactory::Burst("spark3_m", 0.5f, 6.0f, 1.2f, 0.4f, 9.8f, Vector4(1.0f, 0.8f, 0.4f, 1.0f));
	_dustFactory = ParticleFactory::Burst("smoke2_m", 1.2f, 1.0f, 1.4f, 1.2f, 0.0f, Vector4(0.85f, 0.8f, 0.7f, 0.8f));
	_sparks = std::make_unique<ParticleSystem>(*_sparkFactory);
	_dust = std::make_unique<ParticleSystem>(*_dustFactory);
	_skidMarks = std::make_unique<SkidMarks>();
	_pathGraph = std::make_unique<PathGraph>(*_level, *_worldPhysics);
	// Fallen off the map: well below the lowest road. A fixed -50 suited level
	// 1, but level 3's lower town lies at -68, and the player standing there
	// was "respawned" over and over.
	{
		float lowest = 0.0f;
		for (const auto& node : _pathGraph->GetNodes())
			lowest = std::min(lowest, node.position.Y);
		_fallOffY = std::min(-50.0f, lowest - 40.0f);
		Log::Info("Game: level {} falls off below y = {:.0f}", _levelNumber, _fallOffY);
	}
	_trafficManager = std::make_unique<TrafficManager>(*_level, *_lineRenderer, *_pathGraph);
	_trafficManager->LoadModels(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
	_pedestrianManager = std::make_unique<PedestrianManager>(*_worldPhysics);
	_pedestrianManager->LoadModels(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
	_collectibleManager = std::make_unique<CollectibleManager>(*_level, *_lineRenderer);
	// The wasp cameras, at the spawn points the level script lists, their
	// locators in the mission art.
	_level->LoadP3D(fmt::format("missions/{}/wasps.p3d", levelDirectory()));
	_waspManager = std::make_unique<WaspManager>(_levelNumber, fmt::format("scripts/{}/leveli.mfk", levelDirectory()),
	                                             *_level);
	_ufo = UfoBoss::FromLevelScript(fmt::format("scripts/{}/leveli.mfk", levelDirectory()), *_level, *_worldPhysics);
	_collectibleManager->SpawnOnPath();

	bindWorkedObjects();

	// The tutorial: EnableTutorialMode(1) in the level's script (level 1).
	_tutorialMode = false;
	_tutorialWaiting.clear();
	_tutorialSeen = {};
	{
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("EnableTutorialMode(");
			const std::size_t comment = line.find("//");
			if (at == std::string::npos || (comment != std::string::npos && comment < at))
				continue;
			const std::string argument = line.substr(at + 19, line.find(')', at) - at - 19);
			_tutorialMode = argument.find('1') != std::string::npos || argument.find("true") != std::string::npos;
		}
	}

	// The level's ambient people: AddAmbientCharacter("ralph", "ambient_ralph", 1.3).
	{
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("AddAmbientCharacter(");
			const std::size_t comment = line.find("//");
			if (at == std::string::npos || (comment != std::string::npos && comment < at))
				continue;
			std::vector<std::string> args;
			for (std::size_t q = line.find('"', at); q != std::string::npos && args.size() < 2; q = line.find('"', q + 1))
			{
				const std::size_t end = line.find('"', q + 1);
				if (end == std::string::npos)
					break;
				args.push_back(line.substr(q + 1, end - q - 1));
				q = end;
			}
			if (args.size() < 2 || _ambientNpcs.count(args[0]))
				continue;
			const Vector3 spot = _level->GetLocatorPosition(args[1]);
			if (spot == Vector3::Zero)
				continue;
			if (auto npc = makeNpc(args[0]))
			{
				npc->SetPosition(spot);
				_ambientNpcs.emplace(args[0], std::move(npc));
			}
		}
		Log::Info("Game: {} people about level {}", _ambientNpcs.size(), _levelNumber);
	}

	// The car sellers: AddPurchaseCarReward("gil", "gil", "npd", "gil_loc",
	// 1.3, "gil_car"): the shop, who keeps it, where they stand, where the car
	// bought is left. Stood among the level's people.
	_carSellers.clear();
	{
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("AddPurchaseCarReward(");
			const std::size_t comment = line.find("//");
			if (at == std::string::npos || (comment != std::string::npos && comment < at))
				continue;
			std::vector<std::string> args;
			for (std::size_t q = line.find('"', at); q != std::string::npos; q = line.find('"', q + 1))
			{
				const std::size_t end = line.find('"', q + 1);
				if (end == std::string::npos)
					break;
				args.push_back(line.substr(q + 1, end - q - 1));
				q = end;
			}
			if (args.size() < 5)
				continue;
			const Vector3 spot = _level->GetLocatorPosition(args[3]);
			const Vector3 carSpot = _level->GetLocatorPosition(args[4]);
			if (spot == Vector3::Zero || carSpot == Vector3::Zero)
				continue;
			_carSellers.push_back({args[0], spot, carSpot});
			if (!_ambientNpcs.count(args[1]))
				if (auto npc = makeNpc(args[1]))
				{
					npc->SetPosition(spot);
					_ambientNpcs.emplace(args[1], std::move(npc));
				}
		}
		Log::Info("Game: {} car seller(s)", _carSellers.size());
	}

	// Who offers the street races and the bonus mission:
	// AddNPCCharacterBonusMission("milhouse", "npd", "sr1_mhouse_sd", "sr1", ...).
	_bonusGivers.clear();
	_bonusReturn = -1;
	{
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("AddNPCCharacterBonusMission(");
			const std::size_t comment = line.find("//");
			if (at == std::string::npos || (comment != std::string::npos && comment < at))
				continue;
			std::vector<std::string> args;
			for (std::size_t q = line.find('"', at); q != std::string::npos && args.size() < 4; q = line.find('"', q + 1))
			{
				const std::size_t end = line.find('"', q + 1);
				if (end == std::string::npos)
					break;
				args.push_back(line.substr(q + 1, end - q - 1));
				q = end;
			}
			if (args.size() < 4)
				continue;
			const Vector3 spot = _level->GetLocatorPosition(args[2]);
			if (spot == Vector3::Zero ||
			    !FileSystem::exists(fmt::format("scripts/{}/{}i.mfk", levelDirectory(), args[3])))
				continue;
			BonusGiver giver {args[3], spot, makeNpc(args[0])};
			if (giver.npc)
				giver.npc->SetPosition(spot);
			_bonusGivers.push_back(std::move(giver));
		}
		Log::Info("Game: {} street races and bonus missions on offer", _bonusGivers.size());
	}

	// The level's music: what its music script (l<n>_music.rms) plays while
	// exploring. Without one, or with none of it in the archives, the
	// segments of the level's own set play in order instead: Homer's tubas on
	// level 1, Lisa's on 3, Marge's lounge on 4, Halloween on 7, the main
	// themes on the others.
	_audioManager->LoadMusicScript(_levelNumber);
	if (!_audioManager->PlayMusicEvent("OF_explore_mission"))
	{
		static const std::map<int, std::string> kSets {
		    {1, "\\homer\\"}, {3, "\\lisa\\"}, {4, "\\marge\\"}, {7, "\\halloween\\"}};
		const auto set = kSets.find(_levelNumber);
		std::vector<std::string> tracks;
		for (const std::string& track : _audioManager->FindSounds(set != kSets.end() ? set->second : "_main.rsd"))
			if (track.find("\\music\\") != std::string::npos && track.size() > 4 &&
			    track.compare(track.size() - 4, 4, ".rsd") == 0 &&
			    track.find("_end_") == std::string::npos && track.find("reward") == std::string::npos)
				tracks.push_back(track);
		_audioManager->PlayMusicPlaylist(std::move(tracks));
	}

	// Hit & Run: the level's police car (CreateChaseManager) and how fast the
	// meter drains (SetHitAndRunDecay), both from leveli.mfk.
	{
		const auto cops = levelScriptCalls("CreateChaseManager", "leveli.mfk");
		float decay = 1.0f;
		int cars = 1;
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t comment = line.find("//");
			if (const std::size_t at = line.find("SetHitAndRunDecay(");
			    at != std::string::npos && (comment == std::string::npos || comment > at))
				decay = static_cast<float>(std::atof(line.c_str() + at + 18));
			// SetNumChaseCars("2"): how many police cars come.
			if (const std::size_t at = line.find("SetNumChaseCars(");
			    at != std::string::npos && (comment == std::string::npos || comment > at))
			{
				const std::size_t digit = line.find_first_of("0123456789", at);
				if (digit != std::string::npos)
					cars = std::atoi(line.c_str() + digit);
			}
		}
		_hitAndRun = std::make_unique<HitAndRun>(cops.empty() ? "cPolice" : cops.front(), decay, cars);
	}

	// The story is the level script's AddMission list, in order, when the
	// scripts have been copied off the CD. Without them "New Game" plays the
	// test mission, as it always has.
	_story.clear();
	_teleports.clear();
	_playerCharacter = "homer";
	_playerCarModel = "famil_v";
	// A new level, its own cars and clothes: what was bought on the last one
	// stays there, and comes back with it.
	_currentCar.clear();
	_ownedCars.clear();
	_ownedSkins.clear();
	_wornSkin.clear();
	restorePurchases();
	for (const std::string& id : levelScriptCalls("AddMission"))
	{
		// Each mission is reached through its "Sunday drive", <id>sdi.mfk: go
		// home, talk to Marge, buy the car the mission needs... It is played
		// first, as a mission of its own. The story used to jump from one
		// mission straight into the next.
		for (const char* part : {"sd", ""})
		{
			const std::string path = fmt::format("scripts/{}/{}{}i.mfk", levelDirectory(), id, part);
			if (FileSystem::exists(path))
				_story.push_back(path);
		}
	}
	_gags.Load(levelDirectory(), *_level, _save.Entries("gag"));
	// The Warp menu's places, AddTeleportDest("name", x, y, z, "zones").
	{
		std::ifstream script(fmt::format("scripts/{}/level.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("AddTeleportDest(");
			const std::size_t comment = line.find("//");
			if (at == std::string::npos || (comment != std::string::npos && comment < at))
				continue;
			const std::size_t open = line.find('"', at);
			const std::size_t close = open == std::string::npos ? open : line.find('"', open + 1);
			Vector3 place;
			if (close != std::string::npos &&
			    std::sscanf(line.c_str() + close + 1, " , %f , %f , %f", &place.X, &place.Y, &place.Z) == 3)
				_teleports.emplace_back(line.substr(open + 1, close - open - 1), place);
		}
	}
	if (const auto characters = levelScriptCalls("AddCharacter", "leveli.mfk"); !characters.empty())
		_playerCharacter = characters.front();
	// Not every character came off the CD (Lisa, level 3's, did not): Homer
	// stands in rather than nobody.
	if (!FileSystem::exists(fmt::format("art/chars/{}_m.p3d", _playerCharacter)))
	{
		Log::Warn("Game: no model for '{}', Homer plays level {}", _playerCharacter, _levelNumber);
		_playerCharacter = "homer";
	}
	if (const auto cars = levelScriptCalls("InitLevelPlayerVehicle", "leveli.mfk"); !cars.empty())
		_playerCarModel = cars.front();
	Log::Info("Game: level {} is played by '{}' in '{}'", _levelNumber, _playerCharacter, _playerCarModel);
	// Away from level 1 there is no test mission: the level opens on its story.
	if (_levelNumber != 1 && !_launch.missionScriptGiven && !_story.empty())
		_missionScript = _story.front();
}

std::string Game::levelDirectory() const
{
	return fmt::format("level{:02}", _levelNumber);
}

bool Game::levelScriptsName(const std::string& file) const
{
	// Any of the level's scripts loading it (SetDynaLoadData's lists).
	for (const auto& entry : FileSystem::directory_iterator(fmt::format("./scripts/{}", levelDirectory())))
	{
		std::ifstream script(entry.path());
		std::string text((std::istreambuf_iterator<char>(script)), std::istreambuf_iterator<char>());
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
		if (text.find(file) != std::string::npos)
			return true;
	}
	return false;
}

std::vector<std::string> Game::levelWorldFiles() const
{
	// l<n>_TERRA.p3d, then l<n>z*.p3d and l<n>r*.p3d (zones and the roads
	// between them), then the interiors l<n>i*.p3d. Where a road comes in two
	// versions (level 1's l1r4a and l1r4b, the same stretch before and after
	// it changes) only the first is taken: both together overlap -- unless
	// the level's scripts load the second (level 4's street races load l4r4a
	// and l4r4b together: Burns' estate is l4r4b's, and its wasps hung over
	// nothing without it).
	const std::string prefix = fmt::format("l{}", _levelNumber);
	std::string terrain;
	std::vector<std::string> world, interiors;
	for (const auto& entry : FileSystem::directory_iterator("./art"))
	{
		const std::string name = entry.path().filename().string();
		std::string lower = name;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
		if (lower.size() < prefix.size() + 5 || lower.compare(0, prefix.size(), prefix) != 0 ||
		    lower.compare(lower.size() - 4, 4, ".p3d") != 0)
			continue;
		const std::string rest = lower.substr(prefix.size(), lower.size() - prefix.size() - 4);
		if (rest == "_terra")
			terrain = name;
		else if ((rest[0] == 'z' || rest[0] == 'r') && rest.size() > 1 && std::isdigit(static_cast<unsigned char>(rest[1])))
		{
			if (rest.back() == 'b' &&
			    FileSystem::exists(fmt::format("./art/{}a.p3d", name.substr(0, name.size() - 5))) &&
			    !levelScriptsName(lower))
				continue;
			world.push_back(name);
		}
		else if (rest[0] == 'i' && rest.size() == 3 && std::isdigit(static_cast<unsigned char>(rest[1])))
			interiors.push_back(name);
	}
	std::sort(world.begin(), world.end());
	std::sort(interiors.begin(), interiors.end());

	std::vector<std::string> files;
	if (!terrain.empty())
		files.push_back(terrain);
	files.insert(files.end(), world.begin(), world.end());
	files.insert(files.end(), interiors.begin(), interiors.end());
	Log::Info("Game: level {} is {} files", _levelNumber, files.size());
	return files;
}

std::vector<std::string> Game::levelScriptCalls(const std::string& command, const std::string& file) const
{
	std::vector<std::string> arguments;
	std::ifstream script(fmt::format("scripts/{}/{}", levelDirectory(), file));
	for (std::string line; std::getline(script, line);)
	{
		const std::size_t at = line.find(command + "(");
		const std::size_t comment = line.find("//");
		if (at == std::string::npos || (comment != std::string::npos && comment < at))
			continue;
		const std::size_t open = line.find('"', at);
		const std::size_t close = open == std::string::npos ? open : line.find('"', open + 1);
		if (close != std::string::npos)
			arguments.push_back(line.substr(open + 1, close - open - 1));
	}
	return arguments;
}

bool Game::CanEnterInterior(const std::string& name) const
{
	// Which file holds which interior is not in the data as such: the files
	// name their locators after it (int_ele_*, int_kwik_*), and the door's
	// name contains that tag (see Level::FindInteriorFile). This was a table
	// of level 1's three.
	Level::Interior layout;
	const std::string file = _level->FindInteriorFile(name);
	return !file.empty() && _level->GetInterior(file, layout);
}

void Game::enterInterior(const Level::Trigger& door)
{
	// On foot only, as in the game: a car does not go through a front door.
	if (_inVehicle || !_interior.empty() || !_character)
		return;

	// A zone trigger names the interior's file itself, before its '@'; a door
	// names the place, which FindInteriorFile matches to a file. The interior
	// is then known by the trigger's locator ("moe1", which level 2's scripts
	// name as the destination) or by the place ("KwikEMart", level 1's).
	const bool zone = door.type == Level::LocatorType::DynamicZone;
	std::string file;
	if (zone)
	{
		// The file just before the '@', after any others the trigger unloads:
		// "l1z7.p3d:l1z2.p3d:l1i02.p3d@" is l1i02.p3d.
		const std::size_t at = door.data.find('@');
		const std::size_t start = door.data.find_last_of(";:", at);
		file = door.data.substr(start == std::string::npos ? 0 : start + 1, at - (start == std::string::npos ? 0 : start + 1));
	}
	else
		file = _level->FindInteriorFile(door.data);
	if (file.empty() || !_level->GetInterior(file, _interiorLayout))
	{
		Log::Warn("Game: no interior loaded for door '{}' ({})", door.locator, door.data);
		return;
	}

	// Come back out a few metres in front of the door, clear of its trigger, or
	// stepping out would walk straight back in.
	Vector3 away = _character->GetPosition() - door.position;
	away.Y = 0.0f;
	if (away.LengthSquared() < 0.01f)
		away = Vector3(0.0f, 0.0f, 1.0f);
	away.Normalize();
	_interiorReturn = door.position + away * 4.0f;

	_interior = zone ? door.locator : door.data;
	_interiorExitArmed = false;
	// Going indoors loses the police, as the game's hint says.
	if (_hitAndRun)
		_hitAndRun->Reset();
	Log::Info("Game: player goes into '{}'", _interior);
	// The Kwik-E-Mart has its muzak; the other buildings keep the music.
	std::string place = _interior;
	std::transform(place.begin(), place.end(), place.begin(), [](unsigned char c) { return std::tolower(c); });
	_interiorMusic = place.find("kwik") != std::string::npos && _audioManager->PlayMusicEvent("OF_KiwkEMart", true);
	SetPlayerPosition(_interiorLayout.entry);
	// Facing into the room: the camera kept the heading from outside, which
	// for the school looked back out through the door at nothing.
	const Vector3 inward = _interiorLayout.origin - _interiorLayout.entry;
	if (inward.X * inward.X + inward.Z * inward.Z > 0.01f)
		_gameCamera->SetYaw(std::atan2(inward.X, inward.Z));
	_level->SetDrawOnly(file);
	_scriptEngine->OnInteriorChanged(_interior);
}

void Game::updateInterior()
{
	if (_interior.empty() || !_character)
		return;

	const float toExit = (_character->GetPosition() - _interiorLayout.exit).Length();
	// Taken out some other way than its door -- put in a car outside, moved on
	// by the mission: the room's drawing only, kept on, left the town around
	// the player a void of sky.
	if (_inVehicle || toExit > kInteriorLeftBehind)
	{
		Log::Info("Game: player taken out of '{}'", _interior);
		if (_interiorMusic)
			_audioManager->ResumeMusicBase();
		_interiorMusic = false;
		_interior.clear();
		_level->SetDrawOnly({});
		_scriptEngine->OnInteriorChanged(_interior);
		return;
	}
	if (!_interiorExitArmed)
	{
		_interiorExitArmed = toExit > 2.5f;
		return;
	}
	if (toExit > 1.5f)
		return;

	Log::Info("Game: player comes out of '{}'", _interior);
	if (_interiorMusic)
		_audioManager->ResumeMusicBase();
	_interiorMusic = false;
	_interior.clear();
	_level->SetDrawOnly({});
	SetPlayerPosition(_interiorReturn);
	_scriptEngine->OnInteriorChanged(_interior);
}

} // namespace Donut
