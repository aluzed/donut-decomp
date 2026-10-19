// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Game.h"

#include "AnimCamera.h"
#include "Audio/SoundGenerator.h"
#include "Audio/AudioManager.h"
#include "Character.h"
#include "Core/FpsTimer.h"
#include "Core/Math/Math.h"
#include "FreeCamera.h"
#include <cctype>
#include <fstream>
#include "FrontendProject.h"
#include "Game/CollectibleManager.h"
#include "Game/GameInput.h"
#include "Input/Input.h"
#include "Input/Keymap.h"
#include "AI/PathGraph.h"
#include "Game/HitAndRun.h"
#include "AI/PedestrianManager.h"
#include "AI/TrafficManager.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Physics/BulletCast.h"
#include "Physics/WorldPhysics.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"
#include "Render/CompositeModel.h"
#include "Render/SceneSkins.h"
#include "Game/WaspManager.h"
#include "Render/Font.h"
#include "Render/LineRenderer.h"
#include "Render/OpenGL/FrameBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/Shader.h"
#include "Render/SimpleMesh.h"
#include "Render/SkinModel.h"
#include "Render/SpriteBatch.h"
#include "UI/GameMenu.h"
#include "UI/Hud.h"
#include "Render/imgui/imgui.h"
#include "Render/imgui/imgui_impl_opengl3.h"
#include "Render/imgui/imgui_impl_sdl.h"
#include "Render/imgui/imgui_markdown.h"
#include "ResourceManager.h"
#include "Scripting/Commands.h"
#include "Scripting/ScriptEngine.h"
#include "Window.h"

#include <SDL.h>
#include "Core/Log.h"

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <cstdlib>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
// windows.h maps DrawText to DrawTextA, which collides with SpriteBatch::DrawText
#undef DrawText
#endif

namespace Donut
{

Game* Game::instance = nullptr;

#if _DEBUG
const std::string kBuildString = "DEBUG BUILD";
#else
const std::string kBuildString = "Release Build";
#endif

namespace
{
// Human label for the input bound to `action`, preferring a gamepad binding
// when a pad is connected (so the HUD matches the device in use). Reads the
// live keymap so it stays correct after in-game rebinding.
// The 2D projection is MakeOrtho(0, w, h, 0), so text is positioned from its
// top-left corner with y growing downward. Centre a label horizontally.
float centredTextX(const Font* font, const std::string& text, float viewportWidth)
{
	const float width = font != nullptr ? font->MeasureWidth(text) : 0.0f;
	return (viewportWidth - width) * 0.5f;
}

std::string ControlLabel(GameAction action, bool preferGamepad)
{
	const Keymap& km = Input::GetKeymap();
	const auto& binds = BindingsFor(km, action);
	const InputSource* chosen = nullptr;
	for (const auto& src : binds)
	{
		const bool isPad = src.kind == InputSourceKind::GamepadButton || src.kind == InputSourceKind::GamepadAxis;
		if (isPad == preferGamepad)
		{
			chosen = &src;
			break;
		}
	}
	if (!chosen && !binds.empty())
		chosen = &binds.front();
	return chosen ? KeymapConfig::HumanLabel(*chosen) : "—";
}
} // namespace

namespace
{
// Below this speed, holding "back" reverses instead of braking.
constexpr float kReverseThresholdKmh = 2.0f;
} // namespace

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                                const void* userParam)
{
	if (type == GL_DEBUG_TYPE_ERROR)
		Log::Warn("GL CALLBACK: ** GL ERROR ** severity = 0x{:x}, message = {}", severity, message);
}

Game::Game(int argc, char** argv)
{
	instance = this; // global static :D

	// Unattended-run switches. Measuring whether the race opponent gets round the
	// circuit means watching a 120s mission, and every run had to be nursed by
	// hand through the splash screen and then killed. These make a run repeatable:
	//   --autostart          drop straight into the mission, no splash, no menu
	//   --quit-after <secs>  close the window after that much wall time
	//   --mission <script>   run that mission script instead of M1race.con
	//   --autopilot          walk the player through each objective (testing)
	//   --story              play the level's story missions, in order
	//   --level <n>          play level n (1 by default)
	//   --clean              the game alone: no debug windows, lines or skeletons
	//   --from <k>           with --story, start at the level's k-th story mission (from 0)
	//   --language <E|F|G|I|S>  language of the game's own text (English)
	for (int i = 1; i < argc; ++i)
	{
		const std::string_view arg = argv[i];
		if (arg == "--autostart")
			_autoStart = true;
		else if (arg == "--quit-after" && i + 1 < argc)
			_quitAfterSeconds = std::atof(argv[++i]);
		else if (arg == "--mission" && i + 1 < argc)
		{
			_missionScript = argv[++i];
			_missionScriptGiven = true;
		}
		else if (arg == "--autopilot")
			_autopilot = true;
		else if (arg == "--story")
			_startStory = true;
		else if (arg == "--from" && i + 1 < argc)
			_storyFrom = std::max(0, std::atoi(argv[++i]));
		else if (arg == "--clean")
			_clean = true;
		else if (arg == "--level" && i + 1 < argc)
			_levelNumber = std::max(1, std::atoi(argv[++i]));
		else if (arg == "--language" && i + 1 < argc)
			_language = static_cast<char>(std::toupper(static_cast<unsigned char>(argv[++i][0])));
	}

	Commands::RunLine("HelloWorld();");
	// for (const auto& entry : FileSystem::recursive_directory_iterator("scripts"))
	//{
	//	const auto& path = entry.path();
	//	const auto& extension = path.extension().string();
	//	if (extension != ".con" && extension != ".mfk") continue;

	//	Commands::RunScript(path.string());
	//}

	const std::string windowTitle = fmt::format("donut [{0}]", kBuildString);

	const int windowWidth = 1280, windowHeight = 960;
	_window = std::make_unique<Window>(windowTitle, windowWidth, windowHeight);

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);
	glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_FALSE);

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(*_window), static_cast<SDL_GLContext*>(*_window));
	ImGui_ImplOpenGL3_Init("#version 130");

	Input::InitGamepad();

	// const float dpi_scale = 2.0f;
	// ImGuiIO& io = ImGui::GetIO();
	// ImGui::GetStyle().ScaleAllSizes(dpi_scale);
	// io.FontGlobalScale = dpi_scale;

	_lineRenderer = std::make_unique<LineRenderer>(1000000);
	_worldPhysics = std::make_unique<WorldPhysics>(_lineRenderer.get());

	// init sub classes
	_audioManager = std::make_unique<AudioManager>();
	_resourceManager = std::make_unique<ResourceManager>();
	_textBible.Load("art/frontend/scrooby2/resource/txtbible/srr2.txt", _language);
	_scriptEngine = std::make_unique<ScriptEngine>(*this);
	_scriptEngine->SetRecordsEnabled(!_autopilot);

	if (FileSystem::exists("./assets/audio/ambience/ambience.rcf"))
		_audioManager->LoadRCF("./assets/audio/ambience/ambience.rcf");


	if (FileSystem::exists("./art/frontend/scrooby2/resource/fonts/font0_16.p3d"))
	{
		const P3D::P3DFile p3dFont("./art/frontend/scrooby2/resource/fonts/font0_16.p3d");
		_textureFontP3D = P3D::TextureFont::Load(*p3dFont.GetRoot().GetChildren().at(0));

		auto font = std::make_unique<Font>(*_textureFontP3D);
		_resourceManager->AddFont(_textureFontP3D->GetName(), std::move(font));
	}

	// The collector card's and the wrench's models, from the shared art. Only
	// their geometry, shaders and textures: the rest is the pick-up effect's
	// billboards and particles, which nothing here plays.
	for (const char* art : {"./art/cards.p3d", "./art/wrench.p3d"})
	{
		if (!FileSystem::exists(art))
			continue;
		const P3D::P3DFile cards(art);
		for (const auto& chunk : cards.GetRoot().GetChildren())
		{
			switch (chunk->GetType())
			{
			case P3D::ChunkType::Shader: _resourceManager->LoadShader(*P3D::Shader::Load(*chunk)); break;
			case P3D::ChunkType::Texture: _resourceManager->LoadTexture(*P3D::Texture::Load(*chunk)); break;
			case P3D::ChunkType::Geometry:
			{
				const auto geometry = P3D::Geometry::Load(*chunk);
				Log::Info("Game: model '{}' from {}", geometry->GetName(), art);
				_resourceManager->LoadGeometry(*geometry);
				break;
			}
			default: break;
			}
		}
	}

	loadLevel();
	_gameInput = std::make_unique<GameInput>();
	Input::CaptureTextEntry(this, &Game::OnInputTextEntry);


	const auto skinVertSrc = File::ReadAll("shaders/skin.vert");
	const auto skinFragSrc = File::ReadAll("shaders/skin.frag");
	_skinShaderProgram = std::make_unique<GL::ShaderProgram>(skinVertSrc, skinFragSrc);

	const auto meshVertSrc = File::ReadAll("shaders/mesh.vert");
	const auto meshFragSrc = File::ReadAll("shaders/mesh.frag");
	_meshShader = std::make_unique<GL::ShaderProgram>(meshVertSrc, meshFragSrc);

	const auto ppVertSrc = File::ReadAll("shaders/postprocess.vert");
	const auto ppFragSrc = File::ReadAll("shaders/postprocess.frag");
	_postProcessShader = std::make_unique<GL::ShaderProgram>(ppVertSrc, ppFragSrc);

	_playerMesh = SimpleMesh::CreateCapsule(0.3f, 1.8f, Vector4(0.2f, 1.0f, 0.2f, 1.0f), 12);
	_carMesh = SimpleMesh::CreateBox(Vector3(0.9f, 0.5f, 2.2f), Vector4(0.2f, 0.5f, 1.0f, 1.0f));

	runMissionScript();

	loadGlobal();
	LoadModel(_playerCharacter, _playerCharacter);

	// The mission ran before the character existed, so its player placement went
	// nowhere: apply it now. Only a mission that places nobody falls back to the
	// main road. That fallback used to run unconditionally and moved the M1race
	// player 460m away from level1_homer_start.
	Vector3 playerStart;
	if (_scriptEngine->GetPlayerStart(playerStart))
		SetPlayerPosition(playerStart);
	// spawn on the main road (longest path) so the physics ground lines up with
	// the visual ground — the raised grass at the old spawn buried the player
	else if (_level)
	{
		const auto& paths = _level->GetPaths();
		if (!paths.empty())
		{
			size_t bi = 0;
			for (size_t i = 1; i < paths.size(); ++i)
				if (paths[i].points.size() > paths[bi].points.size()) bi = i;
			const auto& pts = paths[bi].points;
			if (!pts.empty())
				SetPlayerPosition(pts[pts.size() / 2] + Vector3(0.0f, 1.0f, 0.0f));
		}
	}

	// load control bindings (writes a default keymap.conf if none exists)
	Input::LoadKeymap("keymap.conf");

	_camera = std::make_unique<FreeCamera>();
	_camera->SetPosition(Vector3(228.0f, 5.0f, -174.0f));
	_camera->SetFOV(70.0f);
	_gameState = _autoStart ? GameState::InGame : GameState::Splash;
	_missionCompleteTimer = 0.0;

	_frontend = std::make_unique<FrontendProject>();
	if (FileSystem::exists("./art/frontend/scrooby/bootup.p3d"))
		_frontend->LoadP3D("./art/frontend/scrooby/bootup.p3d");

	_camera->SetZNear(1.0f);
	_camera->SetZFar(100000.0f);

	if (_startStory && !_story.empty())
		startStoryMission(std::min(_storyFrom, static_cast<int>(_story.size()) - 1));

	if (std::ifstream coins("donut_coins.dat"); !(coins >> _coins) || _coins < 0)
		_coins = 0;

	_mainMenu = std::make_unique<GameMenu>();
	// Picking up the story where it was left, when it was.
	if (const StoryProgress progress = loadStoryProgress();
	    (progress.level > 1 || progress.mission > 0) && levelAvailable(progress.level))
	{
		// Says where: "Continue - LEVEL 2".
		const std::string& level = _textBible.Get(fmt::format("LEVEL_{}", progress.level));
		const std::string label = level.empty() ? "Continue" : "Continue - " + level;
		_mainMenu->AddButton(label, 0, 0, 200, 40, [this, progress]() {
			if (progress.level != _levelNumber)
				switchLevel(progress.level);
			if (progress.mission < static_cast<int>(_story.size()))
				startStoryMission(progress.mission);
			_gameState = GameState::InGame;
		});
	}
	_mainMenu->AddButton("New Game", 0, 0, 200, 40, [this]() {
		// The story starts on level 1, whatever level is loaded.
		if (_levelNumber != 1 && levelAvailable(1))
			switchLevel(1);
		if (!_story.empty())
			startStoryMission(0);
		_gameState = GameState::InGame;
	});
	// Any level the scripts were copied for, from its story's start.
	for (int level = 2; level <= 7; ++level)
	{
		if (!levelAvailable(level))
			continue;
		const std::string& name = _textBible.Get(fmt::format("LEVEL_{}", level));
		_mainMenu->AddButton(name.empty() ? fmt::format("Level {}", level) : name, 0, 0, 200, 40, [this, level]() {
			if (_levelNumber != level)
				switchLevel(level);
			if (!_story.empty())
				startStoryMission(0);
			_gameState = GameState::InGame;
		});
	}
	_mainMenu->AddButton("Quit", 0, 0, 200, 40, [this]() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	});

	_hud = std::make_unique<Hud>();

	_pauseMenu = std::make_unique<GameMenu>();
	_pauseMenu->AddButton("Resume", 0, 0, 200, 40, [this]() {
		_gameState = GameState::InGame;
	});
	_pauseMenu->AddButton("Restart", 0, 0, 200, 40, [this]() {
		_scriptEngine->CleanupMission();
		if (_collectibleManager) _collectibleManager->SpawnOnPath();
		runMissionScript();
		_gameState = GameState::InGame;
		_missionCompleteTimer = 0.0;
	});
	_pauseMenu->AddButton("Quit", 0, 0, 200, 40, [this]() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	});
}

Game::~Game()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// Order matters here, and it was not being kept: members are destroyed in
	// reverse declaration order, which put _worldPhysics before _character, so
	// ~CharacterController reached through a freed world to unregister itself and
	// the process died on every clean exit. Nothing noticed, because a crash on
	// the way out looks like a normal quit -- until the log it was still writing
	// came back truncated.
	//
	// Everything holding a physics reference goes first, the world last.
	_scriptEngine.reset(); // mission vehicles and the race opponent
	_hitAndRun.reset();    // the police car
	_trafficManager.reset(); // its cars' kinematic bodies
	_character.reset();    // its CharacterController unregisters an action
	_npcs.clear();         // NPCs and pedestrians are Characters too
	_ambientNpcs.clear();
	_bonusGivers.clear();
	_pedestrianManager.reset();
	_level.reset();        // static level geometry
	_worldPhysics.reset();

	_window.reset();

	SDL_Quit();
}

void Game::loadGlobal()
{
	if (!FileSystem::exists("art/chars/global.p3d"))
	{
		Log::Warn("Global P3D not found: art/chars/global.p3d");
		return;
	}

	_globalP3D = std::make_unique<P3D::P3DFile>("art/chars/global.p3d");

	const auto& root = _globalP3D->GetRoot();
	for (const auto& chunk : root.GetChildren())
	{
		if (chunk->GetType() != P3D::ChunkType::Texture)
			continue;
		_resourceManager->LoadTexture(*P3D::Texture::Load(*chunk));
	}
}

void Game::LoadModel(const std::string& name, const std::string& anim)
{
	if (_character != nullptr)
		_character.reset();

	_character = std::make_unique<Character>("pc");
	_character->LoadModel(name);
	_character->LoadAnimations(anim);
	_character->SetPosition(Vector3(220, 4.5, -172));
	// Facing +Z, away from the camera that starts behind him: the model faces -Z.
	_character->SetRotation(Quaternion::MakeFromEuler(Vector3(0.0f, 3.14159265f, 0.0f)));

	// "hom_loco_run" is "loco_run": the prefix is the character's.
	_playerAnimations.clear();
	for (const auto& [animation, data] : _character->GetAnimations())
		if (const std::size_t underscore = animation.find('_'); underscore != std::string::npos)
			_playerAnimations[animation.substr(underscore + 1)] = animation;
	_playerOneShot = false;
	_playerIdleFor = 0.0;
	_playerAirborneFor = 0.0;
	playPlayerAnimation("loco_idle_rest", true);
}

Vector3 Game::driverDoor(const Vehicle& car) const
{
	// The car faces +Z; its driver's door is on the side the model puts it, and
	// the driver's seat a little behind the middle (further forward, the climb
	// in ended on the windscreen).
	return car.GetPosition() + car.GetRotation() * Vector3(1.4f * car.GetDriverSide(), 0.0f, -0.5f);
}

void Game::startBoarding(Vehicle* car, bool in)
{
	_boarding = {car, in, -1};
	_playerIdleFor = 0.0;
	updateBoarding();
}

void Game::updateBoarding()
{
	if (!_boarding.car || !_character)
		return;
	// The car may be gone (a mission restarted): the player stays where he is.
	bool exists = _boarding.car == _scriptEngine->GetPlayerCar();
	for (const auto& v : _scriptEngine->GetMissionVehicles())
		exists = exists || v.get() == _boarding.car;
	if (!exists || _inVehicle)
	{
		_boarding = {};
		return;
	}

	static const char* const kIn[] = {"get_into_car_open_door_driver", "get_into_car_driver",
	                                  "get_into_car_close_door_driver"};
	static const char* const kOut[] = {"get_out_of_car_open_door_driver", "get_out_of_car_driver",
	                                   "get_out_of_car_close_door_driver"};
	Vehicle& car = *_boarding.car;

	// At the door, facing the car (the model faces -Z, hence the half turn).
	const Vector3 door = driverDoor(car);
	const Vector3 toCar = car.GetPosition() - door;
	_character->SetPosition(door);
	_character->SetRotation(Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toCar.X, toCar.Z) + 3.14159265f, 0.0f)));

	// The next step once this one has played; a step the character has no
	// animation for is skipped.
	while (_boarding.step < 0 || _character->IsAnimationFinished())
	{
		if (++_boarding.step >= 3)
		{
			car.SetDoorOpen(0.0f);
			if (_boarding.in)
			{
				PutPlayerInVehicle(&car);
				_audioManager->PlayRaw(SoundGenerator::Beep(800, 0.15f), 22050, 1, 16);
			}
			_boarding = {};
			return;
		}
		if (playPlayerAnimation((_boarding.in ? kIn : kOut)[_boarding.step], false))
			break;
	}
	// The door opens with the first step, stands open, and shuts with the last.
	const float progress = _character->GetAnimationProgress();
	car.SetDoorOpen(_boarding.step == 0 ? progress : _boarding.step == 1 ? 1.0f : 1.0f - progress);
}

bool Game::playPlayerAnimation(const std::string& what, bool loop)
{
	const auto found = _playerAnimations.find(what);
	if (!_character || found == _playerAnimations.end())
		return false;
	// Only on a change, or a looped one would restart every frame.
	if (_character->GetAnimName() != found->second || !loop)
		_character->SetAnimation(found->second, loop);
	_playerOneShot = !loop;
	return true;
}

void Game::updatePlayerAnimation(double dt, bool moving, bool onGround)
{
	if (_boarding.car)
		return;
	// The idle ones: Homer scratches, checks his shoes, dances.
	static const char* const kIdles[] = {"idle_scratch", "idle_bumshake", "idle_check_shoes", "idle_dancing",
	                                     "idle_donut",   "idle_fat",      "idle_chili",       "idle_scuzzy"};

	_playerAirborneFor = onGround ? 0.0 : _playerAirborneFor + dt;
	const bool wasFalling = _playerAirborneFor == 0.0 && _character->GetAnimName() == _playerAnimations["jump_idle_in_air"];

	// In the air for more than a moment (a drop off a kerb is not a jump), or
	// off the ground once a jump's take-off has played.
	const bool tookOff = _character->GetAnimName() == _playerAnimations["jump_idle_take_off"] &&
	                     _character->IsAnimationFinished();
	const bool falling = _character->GetAnimName() == _playerAnimations["jump_idle_in_air"];
	// A kick plays through before the fall takes over.
	const bool kicking = _character->GetAnimName() == _playerAnimations["jump_kick"] && !_character->IsAnimationFinished();
	if (kicking && !onGround)
		return;
	if (_playerAirborneFor > 0.25 || (!onGround && (tookOff || falling)))
	{
		_playerIdleFor = 0.0;
		playPlayerAnimation("jump_idle_in_air", true);
		return;
	}
	if (wasFalling)
	{
		playPlayerAnimation("jump_idle_land", false);
		return;
	}
	// A one-shot plays through, unless the player sets off: a landing or an
	// idle is cut short by moving, a victory too.
	if (_playerOneShot && !_character->IsAnimationFinished() && !moving)
		return;
	if (moving)
	{
		_playerIdleFor = 0.0;
		if (!(_playerDashing && playPlayerAnimation("loco_dash", true)) && !playPlayerAnimation("loco_run", true))
			playPlayerAnimation("loco_walk", true);
		return;
	}
	// In a conversation -- a line being spoken, someone within a few metres --
	// the player gestures too.
	bool company = false;
	for (const auto* people : {&_npcs, &_ambientNpcs})
		for (const auto& [name, npc] : *people)
			company = company || (npc->GetPosition() - _character->GetPosition()).Length() < 8.0f;
	for (const auto& giver : _bonusGivers)
		company = company || (giver.npc && (giver.position - _character->GetPosition()).Length() < 8.0f);
	if (company && _audioManager->IsDialoguePlaying() && _elapsedSeconds >= _playerGestureAt)
	{
		std::vector<std::string> gestures;
		for (const auto& [what, name] : _playerAnimations)
			if (what.rfind("dialogue_", 0) == 0)
				gestures.push_back(what);
		_playerGestureAt = _elapsedSeconds + 2.0 + (rand() % 15) / 10.0;
		_playerIdleFor = 0.0;
		if (!gestures.empty() && playPlayerAnimation(gestures[static_cast<std::size_t>(rand()) % gestures.size()], false))
			return;
	}

	_playerIdleFor += dt;
	if (_playerIdleFor > 8.0)
	{
		_playerIdleFor = 0.0;
		for (int tries = 0; tries < 4; ++tries)
			if (playPlayerAnimation(kIdles[static_cast<std::size_t>(rand()) % std::size(kIdles)], false))
				return;
	}
	playPlayerAnimation("loco_idle_rest", true);
}

void Game::LockMouse(bool lockMouse)
{
	if (!_gameInput)
		return;
	_gameInput->LockMouse(static_cast<SDL_Window*>(*_window), lockMouse);
}

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

Vector3 Game::unoccludedCameraPosition(const Vector3& subject, const Vector3& wanted) const
{
	// A follow camera at a fixed offset goes wherever the offset puts it: on
	// sr2's grid that was the middle of a tree, and half the screen was trunk.
	// If the level stands between the subject and the camera, bring the camera
	// in to just this side of it. Only static geometry counts -- a car passing
	// behind must not yank the view.
	constexpr float kCameraStandOff = 0.4f;
	Vector3 toCamera = wanted - subject;
	const float distance = toCamera.Length();
	if (!_worldPhysics || distance < 0.01f)
		return wanted;
	toCamera = toCamera / distance;

	Vector3 hit, normal;
	if (!_worldPhysics->CastRay(subject, toCamera, distance, hit, normal, true))
		return wanted;

	const float clear = (hit - subject).Length() - kCameraStandOff;
	return subject + toCamera * (clear > 0.0f ? clear : 0.0f);
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
	std::ifstream file("donut_progress.dat");
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
	std::ofstream file("donut_progress.dat");
	if (file.good())
		file << next << " " << level << "\n";
	Log::Info("Game: story progress saved, level {} mission {}", level, next + 1);
}

bool Game::levelAvailable(int level) const
{
	return FileSystem::exists(fmt::format("scripts/level{:02}/level.mfk", level));
}

void Game::switchLevel(int level)
{
	Log::Info("Game: leaving level {} for level {}", _levelNumber, level);

	// Everything that lives in the physics world goes before it: the mission's
	// cars and drivers, the NPCs, the pedestrians and the player.
	_scriptEngine->CleanupMission();
	_inVehicle = false;
	_activeVehicle = nullptr;
	_interior.clear();
	ClearNpcs();
	_ambientNpcs.clear();
	_bonusGivers.clear();
	_hitAndRun.reset();
	_pedestrianManager.reset();
	_trafficManager.reset();
	_collectibleManager.reset();
	_waspManager.reset();
	_pathGraph.reset();
	_character.reset();
	_level.reset();
	_worldPhysics = std::make_unique<WorldPhysics>(_lineRenderer.get());

	_levelNumber = level;
	_storyIndex = -1;
	loadLevel();
	LoadModel(_playerCharacter, _playerCharacter);
	_smoothCamPos = Vector3::Zero;
}

Quaternion Game::cameraLooking(float pitchDown, float yaw)
{
	// FreeCamera's orientation is the view's rotation, world to camera: the
	// inverse of the camera's own. Both follow cameras used to pass their own
	// rotation, so the view turned the other way about Y. At a yaw of 0 or pi
	// that is the same thing, which is why it went unseen; anywhere else the
	// camera stood on the right side of its subject and looked away from it --
	// at sr2's grid, facing +X, it showed the lawn behind the car.
	return Quaternion::MakeFromEuler(Vector3(pitchDown, yaw, 0.0f)).Inverse();
}

void Game::updateCarDamageLook(double dt)
{
	// Each car with a damaged look has, for each part, "<model><part>Norm.bmp"
	// and "<model><part>Dam.bmp". The parts give way one after the other.
	static const char* const kParts[] = {"Hood", "DoorD", "DoorP", "Back"};
	static const float kGivesWayBelow[] = {80.0f, 60.0f, 60.0f, 35.0f};

	// On foot, the car left at the kerb keeps its dents.
	if (!_inVehicle || !_activeVehicle)
		return;
	const std::string model = _activeVehicle->GetModelName();
	int stage = 0;
	if (!model.empty())
		while (stage < 4 && _health < kGivesWayBelow[stage])
			++stage;

	// Shaders are shared by name, and a car's are reset to their own texture
	// when a mesh first asks for them: what is shown is put again now and then.
	_damageLookAge += dt;
	if (model == _damageLookModel && stage == _damageLookStage && (stage == 0 || _damageLookAge < 2.0))
		return;
	_damageLookAge = 0.0;

	auto& resources = GetResourceManager();
	auto show = [&](const std::string& car, int parts) {
		for (int i = 0; i < 4; ++i)
		{
			const std::string norm = car + kParts[i] + "Norm.bmp";
			resources.ShowTextureAs(norm, i < parts ? car + kParts[i] + "Dam.bmp" : norm);
		}
	};
	// The car left behind is mended, to the eye.
	if (!_damageLookModel.empty() && _damageLookModel != model)
		show(_damageLookModel, 0);
	if (!model.empty())
		show(model, stage);
	if (model != _damageLookModel || stage != _damageLookStage)
		Log::Debug("Game: '{}' shows {} damaged part(s), health {:.0f}", model, stage, _health);
	_damageLookModel = model;
	_damageLookStage = stage;
}

void Game::updateCarDamage()
{
	// The car is hurt by what stops it: a sudden change in its horizontal
	// velocity, whatever it hit -- a wall, a lamp post, another car. Damage used
	// to be dealt only while the car's centre was within 1.5m of another car's,
	// which two cars bumper to bumper never are, and walls cost nothing.
	// Driving makes no such jolt: full brakes take ~0.2 m/s off a frame. Height
	// is left out, so that landing a jump on the suspension is free.
	constexpr float kImpactThreshold = 4.0f; // m/s lost in one frame
	constexpr float kDamagePerMs = 2.5f;     // health per m/s beyond it: a 70 km/h crash costs ~40

	if (!_inVehicle || !_activeVehicle)
	{
		_damageVehicle = nullptr;
		return;
	}
	const Vector3 velocity = _activeVehicle->GetVelocity();
	const Vector3 position = _activeVehicle->GetPosition();
	if (_damageVehicle == _activeVehicle)
	{
		// A car moved further than its speed explains was teleported, not hit.
		const float moved = (position - _damagePosition).Length();
		const float flat = Vector3(velocity.X - _damageVelocity.X, 0.0f, velocity.Z - _damageVelocity.Z).Length();
		if (moved < 5.0f && flat > kImpactThreshold)
		{
			const float damage = (flat - kImpactThreshold) * kDamagePerMs;
			_health = std::max(0.0f, _health - damage);
			AddShake(std::min(1.0f, flat * 0.05f));
			// A low falling thud, for want of the game's crash sounds.
			_audioManager->PlayRaw(SoundGenerator::Chirp(140.0f, 50.0f, 0.25f, 44100), 44100, 1, 16);
			Log::Info("Game: impact, {:.1f} m/s lost, -{:.0f} health, {:.0f} left", flat, damage, _health);
			if (flat > 7.0f)
				SayLine(flat > 12.0f ? "d_bcrash" : "d_mcrash");
			// Ramming the traffic is a Hit & Run offence.
			if (_hitAndRun && _trafficManager && !_scriptEngine->IsTrafficSuppressed())
				for (const auto& car : _trafficManager->GetCars())
					if ((car.position - position).Length() < 6.0f)
					{
						_hitAndRun->Add(15.0f, "rammed the traffic");
						// Its driver has something to say about it.
						static const std::vector<std::string> shouts = [this] {
							std::vector<std::string> lines;
							for (const auto& line : _audioManager->FindSounds("crash_"))
								if (line.rfind("traffic", 0) == 0)
									lines.push_back(line);
							return lines;
						}();
						if (!shouts.empty())
							_audioManager->PlayAudio(shouts[static_cast<std::size_t>(rand()) % shouts.size()], car.grounded);
						break;
					}
		}
	}
	_damageVehicle = _activeVehicle;
	_damageVelocity = velocity;
	_damagePosition = position;
}

void Game::SayLine(const std::string& kind)
{
	// homer\d_bcrash_hom_03.rsd: the character's folder, the kind, a take. One
	// every few seconds at most, and never over a mission's conversation.
	if (_elapsedSeconds - _lastLineAt < 3.0 || _audioManager->IsDialoguePlaying())
		return;
	const auto lines = _audioManager->FindSounds(_playerCharacter + "\\" + kind + "_");
	if (lines.empty())
		return;
	_lastLineAt = _elapsedSeconds;
	_audioManager->PlayAudio(lines[static_cast<std::size_t>(rand()) % lines.size()]);
}

void Game::alert(const std::string& term, const char* fallback)
{
	const std::string& text = _textBible.Get(term);
	_alertMessage = text.empty() ? fallback : text;
	_alertUntil = _elapsedSeconds + 3.0;
}

void Game::updateHitAndRun(double dt)
{
	// Indoors the police wait. Nor do they chase --autopilot, whose teleports
	// across a target's path are not the player's driving: in the unattended
	// level 4 run they rammed the traffic into a police chase.
	if (!_hitAndRun || !_character || !_interior.empty() || _autopilot)
		return;
	// A street race's stage: no meter, no police (DisableHitAndRun).
	if (_scriptEngine->IsHitAndRunOff())
	{
		if (_hitAndRun->GetMeter() > 0.0f || _hitAndRun->IsChasing())
			_hitAndRun->Reset();
		return;
	}
	const bool driving = _inVehicle && _activeVehicle;
	const Vector3 player = driving ? _activeVehicle->GetPosition() : _character->GetPosition();
	const float speed = driving ? _activeVehicle->GetSpeedKmh() : 0.0f;

	// People run down: a car going through them at more than walking pace.
	if (driving && std::abs(speed) > 15.0f && _pedestrianManager && _interior.empty())
		if (const int hit = _pedestrianManager->KnockDown(player, 2.2f, _activeVehicle->GetVelocity()); hit > 0)
		{
			_hitAndRun->Add(25.0f * static_cast<float>(hit), hit == 1 ? "ran someone down" : "ran people down");
			AddShake(0.4f);
			SayLine("d_hitp");
			// And they say so: one of the generic passers-by's "hit by a car"
			// lines (generic_male_1\p_hitbyc_mn1_01.rsd...).
			static const std::vector<std::string> cries = [this] {
				std::vector<std::string> lines;
				for (const auto& line : _audioManager->FindSounds("\\p_hitbyc_"))
					if (line.rfind("generic_", 0) == 0)
						lines.push_back(line);
				return lines;
			}();
			if (!cries.empty())
				_audioManager->PlayAudio(cries[static_cast<std::size_t>(rand()) % cries.size()]);
		}
	// A near miss at speed: someone jumps clear and says what they think of it
	// (generic_*\p_nhitbyc_*).
	if (driving && std::abs(speed) > 30.0f && _pedestrianManager && _interior.empty())
		if (static double lastShout = -10.0;
		    _elapsedSeconds - lastShout > 2.5 && _pedestrianManager->CountNear(player, 5.0f) > 0)
		{
			static const std::vector<std::string> shouts = [this] {
				std::vector<std::string> lines;
				for (const auto& line : _audioManager->FindSounds("\\p_nhitbyc_"))
					if (line.rfind("generic_", 0) == 0)
						lines.push_back(line);
				return lines;
			}();
			lastShout = _elapsedSeconds;
			if (!shouts.empty())
				_audioManager->PlayAudio(shouts[static_cast<std::size_t>(rand()) % shouts.size()]);
		}

	switch (_hitAndRun->Update(dt, player, speed, !driving, *_worldPhysics, *_pathGraph))
	{
	case HitAndRun::Event::ChaseStarted:
		_audioManager->PlayRaw(SoundGenerator::Chirp(700.0f, 1400.0f, 0.6f, 44100), 44100, 1, 16);
		alert("HIT_AND_RUN", "Hit & Run!");
		break;
	case HitAndRun::Event::Busted:
	{
		// The fine is the game's 50 coins, or what the purse holds; a mission
		// under way is failed.
		const int fine = std::min(_coins, 50);
		if (fine > 0)
			SpendCoins(fine);
		Log::Info("Game: busted, fined {} coins", fine);
		alert("BUSTED", "BUSTED!");
		_hitAndRun->Reset();
		_scriptEngine->FailMission("busted by the police", "MISSION_FAILED_HIT_N_RUN_HINT1");
		break;
	}
	default: break;
	}
}

int Game::rewardPrice(const std::string& name, const std::string& type, std::string* file) const
{
	// BindReward( "plowk_v", "art\cars\plowk_v.p3d", "car", "forsale", 1, 150, "simpson" );
	std::ifstream rewards("scripts/rewards.mfk");
	for (std::string line; std::getline(rewards, line);)
	{
		const std::size_t comment = line.find("//");
		const std::size_t at = line.find("BindReward(");
		if (at == std::string::npos || (comment != std::string::npos && comment < at) ||
		    line.find("\"" + name + "\"") == std::string::npos || line.find("\"" + type + "\"") == std::string::npos ||
		    line.find("\"forsale\"") == std::string::npos)
			continue;
		const std::size_t forsale = line.find("\"forsale\"");
		int level = 0, cost = 0;
		if (std::sscanf(line.c_str() + forsale + 9, " , %d , %d", &level, &cost) != 2)
			continue;
		if (file)
		{
			// The second string: "art\chars\m_pris_m.p3d".
			const std::size_t open = line.find('"', line.find('"', line.find('"', at) + 1) + 1);
			const std::size_t close = open == std::string::npos ? open : line.find('"', open + 1);
			*file = close == std::string::npos ? std::string() : line.substr(open + 1, close - open - 1);
		}
		return cost;
	}
	return 0;
}

bool Game::BuySkin(const std::string& skin)
{
	std::string file;
	const int price = rewardPrice(skin, "skin", &file);
	if (!_ownedSkins.count(skin))
	{
		if (!_autopilot && price > 0 && !SpendCoins(price))
			return false;
		_ownedSkins.insert(skin);
		Log::Info("Game: bought outfit '{}' for {} coins{}", skin, _autopilot ? 0 : price, _autopilot ? " (autopilot)" : "");
	}
	// Worn: "art\chars\m_pris_m.p3d" is model "m_pris", moving with the
	// character's own animations.
	const std::size_t slash = file.find_last_of("\\/");
	std::string model = file.substr(slash == std::string::npos ? 0 : slash + 1);
	if (model.size() > 6 && model.compare(model.size() - 6, 6, "_m.p3d") == 0)
		model.resize(model.size() - 6);
	if (_character && !_inVehicle && !model.empty() && FileSystem::exists(fmt::format("art/chars/{}_m.p3d", model)))
	{
		const Vector3 where = _character->GetPosition();
		LoadModel(model, _playerCharacter);
		SetPlayerPosition(where);
		// A change of clothes empties the Hit & Run meter, the game's hint says.
		if (_hitAndRun)
			_hitAndRun->Reset();
	}
	return true;
}

bool Game::BuyCar(const std::string& car)
{
	if (car.empty() || _ownedCars.count(car))
	{
		if (!car.empty())
			_currentCar = car;
		return true;
	}
	const int price = CarPrice(car);
	if (!_autopilot && price > 0 && !SpendCoins(price))
		return false;
	_ownedCars.insert(car);
	_currentCar = car;
	Log::Info("Game: bought '{}' for {} coins{}", car, _autopilot ? 0 : price, _autopilot ? " (autopilot)" : "");
	return true;
}

bool Game::SpendCoins(int amount)
{
	if (amount > _coins)
		return false;
	_coins -= amount;
	saveCoins();
	return true;
}

void Game::saveCoins() const
{
	// Coins an unattended run walks into were not earned either.
	if (_autopilot)
		return;
	std::ofstream file("donut_coins.dat");
	if (file.good())
		file << _coins << "\n";
}

void Game::runAutopilot(double dt)
{
	if (!_scriptEngine->IsMissionActive() || _gameState != GameState::InGame)
		return;

	// Every two seconds, not every frame: each step should be seen to land, and
	// a race's checkpoints are taken one at a time.
	_autopilotTimer += dt;
	if (_autopilotTimer < 2.0)
		return;
	_autopilotTimer = 0.0;

	// Chasing, ramming or racing is done at the wheel: a player left on foot by
	// the stage before (a talkto) has their car brought round. Level 4 m1's
	// dump was "rammed" on foot and timed out.
	if (!_inVehicle && _character)
	{
		const std::string& wanted = _scriptEngine->GetObjectiveType();
		Vehicle* car = _scriptEngine->GetPlayerCar();
		if (car && (wanted == "follow" || wanted == "destroy" || wanted == "dump" || wanted == "race"))
		{
			Log::Info("Game: autopilot takes the wheel of '{}'", car->GetName());
			car->Teleport(_character->GetPosition() + Vector3(2.5f, 0.0f, 0.0f), car->GetRotation());
			PutPlayerInVehicle(car);
			return;
		}
	}

	if (_scriptEngine->ObjectiveWantsCar() && !_inVehicle && !_scriptEngine->GetMissionVehicles().empty())
	{
		Log::Info("Game: autopilot gets in '{}'", _scriptEngine->GetMissionVehicles().front()->GetName());
		PutPlayerInVehicle(_scriptEngine->GetMissionVehicles().front().get());
		return;
	}

	// Tailing a car is done from behind it, and only when it is getting away:
	// dropping the player's car onto the target every two seconds rammed it
	// off its route.
	// A car to destroy is met head on: the player's car is parked across its
	// path, 15m ahead, and it drives into it.
	// A dump's truck likewise, until something has dropped out of it to pick up.
	const std::string& objective = _scriptEngine->GetObjectiveType();
	const bool ramming = objective == "destroy" || (objective == "dump" && !_scriptEngine->HasPickupsLying());
	if (const Vehicle* quarry = _scriptEngine->GetFollowTarget(); quarry && ramming && _inVehicle && _activeVehicle)
	{
		const Vector3 ahead = quarry->GetPosition() + quarry->GetRotation() * Vector3::Forward * 15.0f;
		Log::Info("Game: autopilot blocks '{}'", quarry->GetName());
		_activeVehicle->Teleport(ahead, quarry->GetRotation() * Quaternion(Vector3(0.0f, 1.0f, 0.0f), 1.5708f));
		return;
	}

	if (const Vehicle* followed = objective == "follow" ? _scriptEngine->GetFollowTarget() : nullptr)
	{
		const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
		// Well inside the tightest follow distance the scripts set (level 4 m1's
		// 120m): the autopilot moves every 2s, and at 75 km/h the car gains 40m
		// between two looks. At 100m it was lost before the next one, and at
		// 60m it still was, now and then, over a long chase.
		if ((followed->GetPosition() - me).Length() < 45.0f)
			return;
		// 25m behind, or closer when there is no road there: m5's van, put back
		// on its route in a sunken lane, had nothing but a drop 25m behind it,
		// and the player's car fell off the map.
		Vector3 behind;
		bool grounded = false;
		for (const float back : {18.0f, 12.0f, 8.0f})
		{
			behind = followed->GetPosition() - followed->GetRotation() * Vector3::Forward * back;
			// Not on top of the traffic: put down inside a traffic car's box, the
			// player's car was thrown out at 55 m/s and off the map, and the
			// car being followed was lost.
			bool clear = true;
			if (_trafficManager && !_scriptEngine->IsTrafficSuppressed())
				for (const auto& car : _trafficManager->GetCars())
					clear = clear && (car.position - behind).Length() > 6.0f;
			float ground = 0.0f;
			if (clear && _worldPhysics->FindGroundHeight(behind, 5.0f, 5.0f, ground, true))
			{
				grounded = true;
				break;
			}
		}
		if (!grounded)
			return;
		Log::Info("Game: autopilot catches up with '{}'", followed->GetName());
		if (_inVehicle && _activeVehicle)
			_activeVehicle->Teleport(behind, followed->GetRotation());
		else
			SetPlayerPosition(behind + Vector3(0.0f, 1.0f, 0.0f));
		return;
	}

	// Doors are walked through, not driven into, and people walked up to.
	if ((objective == "interior" || objective == "talkto") && _inVehicle && _activeVehicle)
	{
		Log::Info("Game: autopilot gets out of '{}'", _activeVehicle->GetName());
		_inVehicle = false;
		SetPlayerPosition(_activeVehicle->GetPosition() + Vector3(3.0f, 0.0f, 0.0f));
		_activeVehicle = nullptr;
		return;
	}

	Vector3 target;
	if (!_scriptEngine->GetObjectiveTarget(target))
		return;
	Log::Info("Game: autopilot to ({:.1f}, {:.1f}, {:.1f})", target.X, target.Y, target.Z);
	// Something to pick up is taken where it is, at its height: level 4 m3's
	// ketchup hangs 11m above the road, for a jump the autopilot cannot make.
	const bool pickup = objective == "delivery" || objective == "dump";
	if (_inVehicle && _activeVehicle)
		_activeVehicle->Teleport(target, _activeVehicle->GetRotation(), !pickup);
	else
		SetPlayerPosition(target + Vector3(0.0f, 1.0f, 0.0f));
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
	}
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
	_collectibleManager->SpawnOnPath();

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

	// The level's music. The game chains short segments by a music script
	// (l<n>_music.rms) that is not read here; the segments of the level's own
	// set play in order instead: Homer's tubas on level 1, Lisa's on 3, Marge's
	// lounge on 4, Halloween on 7. The other levels have no set of their own
	// and get the main themes.
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
		std::ifstream script(fmt::format("scripts/{}/leveli.mfk", levelDirectory()));
		for (std::string line; std::getline(script, line);)
			if (const std::size_t at = line.find("SetHitAndRunDecay("); at != std::string::npos)
				decay = static_cast<float>(std::atof(line.c_str() + at + 18));
		_hitAndRun = std::make_unique<HitAndRun>(cops.empty() ? "cPolice" : cops.front(), decay);
	}

	// The story is the level script's AddMission list, in order, when the
	// scripts have been copied off the CD. Without them "New Game" plays the
	// test mission, as it always has.
	_story.clear();
	_teleports.clear();
	_playerCharacter = "homer";
	_playerCarModel = "famil_v";
	// A new level, its own car: what was bought on the last one stays there.
	_currentCar.clear();
	_ownedCars.clear();
	_ownedSkins.clear();
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
	loadGags();
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
	if (_levelNumber != 1 && !_missionScriptGiven && !_story.empty())
		_missionScript = _story.front();
}

std::string Game::levelDirectory() const
{
	return fmt::format("level{:02}", _levelNumber);
}

std::vector<std::string> Game::levelWorldFiles() const
{
	// l<n>_TERRA.p3d, then l<n>z*.p3d and l<n>r*.p3d (zones and the roads
	// between them), then the interiors l<n>i*.p3d. Where a road comes in two
	// versions (level 1's l1r4a and l1r4b, the same stretch before and after
	// it changes) only the first is taken: both together overlap.
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
			    FileSystem::exists(fmt::format("./art/{}a.p3d", name.substr(0, name.size() - 5))))
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
	SetPlayerPosition(_interiorLayout.entry);
	// Facing into the room: the camera kept the heading from outside, which
	// for the school looked back out through the door at nothing.
	const Vector3 inward = _interiorLayout.origin - _interiorLayout.entry;
	if (inward.X * inward.X + inward.Z * inward.Z > 0.01f)
		_camYaw = std::atan2(inward.X, inward.Z);
	_level->SetDrawOnly(file);
	_scriptEngine->OnInteriorChanged(_interior);
}

void Game::updateInterior()
{
	if (_interior.empty() || !_character)
		return;

	const float toExit = (_character->GetPosition() - _interiorLayout.exit).Length();
	if (!_interiorExitArmed)
	{
		_interiorExitArmed = toExit > 2.5f;
		return;
	}
	if (toExit > 1.5f)
		return;

	Log::Info("Game: player comes out of '{}'", _interior);
	_interior.clear();
	_level->SetDrawOnly({});
	SetPlayerPosition(_interiorReturn);
	_scriptEngine->OnInteriorChanged(_interior);
}

std::unique_ptr<Character> Game::makeNpc(const std::string& name) const
{
	// Without a model they stay positions for "talkto" only.
	if (!FileSystem::exists(fmt::format("art/chars/{}_m.p3d", name)))
		return nullptr;

	auto npc = std::make_unique<Character>(name);
	npc->LoadModel(name);
	// npd_m.p3d and the like are a skeleton without a body: drawing one
	// crashed level 1 m5 the moment its "npd" joined the scene.
	if (!npc->HasMesh())
	{
		Log::Warn("Game: '{}' has no body to show, not placed", name);
		return nullptr;
	}
	npc->LoadAnimations(name);
	// Like the pedestrians, a character extracted without animations borrows
	// Apu's, whose skeleton the adult models share; Barney stood in his bind
	// pose, arms out.
	if (npc->GetAnimations().empty())
		npc->LoadAnimations("apu");
	// Standing about: at rest, or else the first idle in the set. "The first"
	// alone, in no particular order, had Apu doing yoga behind his counter.
	if (const std::string rest = npcRestAnimation(*npc); !rest.empty())
		npc->SetAnimation(rest);
	return npc;
}

bool Game::seatDriver(Character& character, const Vehicle& car)
{
	Vector3 seat;
	if (!car.GetDriverSeat(seat))
		return false;
	// The driving animations are built about a point beside the car: their
	// Motion_Root stands at (1.0, 0.46, -0.72) from the character's origin. Put
	// that root, not the origin, at the seat; facing the car's way.
	Vector3 root = Vector3::Zero;
	if (const auto* joint = character.FindJoint("Motion_Root"))
		root = joint->pose.Translation();
	character.SetRotation(car.GetRotation());
	character.FollowController(seat - car.GetRotation() * Vector3(root.X, 0.0f, root.Z));
	return true;
}

void Game::drawCarDrivers(const Matrix4x4& viewProjection)
{
	// The cars of the mission at hand that are not the player's: Skinner at
	// his own wheel, Eddie in the limo. Drawn where the camera can see them.
	std::set<const Vehicle*> present;
	for (const auto& vehicle : _scriptEngine->GetMissionVehicles())
	{
		const Vehicle* car = vehicle.get();
		const std::string name = _scriptEngine->GetVehicleDriver(car);
		if (name.empty())
			continue;
		present.insert(car);
		if (car == _activeVehicle || (car->GetPosition() - _camera->GetPosition()).Length() > 120.0f)
			continue;
		auto& driver = _carDrivers[car];
		// Made once: a car rebuilt where an old one stood is told by its driver.
		if (driver && driver->GetModelName() != name)
			driver.reset();
		if (!driver)
		{
			// Once only for one without a model: a file read a frame otherwise.
			if (_driversWithoutModel.count(name))
				continue;
			driver = makeNpc(name);
			if (!driver)
			{
				_driversWithoutModel.insert(name);
				continue;
			}
			for (const auto& [anim, data] : driver->GetAnimations())
				if (anim.size() > 12 && anim.compare(anim.size() - 12, 12, "_in_car_idle") == 0)
					driver->SetAnimation(anim);
		}
		if (seatDriver(*driver, *car))
		{
			glDisable(GL_CULL_FACE);
			driver->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
			glEnable(GL_CULL_FACE);
		}
	}
	// Those of cars gone (a mission over) go too.
	for (auto it = _carDrivers.begin(); it != _carDrivers.end();)
		it = present.count(it->first) ? std::next(it) : _carDrivers.erase(it);
}

std::string Game::npcRestAnimation(const Character& npc)
{
	std::string any;
	for (const auto& [anim, data] : npc.GetAnimations())
	{
		if (anim.size() > 15 && anim.compare(anim.size() - 15, 15, "_loco_idle_rest") == 0)
			return anim;
		if (any.empty() && anim.find("idle") != std::string::npos)
			any = anim;
	}
	return any;
}

void Game::animateNpc(Character& npc, bool talking)
{
	// Talking, a character gestures, one gesture after another, and goes back
	// to standing at rest.
	NpcGesture& gesture = _npcGestures[&npc];
	if (gesture.gesturing)
	{
		if (!npc.IsAnimationFinished())
			return;
		gesture.gesturing = false;
		if (const std::string rest = npcRestAnimation(npc); !rest.empty())
			npc.SetAnimation(rest);
	}
	if (gesture.nextAt == 0.0)
		gesture.nextAt = _elapsedSeconds + 12.0 + rand() % 14;
	// A conversation starting does not wait for the next idle.
	if (talking && !gesture.talking)
		gesture.nextAt = _elapsedSeconds;
	gesture.talking = talking;
	if (_elapsedSeconds < gesture.nextAt)
		return;
	// Talking, a gesture; standing about, now and then one of the character's
	// idles (Apu's yoga, Lisa's tai chi), but not those that are a pose -- and
	// not someone else's: Ralph borrows Apu's animations, not his yoga.
	const bool borrowed = npc.GetModelName() != "apu" && npcRestAnimation(npc).rfind("apu_", 0) == 0;
	if (!talking && borrowed)
	{
		gesture.nextAt = _elapsedSeconds + 30.0;
		return;
	}
	std::vector<std::string> gestures;
	for (const auto& [anim, data] : npc.GetAnimations())
		if (talking ? anim.find("_dialogue_") != std::string::npos
		            : anim.find("_idle_") != std::string::npos && anim.find("loco_idle") == std::string::npos &&
		                  anim.find("jump_idle") == std::string::npos && anim.find("car_idle") == std::string::npos)
			gestures.push_back(anim);
	if (gestures.empty())
		return;
	npc.SetAnimation(gestures[static_cast<std::size_t>(rand()) % gestures.size()], false);
	gesture.gesturing = true;
	gesture.nextAt = _elapsedSeconds + (talking ? 1.5 + (rand() % 20) / 10.0 : 12.0 + rand() % 14);
}

void Game::PlaceNpc(const std::string& name, const Vector3& position)
{
	auto found = _npcs.find(name);
	if (found == _npcs.end())
	{
		auto npc = makeNpc(name);
		if (!npc)
			return;
		found = _npcs.emplace(name, std::move(npc)).first;
		Log::Info("Game: NPC '{}' joins the scene", name);
	}
	found->second->SetPosition(position);
}

void Game::ClearNpcs()
{
	_npcs.clear();
}

const Game::BonusGiver* Game::bonusGiverNear() const
{
	if (!_character || _inVehicle || !_interior.empty() || _bonusReturn >= 0)
		return nullptr;
	for (const auto& giver : _bonusGivers)
	{
		const Vector3 gap = giver.position - _character->GetPosition();
		if (gap.X * gap.X + gap.Z * gap.Z < 9.0f && std::abs(gap.Y) < 3.0f)
			return &giver;
	}
	return nullptr;
}

bool Game::startBonusMission()
{
	// The story mission under way is left, to be taken up again from its start
	// once this one is over, won or not.
	const BonusGiver* giver = bonusGiverNear();
	if (!giver || _storyIndex < 0)
		return false;
	_bonusReturn = _storyIndex;
	_storyIndex = -1;
	_missionScript = fmt::format("scripts/{}/{}i.mfk", levelDirectory(), giver->mission);
	Log::Info("Game: bonus mission '{}', the story waits at mission {}", giver->mission, _bonusReturn + 1);
	_scriptEngine->CleanupMission();
	if (_collectibleManager)
		_collectibleManager->SpawnOnPath();
	runMissionScript();
	Vector3 start;
	if (_scriptEngine->GetPlayerStart(start))
		SetPlayerPosition(start);
	return true;
}

bool Game::ringDoorbell()
{
	if (!_character || _inVehicle)
		return false;
	const Vector3 me = _character->GetPosition();
	for (const auto& trigger : _level->GetTriggers())
	{
		// [DB_skinner | skinner_switch11 | Doorbell]: the resident is named
		// in the first string.
		if (trigger.action != "Doorbell" || trigger.data.rfind("DB_", 0) != 0 ||
		    !(trigger.Contains(me) || (trigger.position - me).Length() < 3.0f))
			continue;
		const std::string resident = trigger.data.substr(3);
		playPlayerAnimation("hit_switch", false);
		// The button goes in, and the switch it names moves with it.
		_level->PlayAnimatedObject(trigger.data);
		_level->PlayAnimatedObject(trigger.actionTarget);
		const auto lines = _audioManager->FindSounds(resident + "\\w_doorbell_");
		Log::Info("Game: rang {}'s doorbell ({} answers)", resident, lines.size());
		_audioManager->PlayRaw(SoundGenerator::Chirp(900.0f, 700.0f, 0.3f, 44100), 44100, 1, 16);
		if (!lines.empty())
			_audioManager->QueueDialogue(lines[static_cast<std::size_t>(rand()) % lines.size()],
			                             AudioManager::DialoguePriority::Gameplay);
		return true;
	}
	return false;
}

void Game::loadGags()
{
	_gags.clear();
	_gagsPrimed = false;
	std::ifstream script(fmt::format("scripts/{}/level.mfk", levelDirectory()));
	if (!script)
		return;
	// Found in an earlier session (donut_gags.dat, every level's).
	std::set<std::string> found;
	{
		std::ifstream file("donut_gags.dat");
		for (std::string id; std::getline(file, id);)
			if (!id.empty())
				found.insert(id);
	}
	std::optional<Gag> gag;
	int unplaced = 0;
	for (std::string line; std::getline(script, line);)
	{
		if (const std::size_t comment = line.find("//"); comment != std::string::npos)
			line.erase(comment);
		const std::size_t open = line.find('(');
		const std::size_t close = line.rfind(')');
		if (open == std::string::npos || close == std::string::npos || close < open)
			continue;
		std::string command = line.substr(0, open);
		command.erase(0, command.find_first_not_of(" \t"));
		command.erase(command.find_last_not_of(" \t") + 1);
		// The arguments, unquoted and trimmed.
		std::vector<std::string> args;
		std::stringstream list(line.substr(open + 1, close - open - 1));
		for (std::string arg; std::getline(list, arg, ',');)
		{
			arg.erase(std::remove(arg.begin(), arg.end(), '"'), arg.end());
			arg.erase(0, arg.find_first_not_of(" \t"));
			arg.erase(arg.find_last_not_of(" \t") + 1);
			args.push_back(arg);
		}

		if (command == "GagBegin")
		{
			gag.emplace();
			gag->model = args.empty() ? "" : args[0];
		}
		else if (!gag)
			continue;
		else if (command == "GagSetPosition" && args.size() == 1)
			gag->placed = _level->GetLocatorPosition(args[0]);
		else if (command == "GagSetPosition" && args.size() == 3)
			gag->placed = Vector3(static_cast<float>(std::atof(args[0].c_str())),
			                      static_cast<float>(std::atof(args[1].c_str())),
			                      static_cast<float>(std::atof(args[2].c_str())));
		else if (command == "GagSetSound" && !args.empty())
			gag->sound = args[0];
		else if (command == "GagSetRandom" && !args.empty())
			gag->random = std::atoi(args[0].c_str()) != 0;
		else if (command == "GagSetCycle" && !args.empty())
			gag->cycle = args[0];
		else if (command == "GagSetCameraShake")
			gag->shakes = true;
		else if (command == "GagSetCoins" && !args.empty())
			gag->coins = std::atoi(args[0].c_str());
		else if (command == "GagSetTrigger" && args.size() == 3)
		{
			// ("action", PickleTrig, 1.5): a locator of the level's.
			gag->byHand = args[0] == "action";
			gag->position = _level->GetLocatorPosition(args[1]);
			gag->radius = static_cast<float>(std::atof(args[2].c_str()));
			if (gag->position == Vector3::Zero)
				gag->sound.clear(), ++unplaced;
		}
		else if (command == "GagSetTrigger" && args.size() == 5)
		{
			// ("touch", x, y, z, radius).
			gag->byHand = args[0] == "action";
			gag->position = Vector3(static_cast<float>(std::atof(args[1].c_str())),
			                        static_cast<float>(std::atof(args[2].c_str())),
			                        static_cast<float>(std::atof(args[3].c_str())));
			gag->radius = static_cast<float>(std::atof(args[4].c_str()));
		}
		else if (command == "GagEnd")
		{
			if (!gag->sound.empty())
			{
				// A model of its own: an animated one is posed gag by gag.
				const std::string file = "art/nis/gags/" + gag->model;
				if (gag->placed != Vector3::Zero && FileSystem::exists(file))
				{
					gag->drawable = CompositeModel::LoadP3D(file);
					gag->skins = SceneSkins::Load(file);
				}
				gag->played = found.count(gagId(*gag)) > 0;
				_gags.push_back(*gag);
			}
			gag.reset();
		}
	}
	Log::Info("Game: {} gag(s) in the level, {} found before, {} whose trigger locator is not loaded", _gags.size(),
	          std::count_if(_gags.begin(), _gags.end(), [](const Gag& g) { return g.played; }), unplaced);
}

std::string Game::gagId(const Gag& gag) const
{
	return fmt::format("{} {} {:.0f} {:.0f}", levelDirectory(), gag.model, gag.position.X, gag.position.Z);
}

void Game::saveGags() const
{
	if (_autopilot)
		return;
	std::set<std::string> found;
	{
		std::ifstream file("donut_gags.dat");
		for (std::string id; std::getline(file, id);)
			if (!id.empty())
				found.insert(id);
	}
	for (const auto& gag : _gags)
		if (gag.played)
			found.insert(gagId(gag));
	std::ofstream file("donut_gags.dat");
	for (const auto& id : found)
		file << id << "\n";
}

void Game::playGag(Gag& gag)
{
	// "gag_jasp" is interactive_props\gag_jasp_01.rsd to _03, "gag_s1"
	// nis\gag_s1.rsd: one of them, from the English set (the other languages
	// have folders of their own).
	std::vector<std::string> sounds;
	for (const auto& sound : _audioManager->FindSounds("\\" + gag.sound))
	{
		const std::size_t slash = sound.rfind('\\');
		const std::size_t folder = sound.rfind('\\', slash - 1);
		const std::string parent = sound.substr(folder + 1, slash - folder - 1);
		const std::string rest = sound.substr(slash + 1 + gag.sound.size());
		const bool numbered = rest.size() > 1 && rest[0] == '_' && std::isdigit(static_cast<unsigned char>(rest[1]));
		if (parent != "french" && parent != "german" && parent != "spanish" && (rest[0] == '.' || numbered))
			sounds.push_back(sound);
	}
	if (!sounds.empty())
		_audioManager->QueueDialogue(sounds[static_cast<std::size_t>(rand()) % sounds.size()],
		                             AudioManager::DialoguePriority::Gameplay);
	gag.lastPlayed = _elapsedSeconds;
	if (gag.animStartedAt < 0.0 || gag.cycle != "single")
		gag.animStartedAt = _elapsedSeconds;
	if (gag.shakes)
		AddShake(1.0f);
	Log::Info("Game: gag '{}' ({}), {} sound(s){}", gag.model, gag.sound, sounds.size(),
	          !gag.played && gag.coins > 0 ? fmt::format(", {} coin(s)", gag.coins) : "");
	if (!gag.played && gag.coins > 0)
	{
		_coins += gag.coins;
		saveCoins();
	}
	if (!gag.played)
	{
		// Found: the count, as the card's title is shown, with the game's own
		// word for them.
		gag.played = true;
		const auto found = std::count_if(_gags.begin(), _gags.end(), [](const Gag& g) { return g.played; });
		const std::string& word = _textBible.Get("GAGS");
		_cardMessage = fmt::format("{} {}/{}", word.empty() ? "Gags" : word, found, _gags.size());
		if (static_cast<std::size_t>(found) == _gags.size())
			_cardMessage += " - " + _textBible.Get("GAGS_COMPLETE");
		_cardMessageUntil = _elapsedSeconds + 3.0;
		saveGags();
	}
}

void Game::drawGags(const Matrix4x4& viewProjection)
{
	GL::ShaderProgram& worldShader = _level->GetWorldShader();
	for (auto& gag : _gags)
	{
		if (!gag.drawable || (gag.placed - _camera->GetPosition()).Length() > 150.0f)
			continue;
		// A scene with characters in it is seen only while it plays: Grandpa
		// walks in, says his piece and is gone.
		if (gag.skins && gag.drawable->IsAnimated())
		{
			const double played = (_elapsedSeconds - gag.animStartedAt) * gag.drawable->GetAnimationFrameRate();
			if (gag.animStartedAt < 0.0 || (gag.cycle != "cycle" && played >= gag.drawable->GetAnimationFrames()))
				continue;
		}
		if (gag.drawable->IsAnimated())
		{
			const float rate = gag.drawable->GetAnimationFrameRate();
			const float frames = gag.drawable->GetAnimationFrames();
			float frame = 0.0f;
			if (gag.cycle == "cycle")
				frame = static_cast<float>(_elapsedSeconds) * rate;
			else if (gag.animStartedAt >= 0.0)
			{
				frame = static_cast<float>(_elapsedSeconds - gag.animStartedAt) * rate;
				// Played through: "reset" goes back to its rest, the others
				// stay as they end.
				if (frame >= frames && gag.cycle == "reset")
				{
					gag.animStartedAt = -1.0;
					frame = 0.0f;
				}
			}
			gag.drawable->Pose(frame, gag.cycle == "cycle");
			if (gag.skins)
				gag.skins->Pose(frame, gag.cycle == "cycle");
		}
		const Matrix4x4 model = Matrix4x4::MakeTranslate(gag.placed);
		for (const bool opaque : {true, false})
			gag.drawable->Draw(worldShader, viewProjection, model, opaque);
		if (gag.skins)
		{
			glDisable(GL_CULL_FACE); // skinned characters, as for the player
			gag.skins->Draw(viewProjection, model, *_skinShaderProgram);
		}
	}
}

bool Game::useGag()
{
	if (!_character || _inVehicle)
		return false;
	const Vector3 me = _character->GetPosition();
	for (auto& gag : _gags)
		if (gag.byHand && (gag.position - me).Length() < gag.radius + 1.0f)
		{
			playGag(gag);
			return true;
		}
	return false;
}

void Game::updateGags()
{
	if (!_character)
		return;
	const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
	for (auto& gag : _gags)
	{
		if (gag.byHand)
			continue;
		const bool inside = (gag.position - me).Length() < gag.radius;
		// Once on the way in, and not again for a few seconds.
		if (_gagsPrimed && inside && !gag.inside && _elapsedSeconds - gag.lastPlayed > 5.0)
		{
			// Of the random gags on this trigger, one plays; the others are
			// counted as walked into, so that they wait for the next time.
			std::vector<Gag*> shared;
			if (gag.random)
				for (auto& other : _gags)
					if (other.random && !other.byHand && (other.position - gag.position).Length() < 0.1f &&
					    std::abs(other.radius - gag.radius) < 0.01f)
					{
						shared.push_back(&other);
						other.inside = true;
					}
			Gag& picked = shared.empty() ? gag : *shared[static_cast<std::size_t>(rand()) % shared.size()];
			playGag(picked);
			for (Gag* other : shared)
				other->lastPlayed = _elapsedSeconds;
		}
		gag.inside = inside;
	}
	_gagsPrimed = true;
}

bool Game::usePhoneBooth()
{
	if (!_character || _inVehicle)
		return false;
	const Vector3 me = _character->GetPosition();
	for (const auto& trigger : _level->GetTriggers())
	{
		if (trigger.action != "SummonVehiclePhone" ||
		    !(trigger.Contains(me) || (trigger.position - me).Length() < 3.0f))
			continue;

		// The game offers every car the player owns; there is no garage here,
		// so it is the car the mission gave them.
		Vehicle* car = _scriptEngine->GetPlayerCar();
		if (car == nullptr)
		{
			Log::Info("Game: phone booth '{}', but no car to send", trigger.locator);
			return true;
		}
		const int node = _pathGraph->FindNearestRoadNode(trigger.position);
		const Vector3 road = node >= 0 ? _pathGraph->GetNodes()[node].position : trigger.position;
		car->Teleport(road, car->GetRotation());
		_health = 100.0f;
		PutPlayerInVehicle(car);
		_audioManager->PlayRaw(SoundGenerator::Beep(1200, 0.2f), 22050, 1, 16);
		Log::Info("Game: phone booth '{}' sends '{}' to ({:.0f}, {:.0f}, {:.0f}), repaired", trigger.locator,
		          car->GetName(), road.X, road.Y, road.Z);
		return true;
	}
	return false;
}

void Game::PutPlayerInVehicle(Vehicle* vehicle)
{
	if (vehicle == nullptr)
		return;
	_inVehicle = true;
	_activeVehicle = vehicle;
	Log::Info("Game: player put in vehicle '{}'", vehicle->GetName());
}

void Game::SetPlayerPosition(const Vector3& pos)
{
	if (_character)
	{
		_character->SetPosition(pos);
		_character->GetCharacterController().warp(BulletCast<btVector3>(pos));
		Log::Info("Game: player teleported to ({:.1f}, {:.1f}, {:.1f})", pos.X, pos.Y, pos.Z);
	}
}

Vector3 Game::GetPlayerPosition() const
{
	if (_character)
		return _character->GetPosition();
	return Vector3::Zero;
}

std::vector<std::pair<std::string, std::string>> models {
    {"homer", "homer"}, {"h_evil", "homer"}, {"h_fat", "homer"}, {"h_undr", "homer"},
    {"marge", "marge"}, {"bart", "bart"},    {"apu", "apu"},     {"a_amer", "apu"},
};

void Game::OnInputTextEntry(const std::string& text) {}

void Game::ensureFullscreenQuad()
{
	if (_fullscreenQuadVB)
		return;

	// Two triangles in NDC (xyz), covering the whole screen. The postprocess
	// vertex shader derives texCoord from position.xy so we only need positions.
	const float quad[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
	};

	_fullscreenQuadVB = std::make_shared<GL::VertexBuffer>(quad, 6, sizeof(float) * 3);
	_fullscreenQuadBinding = std::make_shared<GL::VertexBinding>();
	GL::ArrayElement element(_fullscreenQuadVB.get(), 0, 3, GL::AE_FLOAT, sizeof(float) * 3, 0);
	_fullscreenQuadBinding->Create(&element, 1, *_fullscreenQuadVB);
}

void Game::ensureSceneFBO(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	if (!_sceneFBO)
	{
		GL::FrameBuffer::Format format;
		format.EnableColourBuffer(true, 1);
		format.EnableDepthBuffer(true, true);
		_sceneFBO = std::make_unique<GL::FrameBuffer>(width, height, format);
		_sceneFboW = width;
		_sceneFboH = height;
	}
	else if (width != _sceneFboW || height != _sceneFboH)
	{
		_sceneFBO->SetSize(width, height);
		_sceneFboW = width;
		_sceneFboH = height;
	}
}

void Game::blitSceneToBackbuffer(int width, int height)
{
	if (!_sceneFBO || !_postProcessShader || !_fullscreenQuadBinding)
		return;

	GL::FrameBuffer::Unbind();
	glViewport(0, 0, width, height);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	_postProcessShader->Bind();

	// Passthrough by default: neutralise every effect so the image is identical
	// to a direct render. Individual FX tickets raise the relevant uniforms.
	glActiveTexture(GL_TEXTURE0);
	// The world renderer leaves a sampler object bound to unit 0; it requests
	// mipmapped filtering, which makes our single-level FBO colour texture
	// sampler-incomplete (every fetch returns black). Detach it so the
	// texture's own GL_LINEAR parameters apply for the post-process pass.
	glBindSampler(0, 0);
	_sceneFBO->BindColorTexture(0);
	_postProcessShader->SetUniformValue("sceneTex", 0);
	_postProcessShader->SetUniformValue("screenSize", Vector2(static_cast<float>(width), static_cast<float>(height)));
	_postProcessShader->SetUniformValue("bloomIntensity", 0.0f);
	_postProcessShader->SetUniformValue("vignetteIntensity", _vignetteIntensity);
	_postProcessShader->SetUniformValue("contrast", 1.0f);
	_postProcessShader->SetUniformValue("saturation", 1.0f);

	_fullscreenQuadBinding->Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	_fullscreenQuadBinding->Unbind();

	glEnable(GL_DEPTH_TEST);
}

void Game::Run()
{
	// measure our delta time
	uint64_t now = SDL_GetPerformanceCounter();
	uint64_t last = 0;
	double deltaTime = 0.0;

	FpsTimer timer;

	SpriteBatch sprites(1024);
	GL::ShaderProgram& spriteShader = sprites.GetShader();

	auto animCamera = AnimCamera::LoadP3D("art/missions/level01/mission0cam.p3d");

	bool running = true;
	while (running)
	{
		last = now;
		now = SDL_GetPerformanceCounter();

		deltaTime = ((now - last) / (double)SDL_GetPerformanceFrequency());
		timer.Update(deltaTime);
		if (_hornCooldown > 0.0f)
			_hornCooldown = std::max(0.0f, _hornCooldown - static_cast<float>(deltaTime));

		GameIntent intent = _gameInput->Poll(static_cast<SDL_Window*>(*_window));
		if (intent.requestQuit)
			running = false;

		// A frame that takes seconds is not a slow frame, it is a hang, and it is
		// invisible from the outside: the window simply stops. Say so.
		if (deltaTime > 1.0)
			Log::Warn("Game: frame took {:.1f}s at t={:.0f}s", deltaTime, _elapsedSeconds);

		_elapsedSeconds += deltaTime;
		if (_quitAfterSeconds > 0.0 && _elapsedSeconds >= _quitAfterSeconds)
		{
			Log::Info("Game: --quit-after {:.0f}s reached, closing", _quitAfterSeconds);
			running = false;
		}

		// Third-person camera: capture the mouse during on-foot gameplay so its
		// motion orbits the camera around the character. The camera no longer
		// follows the character's facing — only the mouse turns it.
		const bool onFoot = (_gameState == GameState::InGame) && _character && !_inVehicle;
		LockMouse(onFoot);
		if (onFoot && _gameInput->IsMouseLocked())
		{
			_camYaw += intent.mouseDX * 0.005f;
			_camPitch -= intent.mouseDY * 0.005f; // mouse up -> look up
			if (_camPitch < -1.0f) _camPitch = -1.0f; // steep down
			if (_camPitch > 0.2f) _camPitch = 0.2f;    // slightly up
		}

		if (onFoot && _boarding.car)
		{
			intent.moveForward = intent.moveBackward = intent.moveLeft = intent.moveRight = 0.0f;
			intent.interact = false;
			updateBoarding();
		}
		if (onFoot)
		{
			auto& ctrl = _character->GetCharacterController();
			auto charMove = Vector3::Zero;
			charMove += Vector3::Forward * intent.moveForward;
			charMove += Vector3::Backward * intent.moveBackward;
			charMove += Vector3::Left * intent.moveLeft;
			charMove += Vector3::Right * intent.moveRight;

			// A phone booth answers the action button before anything else.
			if (intent.interact && (usePhoneBooth() || ringDoorbell() || useGag() || startBonusMission()))
				intent.interact = false;

			if (intent.interact)
			{
				if (_scriptEngine->IsMissionActive())
				{
					for (auto& v : _scriptEngine->GetMissionVehicles())
					{
						if ((v->GetPosition() - _character->GetPosition()).Length() < 5.0f)
						{
							// By the driver's door, the door first (updateBoarding
							// seats the player when done). Consume the press, or it
							// would jump as well.
							startBoarding(v.get(), true);
							intent.interact = false;
							// getting in the car starts the race; finishing it is the
							// checkpoint loop's job (this used to complete the stage here)
							break;
						}
					}
				}
				if (!_inVehicle && !_boarding.car && ctrl.canJump())
				{
					ctrl.jump(btVector3(0, 0, 0));
					playPlayerAnimation("jump_idle_take_off", false);
					_lastJumpAt = _elapsedSeconds;
					_kickedThisJump = false;
				}
				// In the air, a kick: once a jump. It hits a wasp in reach (the
				// attack lasts a second more) and breaks what it lands on.
				else if (!_inVehicle && !_boarding.car && !ctrl.onGround() && !_kickedThisJump &&
				         playPlayerAnimation("jump_kick", false))
				{
					_kickedThisJump = true;
					_lastJumpAt = _elapsedSeconds;
					if (_level->BreakObjectsNear(_character->GetPosition(), 2.0f) > 0)
						AddShake(0.2f);
				}
			}

			if (intent.debugTeleportToVehicle && _scriptEngine->IsMissionActive())
			{
				for (auto& v : _scriptEngine->GetMissionVehicles())
				{
					// Land beside the car, inside the boarding radius below. The old
					// offset (0, 2, 5) put the player 5.39 units away -- just outside
					// the < 5 check -- so the debug teleport could never be followed
					// by "interact" to actually get in.
					SetPlayerPosition(v->GetPosition() + Vector3(2.5f, 1.0f, 0.0f));
					break;
				}
			}

			const bool moving = charMove.LengthSquared() > 0.0f;
			if (moving)
			{
				// Camera-relative movement: "forward" is where the camera looks.
				// worldMove depends on the (mouse-driven) camera yaw, not on the
				// character's own rotation, so there's no feedback loop. The
				// character then pivots to face its movement direction.
				charMove.Normalize();
				Vector3 worldMove = Quaternion::MakeFromEuler(Vector3(0.0f, _camYaw, 0.0f)) * charMove;
				// A dash with the boost held: faster, and in Homer's dash.
				_playerDashing = intent.boostHeld;
				ctrl.setWalkDirection(BulletCast<btVector3>(worldMove * (_playerDashing ? 8.0f : 5.0f)));

				// A character model faces -Z: unturned, Homer walked +Z backwards,
				// showing the camera behind him his face.
				float targetYaw = atan2f(worldMove.X, worldMove.Z) + 3.14159265f;
				float curYaw = _character->GetRotation().Yaw();
				float delta = targetYaw - curYaw;
				while (delta > 3.14159265f) delta -= 6.28318531f;
				while (delta < -3.14159265f) delta += 6.28318531f;
				float t = 1.0f - static_cast<float>(exp(-12.0 * deltaTime));
				_character->SetRotation(Quaternion::MakeFromEuler(Vector3(0.0f, curYaw + delta * t, 0.0f)));
			}
			else
			{
				ctrl.setWalkDirection(btVector3(0, 0, 0));
			}

			updatePlayerAnimation(deltaTime, moving, ctrl.onGround());
		}

		// Checkpoints are reached on foot as well as at the wheel: a mission's
		// "goto" is often a walk to a door.
		if (_scriptEngine->IsMissionActive() && _character)
		{
			const Vector3 playerPos = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
			                                                       : _character->GetPosition();
			const auto& checkpoints = _scriptEngine->GetCheckpoints();
			const int current = _scriptEngine->GetCurrentCheckpoint();
			if (current >= 0 && current < static_cast<int>(checkpoints.size()) &&
			    (playerPos - checkpoints[current]).Length() < 5.0f)
				_scriptEngine->AdvanceCheckpoint();
		}

		if (_inVehicle && _activeVehicle)
		{
			if (intent.interact)
			{
				// Out by the driver's door, and the door shut behind.
				Vehicle* car = _activeVehicle;
				_inVehicle = false;
				_activeVehicle = nullptr;
				SetPlayerPosition(driverDoor(*car));
				startBoarding(car, false);
				Log::Info("Game: exited vehicle");
			}
		else if (intent.honk)
		{
			if (_hornCooldown <= 0.0f)
			{
				// The car's own horn when its sound script names one.
				if (const std::string horn = _audioManager->CarSoundFile(_activeVehicle->GetName(), "_horn"); !horn.empty())
					_audioManager->PlayAudio(horn);
				else
					_audioManager->PlayRaw(SoundGenerator::Horn(0.4f), 22050, 1, 16);
				_hornCooldown = 0.4f;
			}
		}
			else if (intent.vehicleJump)
			{
				_activeVehicle->Jump();
				_shakeAmount = 1.0f;
			}
			else
			{
				// throttle/brake are 0..1, steer is -1..1 (right minus left);
				// keyboard gives full deflection, sticks/triggers scale analogically
				float throttle = intent.moveForward;
				float brake = intent.moveBackward;
				const float steer = intent.moveRight - intent.moveLeft;
				const float boost = intent.boostHeld ? 3.0f : 1.0f;
				// The turbo's first moment gets a word.
				static bool boosting = false;
				if (intent.boostHeld && !boosting)
					SayLine("w_turbo");
				boosting = intent.boostHeld;

				// Back brakes while the car is still rolling forwards and reverses
				// once it has stopped. Previously it only ever braked, so a car
				// nosed into a wall stayed there.
				if (brake > 0.0f && _activeVehicle->GetSpeedKmh() < kReverseThresholdKmh)
				{
					throttle = -brake;
					brake = 0.0f;
				}

				// Held on the line, like the opponents, until the countdown says go.
				if (_scriptEngine->IsMissionActive() && _scriptEngine->IsRaceHeld())
					_activeVehicle->ApplyInput(0.0f, 0.0f, 1.0f, 1.0f);
				else
					_activeVehicle->ApplyInput(throttle, steer, brake, boost);
			}
		}

		auto cameraTransform = animCamera->Update(deltaTime * 35.0);

		if (_inVehicle && _activeVehicle && !_gameInput->IsMouseLocked())
		{
			auto vehPos = _activeVehicle->GetPosition();
			auto vehRot = _activeVehicle->GetRotation();
			Vector3 camTarget = vehPos + Vector3(0, 1.5f, 0);
			// Camera sits behind the vehicle; forward is +Z (left-handed proj), so
			// "behind" is -Z in the vehicle's local frame. Use the vehicle's yaw
			// only: the chassis is a physics body that pitches and rolls over
			// bumps, and rotating the offset by the full orientation swung the
			// camera under the terrain. The look rotation below already takes
			// yaw alone.
			// Euler().Y was taken here for the yaw, but it is in degrees and folds
			// past 90: the camera stood behind only a car heading straight down +Z.
			const Quaternion vehYaw = Quaternion::MakeFromEuler(Vector3(0.0f, vehRot.Yaw(), 0.0f));
			Vector3 targetPos = camTarget + vehYaw * Vector3(0, 3.0f, -12.0f);

			float lerpFactor = 1.0f - exp(-8.0f * static_cast<float>(deltaTime));
			if (_smoothCamPos == Vector3::Zero) _smoothCamPos = targetPos;
			_smoothCamPos = _smoothCamPos + (targetPos - _smoothCamPos) * lerpFactor;

			// Looking at the car from wherever the camera ended up. A fixed pitch
			// along the car's heading lost it whenever the level pulled the camera
			// in: at sr2's grid, with the lawn behind, it sat 2m above the car and
			// looked out over it.
			const Vector3 cameraPos = unoccludedCameraPosition(camTarget, _smoothCamPos);
			const Vector3 toCar = camTarget - cameraPos;
			const float flat = std::sqrt(toCar.X * toCar.X + toCar.Z * toCar.Z);
			const float yaw = flat > 0.5f ? atan2f(toCar.X, toCar.Z) : vehRot.Yaw();
			_camera->SetPosition(cameraPos);
			_camera->SetQuaternion(cameraLooking(atan2f(-toCar.Y, std::max(flat, 0.5f)), yaw));
		}
		else if (_character)
		{
			// Mouse-controlled orbit camera: the camera yaw/pitch come from the
			// mouse (above), the camera orbits the character at a fixed distance
			// and looks at it. It follows the character's position but not its
			// facing, so it no longer swings around when the character turns.
			auto charPos = _character->GetPosition();
			// _camPitch is negative looking down; MakeFromEuler pitches +Z down
			// for a positive angle.
			Quaternion camRot = Quaternion::MakeFromEuler(Vector3(-_camPitch, _camYaw, 0.0f));
			Vector3 lookAt = charPos + Vector3(0.0f, 1.2f, 0.0f);
			Vector3 forward = camRot * Vector3::Forward;
			Vector3 targetPos = lookAt - forward * 6.0f;

			float lerpFactor = 1.0f - exp(-12.0f * static_cast<float>(deltaTime));
			if (_smoothCamPos == Vector3::Zero) _smoothCamPos = targetPos;
			_smoothCamPos = _smoothCamPos + (targetPos - _smoothCamPos) * lerpFactor;

			_camera->SetPosition(unoccludedCameraPosition(lookAt, _smoothCamPos));
			_camera->SetQuaternion(cameraLooking(-_camPitch, _camYaw));
		}
		//_camera->SetPosition(cameraTransform.Translation());

		// Pause has to stop the world, not just hide it: this used to keep running
		// every frame while paused, so physics and scripts ticked behind the overlay.
		const bool simulate = _gameState != GameState::Paused;

		if (simulate)
			_worldPhysics->Update(static_cast<float>(deltaTime));

		// The listener is the camera; it was never placed, and every sound given
		// a position was heard from the world's origin, if at all. OpenAL takes
		// right as forward x up; in this world +X is on the right looking down
		// +Z, which takes "up" as -Y to come out the same way round.
		{
			const Vector3 forward = _camera->GetOrientation().Inverse() * Vector3::Forward;
			_audioManager->SetListenerPosition(_camera->GetPosition(), forward, Vector3(0.0f, -1.0f, 0.0f));
		}
		_audioManager->Update();
		// The place's background: the level's outdoors, or a small room indoors.
		// The game picks it by zone with a script (ambience.rms) not read here.
		{
			static const char* const kOutdoors[] = {"suburb_day_loop",    "city_busy_loop",       "squidport_amb_loop_02",
			                                       "suburb_night_loop",  "city_night_loop",      "squidport_night_loop",
			                                       "halloween_amb_loop"};
			const int level = std::clamp(_levelNumber, 1, 7);
			std::string indoors = _interior;
			std::transform(indoors.begin(), indoors.end(), indoors.begin(), [](unsigned char c) { return std::tolower(c); });
			_audioManager->PlayAmbience(_interior.empty()                         ? kOutdoors[level - 1]
			                            : indoors.find("dmv") != std::string::npos ? "\\dmv.rsd"
			                                                                       : "room_small_01");
		}

		// The engine of the car the player drives: its own recorded loop (its
		// sound script's), pitched up with the speed.
		{
			const std::string driving = _inVehicle && _activeVehicle ? _activeVehicle->GetName() : std::string();
			if (driving != _engineCar)
			{
				_engineCar = driving;
				const std::string file = driving.empty() ? std::string() : _audioManager->CarSoundFile(driving);
				if (file.empty())
					_audioManager->StopEngine();
				else
					_audioManager->PlayEngine(file);
			}
			if (!_engineCar.empty() && _activeVehicle)
				_audioManager->SetEnginePitch(0.7f + std::abs(_activeVehicle->GetSpeedKmh()) / 110.0f);
			// A good jump gets a word from the driver.
			static double airborne = 0.0;
			airborne = _inVehicle && _activeVehicle && _activeVehicle->IsAirborne() ? airborne + deltaTime : 0.0;
			if (airborne > 0.8 && airborne - deltaTime <= 0.8)
				SayLine("d_air");
		}

		if (simulate)
		{
			// The car smashes what it runs into: crates, vending machines, gas
			// pumps play their breaking and stay broken. They have no body to
			// stop it (PHYS-011).
			if (_inVehicle && _activeVehicle && std::abs(_activeVehicle->GetSpeedKmh()) > 15.0f)
				if (const int broke = _level->BreakObjectsNear(_activeVehicle->GetPosition(), 2.5f); broke > 0)
				{
					AddShake(0.3f);
					_audioManager->PlayRaw(SoundGenerator::Chirp(220.0f, 80.0f, 0.2f, 44100), 44100, 1, 16);
					Log::Info("Game: {} object(s) smashed", broke);
				}
			updateCarDamage();
			updateCarDamageLook(deltaTime);
			if (_gameState == GameState::InGame)
				updateHitAndRun(deltaTime);
		}

		if (_shakeAmount > 0.0f)
		{
			float sx = (rand() % 100 - 50) * 0.001f * _shakeAmount;
			float sy = (rand() % 100 - 50) * 0.001f * _shakeAmount;
			_camera->SetPosition(_camera->GetPosition() + Vector3(sx, sy, 0));
			_shakeAmount -= static_cast<float>(deltaTime) * 5.0f;
			if (_shakeAmount < 0.0f) _shakeAmount = 0.0f;
		}

		if (_gameState == GameState::MissionComplete || _gameState == GameState::MissionFailed)
		{
			// Won on foot, the player celebrates.
			if (_missionCompleteTimer == 0.0 && _gameState == GameState::MissionComplete && !_inVehicle)
				playPlayerAnimation("victory_small", false);
			_missionCompleteTimer += deltaTime;
			if (_missionCompleteTimer > 5.0)
			{
				const bool failed = _gameState == GameState::MissionFailed;
				_gameState = GameState::InGame;
				_missionCompleteTimer = 0.0;
				if (_inVehicle)
				{
					_inVehicle = false;
					_activeVehicle = nullptr;
				}
				// A failure is retried from the last RESET_TO_HERE, not from the
				// top: losing m2's twelfth stage used to mean driving the first
				// eleven again.
				// A story mission won leads on to the next one.
				_interior.clear();
				_level->SetDrawOnly({});
				// A bonus mission over, won or lost: back to the story where it was.
				const bool bonusOver = _bonusReturn >= 0 && (!failed || !_scriptEngine->RetryFromCheckpoint());
				if (bonusOver)
				{
					const int resume = _bonusReturn;
					_bonusReturn = -1;
					startStoryMission(resume);
				}
				else if (_bonusReturn >= 0)
					_health = 100.0f; // retried from its checkpoint, just above
				else
				{
					// The level's last won leads on to the next level.
					const bool levelWon = !failed && _storyIndex >= 0 && _storyIndex + 1 >= static_cast<int>(_story.size());
					if (levelWon && levelAvailable(_levelNumber + 1))
					{
						saveStoryProgress(_levelNumber + 1, 0);
						switchLevel(_levelNumber + 1);
						if (!_story.empty())
							startStoryMission(0);
					}
					else if (levelWon)
					{
						// The last level's last mission: the story is over. It used to
						// replay that mission, for ever.
						Log::Info("Game: the story is over, level {} won", _levelNumber);
						_scriptEngine->CleanupMission();
						_gameState = GameState::MainMenu;
					}
					else if (!failed && _storyIndex >= 0 && _storyIndex + 1 < static_cast<int>(_story.size()))
					{
						saveStoryProgress(_levelNumber, _storyIndex + 1);
						startStoryMission(_storyIndex + 1);
					}
					else if (failed && _scriptEngine->RetryFromCheckpoint())
						_health = 100.0f;
					else
					{
						_scriptEngine->CleanupMission();
						if (_collectibleManager) _collectibleManager->SpawnOnPath();
						runMissionScript();
					}
				}
			}
		}

		if (_gameState == GameState::Splash)
		{
			if (intent.uiConfirm || intent.pauseToggle)
			{
				_gameState = GameState::MainMenu;
				_missionCompleteTimer = 0.0;
			}
		}

		if (_character && _character->GetPosition().Y < _fallOffY)
		{
			Log::Warn("Game: player fell off the map, respawning...");
			_health -= 20.0f;
			if (_health <= 0.0f) _health = 100.0f;
			SetPlayerPosition(_lastSafePos);
		}

		if (_activeVehicle && _inVehicle && _activeVehicle->GetPosition().Y < _fallOffY && _safeCar == _activeVehicle)
		{
			const int node = _pathGraph ? _pathGraph->FindNearestRoadNode(_safeCarPos) : -1;
			const Vector3 road = node >= 0 ? _pathGraph->GetNodes()[node].position : _safeCarPos;
			Log::Warn("Game: vehicle fell off the map, put back on the road at ({:.1f}, {:.1f}, {:.1f})", road.X, road.Y,
			          road.Z);
			_activeVehicle->Teleport(road, Quaternion::MakeFromEuler(Vector3(0.0f, _safeCarYaw, 0.0f)));
			// Known safe again only once it is back on its wheels.
			_safeCar = nullptr;
		}
		else if (_activeVehicle && _inVehicle && !_activeVehicle->IsAirborne() &&
		         _activeVehicle->GetPosition().Y > _fallOffY)
		{
			_safeCar = _activeVehicle;
			_safeCarPos = _activeVehicle->GetPosition();
			_safeCarYaw = _activeVehicle->GetRotation().Yaw();
		}

		if (_activeVehicle && _inVehicle && _activeVehicle->GetPosition().Y < _fallOffY)
		{
			Log::Warn("Game: vehicle fell off the map, exiting...");
			_inVehicle = false;
			_activeVehicle = nullptr;
			SetPlayerPosition(_lastSafePos);
		}

		if (_character && _character->GetCharacterController().onGround())
			_lastSafePos = _character->GetPosition();

		if (_showDebug)
		{
			if (_scriptEngine->IsMissionActive() && _character)
			{
				for (auto& v : _scriptEngine->GetMissionVehicles())
				{
					Vector3 dir = v->GetPosition() - _character->GetPosition();
					if (dir.LengthSquared() > 0.0f)
					{
						dir.Normalize();
						Vector3 arrowStart = _character->GetPosition() + Vector3(0, 1.5f, 0);
						Vector3 arrowEnd = arrowStart + dir * 3.0f;
						_lineRenderer->DrawLine(arrowStart, arrowEnd, Vector4(0.0f, 1.0f, 1.0f, 1.0f));
					}
					break;
				}
			}

			// The character's debug frame, where it is drawn: not over the car
			// it is sitting in.
			if (!_inVehicle)
			{
				_lineRenderer->DrawSkeleton(_character->GetPosition(), _character->GetSkeleton());
				_lineRenderer->DrawBox(_character->GetPosition(), _character->GetRotation(),
					Vector3(-0.3f, 0.0f, -0.2f), Vector3(0.3f, 1.8f, 0.3f), Vector4(0.2f, 1.0f, 0.2f, 1.0f));
			}
		}

		// Race guidance, not debug: without a marker in the world the player has no
		// way to find the next checkpoint, so a race can never be completed.
		if (_scriptEngine->IsMissionActive())
		{
			const auto& checkpoints = _scriptEngine->GetCheckpoints();
			int currentCp = _scriptEngine->GetCurrentCheckpoint();
			for (int i = 0; i < static_cast<int>(checkpoints.size()); ++i)
			{
				Vector4 cpCol = i == currentCp ? Vector4(1.0f, 1.0f, 0.0f, 1.0f) :
				                i < currentCp  ? Vector4(0.0f, 1.0f, 0.0f, 1.0f) :
				                                 Vector4(1.0f, 0.0f, 0.0f, 1.0f);
				Vector3 cpBase = checkpoints[i] + Vector3(0, 0.5f, 0);
				_lineRenderer->DrawLine(cpBase, cpBase + Vector3(0, 15.0f, 0), cpCol);

				int next = (i + 1) % checkpoints.size();
				Vector3 from = checkpoints[i] + Vector3(0, 0.1f, 0);
				Vector3 to = checkpoints[next] + Vector3(0, 0.1f, 0);
				Vector3 mid = (from + to) * 0.5f;
				int segments = 8;
				for (int s = 0; s < segments; s += 2)
				{
					float t1 = s / (float)segments;
					float t2 = (s + 1) / (float)segments;
					_lineRenderer->DrawLine(from + (to - from) * t1, from + (to - from) * t2,
						Vector4(1.0f, 1.0f, 1.0f, 0.3f));
				}
			}
		}
		_level->Update(deltaTime);
		// A stage that asks for no traffic gets empty streets: neither moved,
		// drawn nor run into.
		const bool traffic = !_scriptEngine->IsTrafficSuppressed();
		_trafficManager->SetPresent(traffic);
		if (traffic)
		{
			std::vector<Vector3> obstacles;
			// The player on foot too: the traffic waits for them to cross.
			if (_inVehicle && _activeVehicle)
				obstacles.push_back(_activeVehicle->GetPosition());
			else if (_character)
				obstacles.push_back(_character->GetPosition());
			if (const Vehicle* parked = _scriptEngine->GetPlayerCar(); parked && parked != _activeVehicle)
				obstacles.push_back(parked->GetPosition());
			for (const auto& vehicle : _scriptEngine->GetMissionVehicles())
				obstacles.push_back(vehicle->GetPosition());
			if (_hitAndRun && _hitAndRun->GetCop())
				obstacles.push_back(_hitAndRun->GetCop()->GetPosition());
			_trafficManager->SetObstacles(std::move(obstacles));
			_trafficManager->Update(deltaTime);
			for (const auto& [model, position] : _trafficManager->TakeHonks())
				if (const std::string horn = _audioManager->CarSoundFile(model, "_horn"); !horn.empty())
					_audioManager->PlayAudio(horn, position);
		}
		if (traffic && _showDebug)
			_trafficManager->Draw();
		if (simulate)
		{
			_scriptEngine->Update(deltaTime);
			if (_autopilot)
				runAutopilot(deltaTime);
		}

		// Re-enabled (REGR-001): db9c8db commented this out to isolate a crash on "press
		// up"; 9860a4f fixed that same day (uninitialised Character::_rotation feeding the
		// follow camera, plus stepUp/stepDown/onGround bugs) but the block was never restored.
		if (_character != nullptr && _gameState == GameState::InGame)
		{
			// Triggers fire on the transition, once, from wherever the player
			// actually is -- the car, when driving. This used to log "entered
			// trigger zone" every two seconds for as long as the player stood in
			// one, without saying which.
			const Vector3 playerPos = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
			                                                       : _character->GetPosition();
			// A player who came to the door at the wheel is already inside its
			// volume when they get out, and its entry has gone by: a door is
			// gone through by a player on foot standing in it, not only on
			// stepping in. Level 4 m5 waited at the Kwik-E-Mart for ever.
			if (!_inVehicle && _interior.empty() && _character)
				for (const auto& door : _level->GetTriggers())
					if ((door.type == Level::LocatorType::InteriorEntrance ||
					     (door.type == Level::LocatorType::DynamicZone && door.data.find('@') != std::string::npos)) &&
					    door.Contains(_character->GetPosition()))
					{
						enterInterior(door);
						break;
					}
			for (const auto& event : _level->UpdateTriggers(playerPos))
			{
				_scriptEngine->OnTrigger(*event.trigger, event.entered);
				// A door is an interior entrance (level 1), or a zone trigger that
				// loads an interior, "l2i04.p3d@..." (level 2 onwards).
				if (event.entered && (event.trigger->type == Level::LocatorType::InteriorEntrance ||
				                      (event.trigger->type == Level::LocatorType::DynamicZone &&
				                       event.trigger->data.find('@') != std::string::npos)))
					enterInterior(*event.trigger);
			}
			updateInterior();
			// Pedestrians walk every frame; only their arrivals are spaced out.
			// Update used to run once every half second with one frame's time,
			// so they moved, and walked, about thirty times too slowly. Nobody
			// walks about inside an interior.
			static double pedSpawnTimer = 0.0;
			pedSpawnTimer += deltaTime;
			if (_pedestrianManager && _interior.empty())
				_pedestrianManager->Update(deltaTime, _character->GetPosition());
			if (pedSpawnTimer > 0.5)
			{
				pedSpawnTimer = 0.0;
				if (_pedestrianManager && _interior.empty())
					_pedestrianManager->Spawn(_character->GetPosition(), 40.0f);
			}
			// A card found: its title, from the phrase table, which numbers each
			// level's cards from (level - 1) * 8, the eighth being a bonus card.
			if (_collectibleManager)
				if (const std::string card = _collectibleManager->TakeFoundCard(); card.size() == 6)
				{
					const int index = (card[4] - '1') * 8 + (card[5] - '1');
					_cardMessage = _textBible.Get(fmt::format("CARD_TITLE_{:02}", index));
					if (_collectibleManager->GetCardsFound() == _collectibleManager->GetCardsTotal())
					{
						const std::string& all = _textBible.Get("CARD_LEVEL_COMPLETE");
						_cardMessage += _cardMessage.empty() ? all : " - " + all;
					}
					_cardMessageUntil = _elapsedSeconds + 4.0;
					SayLine("w_card");
				}
			// Coins every frame: a car at 30 m/s is past one in a tenth of a second.
			if (_collectibleManager && _inVehicle && _activeVehicle &&
			    _collectibleManager->TakeWrench(_activeVehicle->GetPosition()))
			{
				Log::Info("Game: a wrench, the car is repaired ({:.0f} -> 100)", _health);
				_health = 100.0f;
			}
			updateGags();
			// The wasp cameras: a jump at one, or a car driven into it,
			// destroys it, for coins.
			if (_waspManager && _character && _interior.empty())
			{
				const bool driving = _inVehicle && _activeVehicle;
				const Vector3 at = driving ? _activeVehicle->GetPosition() : _character->GetPosition();
				// A jump, not merely being off the ground: a teleport sets the
				// player down from a little above, and destroyed a wasp nearby.
				const bool attacking = driving ? std::abs(_activeVehicle->GetSpeedKmh()) > 10.0f
				                               : _elapsedSeconds - _lastJumpAt < 1.0;
				const auto wasps = _waspManager->Update(deltaTime, at, attacking, driving ? 3.5f : 2.5f);
				for (const auto& [sound, where] : wasps.sounds)
					_audioManager->PlayAudio(sound, where);
				// Shot: a zap, a jolt and the damage, on foot or at the wheel.
				if (wasps.damage > 0.0f)
				{
					_health = std::max(0.0f, _health - wasps.damage);
					AddShake(0.25f);
				}
				if (wasps.destroyed > 0)
				{
					_coins += wasps.coins;
					saveCoins();
					AddShake(0.4f);
					_cardMessage = fmt::format("Wasp cameras {}/{}", _waspManager->GetDestroyed(), _waspManager->GetTotal());
					_cardMessageUntil = _elapsedSeconds + 3.0;
				}
			}
			if (_collectibleManager)
			{
				const Vector3 collector = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
				                                                      : _character->GetPosition();
				_collectibleManager->Update(collector, 3.0f);
				if (const int fresh = _collectibleManager->TakeNewlyCollected(); fresh > 0)
				{
					_coins += fresh;
					saveCoins();
					Log::Debug("Game: {} coin(s) picked up, {} in the purse", fresh, _coins);
				}
			}
		}
		if (simulate)
			_scriptEngine->UpdateAI(deltaTime);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(*_window));
		ImGui::NewFrame();

		imguiMenuBar();

		//if (_character != nullptr)
		//	guiModelMenu(*_character);

		// ImGui::ShowDemoWindow();

		if (_debugResourceManagerWindowOpen)
			_resourceManager->ImGuiDebugWindow(&_debugResourceManagerWindowOpen);

		if (_debugLevelWindowOpen)
			_level->ImGuiDebugWindow(&_debugLevelWindowOpen);

		if (_debugAudioWindowOpen)
			_audioManager->DebugGUI(&_debugAudioWindowOpen);

		debugAboutMenu();
		drawControlsWindow();

		if (intent.pauseToggle)
		{
			if (_gameState == GameState::InGame)
				_gameState = GameState::Paused;
			else if (_gameState == GameState::Paused)
				_gameState = GameState::InGame;
		}

		if (intent.resetBest && _gameState == GameState::InGame)
		{
			_scriptEngine->ResetBestTime();
			Log::Info("Game: best time reset!");
		}

		if (_gameState == GameState::Paused && intent.quit)
		{
			running = false;
			Log::Info("Game: quit from pause menu");
		}

		if (intent.restart && _gameState == GameState::InGame)
		{
			Log::Info("Game: restarting mission...");
			_scriptEngine->CleanupMission();
			if (_collectibleManager) _collectibleManager->SpawnOnPath();
			if (_inVehicle) { _inVehicle = false; _activeVehicle = nullptr; }
			runMissionScript();
		}

		if (intent.toggleDebug)
		{
			_showDebug = !_showDebug;
			Log::Info("Debug draw: {}", _showDebug ? "ON" : "OFF");
		}

		if (intent.toggleHelp)
		{
			_showHelp = !_showHelp;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 8.0f, io.DisplaySize.y - 8.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(0.35f);
		if (ImGui::Begin("Camera position overlay", NULL,
		                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
		                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			auto const& camPos = _camera->GetPosition();
			auto const& camRot = _camera->GetOrientation();
			ImGui::Text("Camera Position: %s", camPos.ToString().c_str());
			ImGui::Text("Camera Orientation: %s", camRot.ToString().c_str());

			float fov = _camera->GetFOV();
			if (ImGui::SliderFloat("FOV", &fov, 0.0f, 120.0f))
				_camera->SetFOV(fov);
		}
		ImGui::End();

		// Post-FX debug controls (FX-002 vignette, etc.).
		ImGui::SetNextWindowBgAlpha(0.35f);
		ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_Once);
		// Under the minimap: ImGui's default corner sat on the HUD's coin count.
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 240.0f, 175.0f), ImGuiCond_Once);
		if (ImGui::Begin("Post-FX", NULL,
		                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderFloat("Vignette", &_vignetteIntensity, 0.0f, 1.0f);
		}
		ImGui::End();

		if (_character != nullptr)
			_character->Update(deltaTime);
		// The level's people say something to a player who walks up to them,
		// now and then: one of their recorded idle replies.
		if (_character && !_inVehicle)
			for (auto& [name, npc] : _ambientNpcs)
			{
				static std::map<std::string, double> spokeAt;
				const Vector3 gap = _character->GetPosition() - npc->GetPosition();
				if (gap.X * gap.X + gap.Z * gap.Z > 16.0f || std::abs(gap.Y) > 3.0f)
					continue;
				double& last = spokeAt.try_emplace(name, -100.0).first->second;
				if (_elapsedSeconds - last < 20.0 || _audioManager->IsDialoguePlaying())
					continue;
				last = _elapsedSeconds;
				const auto lines = _audioManager->FindSounds(name + "\\w_idlereply_");
				if (!lines.empty())
					_audioManager->QueueDialogue(lines[static_cast<std::size_t>(rand()) % lines.size()],
					                             AudioManager::DialoguePriority::Gameplay);
			}
		// Someone near the player while a line is being spoken is in the
		// conversation, and gestures.
		const bool speaking = _audioManager->IsDialoguePlaying() && _character && !_inVehicle;
		const auto talkingNear = [&](const Vector3& position) {
			return speaking && (position - _character->GetPosition()).Length() < 8.0f;
		};
		for (auto& [car, driver] : _carDrivers)
			if (driver)
				driver->Update(deltaTime);
		for (auto& giver : _bonusGivers)
			if (giver.npc)
			{
				if (_character)
				{
					const Vector3 toPlayer = _character->GetPosition() - giver.position;
					if (toPlayer.X * toPlayer.X + toPlayer.Z * toPlayer.Z < 100.0f)
						giver.npc->SetRotation(Quaternion::MakeFromEuler(
						    Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f, 0.0f)));
				}
				animateNpc(*giver.npc, talkingNear(giver.npc->GetPosition()));
				giver.npc->Update(deltaTime);
			}
		for (auto* people : {&_npcs, &_ambientNpcs})
		for (auto& [name, npc] : *people)
		{
			animateNpc(*npc, talkingNear(npc->GetPosition()));
			// Someone the player walks up to turns to face them. The model faces
			// -Z, hence the half turn.
			if (_character && !_inVehicle)
			{
				const Vector3 toPlayer = _character->GetPosition() - npc->GetPosition();
				if (toPlayer.X * toPlayer.X + toPlayer.Z * toPlayer.Z < 100.0f)
					npc->SetRotation(Quaternion::MakeFromEuler(
					    Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f, 0.0f)));
			}
			npc->Update(deltaTime);
		}

		ImGui::Render();

		int viewportWidth = (int)io.DisplaySize.x;
		int viewportHeight = (int)io.DisplaySize.y;

		// Render the 3D scene into an offscreen FBO so a post-process pass can
		// sample it (FX-000). The HUD/ImGui are drawn afterwards on the
		// backbuffer so they are not filtered.
		ensureFullscreenQuad();
		ensureSceneFBO(viewportWidth, viewportHeight);
		_sceneFBO->Bind();

		glViewport(0, 0, viewportWidth, viewportHeight);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		// SH&R geometry uses Direct3D winding (clockwise front faces); OpenGL
		// defaults to CCW, which back-face-culled the ground/road away. Match
		// the source winding so single-sided world geometry renders.
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CW);
		glClearColor(0.62f, 0.78f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		_camera->SetAspectRatio(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));

		Matrix4x4 viewMatrix = _camera->GetViewMatrix();
		Matrix4x4 projMatrix = _camera->GetProjectionMatrix();
		Matrix4x4 viewProjection = projMatrix * viewMatrix;

		if (_level != nullptr)
			_level->Draw(viewProjection);

		// At the wheel, the player sits in the driver's seat, idling as drivers
		// do (in_car_idle); the controller keeps him where he got in, and
		// takes him back as he gets out.
		if (_character != nullptr && _inVehicle && _activeVehicle)
		{
			playPlayerAnimation("in_car_idle", true);
			if (seatDriver(*_character, *_activeVehicle))
			{
				glDisable(GL_CULL_FACE);
				_character->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
				glEnable(GL_CULL_FACE);
			}
		}
		drawCarDrivers(viewProjection);
		if (_character != nullptr && !_inVehicle)
		{
			// the skinned character mesh has inconsistent winding; with the
			// world's clockwise back-face culling, parts (e.g. an arm) get
			// culled away. Render the character double-sided.
			glDisable(GL_CULL_FACE);
			_character->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
			glEnable(GL_CULL_FACE);
		}

		// The mission's NPCs and the level's people, double-sided for the same
		// reason as the player; someone the mission has placed is not also
		// standing in their usual spot.
		glDisable(GL_CULL_FACE);
		for (auto& [name, npc] : _npcs)
			npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
		for (auto& [name, npc] : _ambientNpcs)
			if (_npcs.find(name) == _npcs.end())
				npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
		if (_bonusReturn < 0)
			for (auto& giver : _bonusGivers)
				if (giver.npc)
					giver.npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
		glEnable(GL_CULL_FACE);

		if (_scriptEngine->IsMissionActive())
		{
			for (auto& v : _scriptEngine->GetMissionVehicles())
			{
				v->Update(deltaTime);
				if (v->HasModel())
				{
					// A car is a CompositeModel of plain P3D Meshes -- position,
					// uv, lightmap uv, colour -- so it takes the world shader, as
					// the level's own composite props do. It was drawn with the
					// skinning shader, which reads normals and bone weights the
					// mesh never supplies: the vertices came out degenerate and
					// smeared triangles out from the middle of the screen. And only
					// the translucent pass was drawn, so the car itself never
					// appeared.
					GL::ShaderProgram& worldShader = _level->GetWorldShader();
					v->Draw(viewProjection, worldShader, true);
					v->Draw(viewProjection, worldShader, false);
					v->DrawLights(viewProjection, _level->GetBillboardShader());
				}
				else
				{
					_carMesh->Draw(*_meshShader,
						Matrix4x4::MakeTranslate(v->GetPosition()) * Matrix4x4(v->GetRotation()),
						viewProjection, Vector4(0.2f, 0.5f, 1.0f, 1.0f));
					Vector3 beaconBase = v->GetPosition() + Vector3(0, 1.0f, 0);
					_lineRenderer->DrawLine(beaconBase, beaconBase + Vector3(0, 20.0f, 0),
						Vector4(0.0f, 1.0f, 1.0f, 1.0f));

					if (_inVehicle && v.get() == _activeVehicle)
					{
						float speed = v->GetSpeedKmh();
						Vector3 exhaustPos = v->GetPosition() + v->GetRotation() * Vector3(-0.5f, -0.2f, -2.2f);
						Vector3 exhaustDir = v->GetRotation() * Vector3(0, 0.1f, -0.5f);
						for (int e = 0; e < 3; ++e)
						{
							Vector3 offset(rand() % 10 * 0.02f - 0.1f, rand() % 10 * 0.02f, rand() % 10 * 0.02f);
							_lineRenderer->DrawLine(exhaustPos + offset,
								exhaustPos + exhaustDir * (0.5f + speed * 0.02f) + offset,
								Vector4(0.5f, 0.5f, 0.5f, 1.0f));
						}
					}
				}
			}
		}

		// Traffic with a model is drawn as itself in either view; the debug view
		// (on by default) otherwise shows the traffic only as wire boxes.
		if (!_scriptEngine->IsTrafficSuppressed() && _interior.empty())
		{
			_trafficManager->DrawModels(_level->GetWorldShader(), viewProjection);
			// Blending on for the glows, then as it was.
			{
				const GLboolean blending = glIsEnabled(GL_BLEND);
				GLint source = GL_SRC_ALPHA, destination = GL_ONE_MINUS_SRC_ALPHA;
				glGetIntegerv(GL_BLEND_SRC_RGB, &source);
				glGetIntegerv(GL_BLEND_DST_RGB, &destination);
				glEnable(GL_BLEND);
				_trafficManager->DrawBrakeLights(_level->GetBillboardShader(), viewProjection);
				glBlendFunc(source, destination);
				if (!blending)
					glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_TRUE);
			}
			if (!_showDebug)
				_trafficManager->DrawSolid(*_carMesh, *_meshShader, viewProjection);
		}

		// Pedestrians with a body are drawn as people in either view, like the
		// traffic; the rest are capsules outside the debug view.
		if (_pedestrianManager && _interior.empty())
		{
			if (!_showDebug)
				_pedestrianManager->Draw(*_meshShader, *_playerMesh, viewProjection);
			glDisable(GL_CULL_FACE); // skinned characters, as for the player
			_pedestrianManager->DrawBodies(viewProjection, *_skinShaderProgram, *_resourceManager);
			glEnable(GL_CULL_FACE);
		}

		if (_collectibleManager)
			_collectibleManager->Draw(_level->GetWorldShader(), viewProjection, static_cast<float>(_elapsedSeconds));

		drawGags(viewProjection);
		if (_waspManager && _interior.empty())
			_waspManager->Draw(_level->GetWorldShader(), _level->GetBillboardShader(), viewProjection);

		if (_scriptEngine->IsMissionActive() && !_scriptEngine->GetCheckpoints().empty())
		{
			_carMesh->Draw(*_meshShader,
				Matrix4x4::MakeTranslate(_scriptEngine->GetAIPosition()) *
					Matrix4x4(_scriptEngine->GetAIRotation()),
				viewProjection, Vector4(1.0f, 0.2f, 0.2f, 1.0f));
		}

		if (_hitAndRun && _hitAndRun->GetCop() && _interior.empty())
		{
			GL::ShaderProgram& worldShader = _level->GetWorldShader();
			_hitAndRun->GetCop()->Draw(viewProjection, worldShader, true);
			_hitAndRun->GetCop()->Draw(viewProjection, worldShader, false);
		}

		if (_scriptEngine->IsMissionActive())
		{
			auto* chase = _scriptEngine->GetChaseManager();
			if (chase)
			{
				for (auto& cop : chase->GetCopCars())
				{
					_carMesh->Draw(*_meshShader,
						Matrix4x4::MakeTranslate(cop->GetPosition()) *
							Matrix4x4(cop->GetRotation()),
						viewProjection, Vector4(0.1f, 0.1f, 0.1f, 1.0f));
				}
			}
		}

		glDisable(GL_DEPTH_TEST);
		if (_clean)
			_lineRenderer->Discard();
		else
			_lineRenderer->Flush(viewProjection);
		glEnable(GL_DEPTH_TEST);

		// Scene complete: blit the FBO to the backbuffer through the
		// post-process shader (passthrough when all effects are neutralised).
		blitSceneToBackbuffer(viewportWidth, viewportHeight);

		Matrix4x4 proj = Matrix4x4::MakeOrtho(0.0f, viewportWidth, viewportHeight, 0.0f);

		if (_textureFontP3D != nullptr)
		{
			auto font = _resourceManager->GetFont("boulder_16");
			const bool pad = intent.gamepadConnected;

			HudState hud;
			hud.viewportWidth = static_cast<float>(viewportWidth);
			hud.viewportHeight = static_cast<float>(viewportHeight);
			hud.fps = _clean ? 0 : static_cast<int>(timer.GetFps());
			hud.health = _health;
			hud.inVehicle = _inVehicle;
			hud.boost = intent.boostHeld ? 0.0f : 1.0f; // placeholder, see UI-HUD

			if (_inVehicle && _activeVehicle)
				hud.speedKmh = _activeVehicle->GetSpeedKmh();

			if (_collectibleManager)
			{
				hud.coins = _coins;
				if (!_alertMessage.empty() && _elapsedSeconds < _alertUntil && hud.banner.empty())
				{
					hud.banner = _alertMessage;
					hud.bannerColour = Vector4(1.0f, 0.25f, 0.2f, 1.0f);
				}
				if (!_cardMessage.empty() && _elapsedSeconds < _cardMessageUntil && hud.banner.empty())
				{
					hud.banner = _cardMessage;
					hud.bannerColour = Vector4(0.4f, 0.7f, 1.0f, 1.0f);
				}
				hud.coinsTotal = 0;
			}
			// A race or bonus mission on offer, at hand.
			if (const BonusGiver* giver = bonusGiverNear())
			{
				const std::string& title = _textBible.Get(fmt::format("MISSION_TITLE_L{}_M{}", _levelNumber,
				    giver->mission == "sr1" ? 8 : giver->mission == "sr2" ? 9 : giver->mission == "sr3" ? 10 : giver->mission == "gr1" ? 11 : 12));
				hud.hints.push_back(fmt::format("E: {}", title.empty() ? giver->mission : title));
			}
			hud.cardsTotal = 7;
			if (_collectibleManager)
				hud.cards = _collectibleManager->GetCardsFound();

			if (_scriptEngine->IsMissionActive())
			{
				hud.missionActive = true;
				// The mission's title card, for its first few seconds.
				constexpr double kTitleCardSeconds = 4.0;
				if (_gameState == GameState::InGame && _elapsedSeconds - _missionLoadedAt < kTitleCardSeconds)
				{
					hud.banner = _scriptEngine->GetMissionTitle();
					hud.bannerColour = Vector4(1.0f, 0.84f, 0.0f, 1.0f);
					hud.bannerDetail = _scriptEngine->GetMissionTitle(true);
					// A level's first story mission also says which level this is.
					if (_storyIndex == 0)
						if (const std::string& level = _textBible.Get(fmt::format("LEVEL_{}", _levelNumber)); !level.empty())
							hud.banner = hud.banner.empty() ? level : level + " - " + hud.banner;
				}
				hud.countdown = _scriptEngine->GetCountdownText();
				hud.objective = _scriptEngine->GetObjectiveText();
				hud.timeRemaining = _scriptEngine->GetStageTimeRemaining();
				hud.lap = _scriptEngine->GetCurrentLap();
				hud.totalLaps = _scriptEngine->GetTotalLaps();
				if (!_scriptEngine->GetRacePosition(hud.racePosition, hud.raceField))
					hud.racePosition = hud.raceField = 0;
				hud.checkpoint = _scriptEngine->GetCurrentCheckpoint();
				hud.totalCheckpoints = static_cast<int>(_scriptEngine->GetCheckpoints().size());

				for (const auto& v : _scriptEngine->GetMissionVehicles())
					hud.missionBlips.emplace_back(v->GetPosition().X, v->GetPosition().Z);

				const auto& cps = _scriptEngine->GetCheckpoints();
				for (const auto& cp : cps)
					hud.checkpointBlips.emplace_back(cp.X, cp.Z);

				// Wherever the objective wants the player -- a checkpoint, a
				// destination, an item to pick up, someone to talk to -- not only
				// a race's next checkpoint, which left every other objective
				// without a marker.
				Vector3 target;
				if (_scriptEngine->GetObjectiveTarget(target))
				{
					const Vector3 from = _inVehicle && _activeVehicle
					                         ? _activeVehicle->GetPosition()
					                         : (_character ? _character->GetPosition() : Vector3::Zero);
					hud.hasNextCheckpoint = true;
					hud.nextCheckpointXZ = Vector2(target.X, target.Z);
					hud.nextCheckpointDistance = (target - from).Length();
					hud.nextCheckpointLabel = _scriptEngine->GetObjectiveTargetLabel();
				}
			}

			if (_hitAndRun && (_hitAndRun->GetMeter() > 0.0f || _hitAndRun->IsChasing()))
			{
				hud.chaseActive = true;
				hud.heat = _hitAndRun->GetMeter() / 10.0f;
				if (const Vehicle* cop = _hitAndRun->GetCop())
					hud.policeBlips.emplace_back(cop->GetPosition().X, cop->GetPosition().Z);
			}
			auto* chase = _scriptEngine->GetChaseManager();
			if (chase != nullptr && chase->IsActive())
			{
				hud.chaseActive = true;
				hud.heat = chase->GetHeat();
				hud.busted = chase->IsBusted();
				if (hud.busted)
					hud.banner = "BUSTED!";
				for (const auto& cop : chase->GetCopCars())
					hud.policeBlips.emplace_back(cop->GetPosition().X, cop->GetPosition().Z);
			}

			const Vector3 playerPos = _inVehicle && _activeVehicle
			                              ? _activeVehicle->GetPosition()
			                              : (_character ? _character->GetPosition() : Vector3::Zero);
			hud.playerXZ = Vector2(playerPos.X, playerPos.Z);
			// The way the player faces: the car's heading at the wheel (it used
			// to be the on-foot camera's, whatever the car did).
			hud.playerHeading = _inVehicle && _activeVehicle ? _activeVehicle->GetRotation().Yaw() : _camYaw;
			if (_pathGraph)
				for (const auto& node : _pathGraph->GetNodes())
				{
					const float dx = node.position.X - playerPos.X, dz = node.position.Z - playerPos.Z;
					if (!node.shortcut && dx * dx + dz * dz < hud.radarRange * hud.radarRange * 2.0f)
						hud.roadPoints.emplace_back(node.position.X, node.position.Z);
				}

			if (_inVehicle)
			{
				hud.hintColour = Vector4(0.5f, 0.8f, 1.0f, 1.0f);
				hud.hints.push_back(fmt::format("{}: Drive | {}: Boost | {}: Horn | {}: Exit",
					ControlLabel(GameAction::MoveForward, pad), ControlLabel(GameAction::Boost, pad),
					ControlLabel(GameAction::Honk, pad), ControlLabel(GameAction::Interact, pad)));
			}
			else if (_gameState == GameState::InGame)
			{
				hud.hints.push_back(fmt::format("{}: Move | {}: Action | {}: Restart | {}: Pause",
					ControlLabel(GameAction::MoveForward, pad), ControlLabel(GameAction::Interact, pad),
					ControlLabel(GameAction::RestartMission, pad), ControlLabel(GameAction::PauseToggle, pad)));

				if (_showHelp)
				{
					hud.hints.push_back("=== CONTROLS ===");
					hud.hints.push_back(fmt::format("{}: Move/Drive", ControlLabel(GameAction::MoveForward, pad)));
					hud.hints.push_back(fmt::format("{}: Action | {}: Boost",
						ControlLabel(GameAction::Interact, pad), ControlLabel(GameAction::Boost, pad)));
					hud.hints.push_back(fmt::format("{}: Horn | {}: Restart | {}: Reset",
						ControlLabel(GameAction::Honk, pad), ControlLabel(GameAction::RestartMission, pad),
						ControlLabel(GameAction::ResetBestTime, pad)));
					hud.hints.push_back(fmt::format("{}: Jump | {}: Debug | {}: Help",
						ControlLabel(GameAction::VehicleJump, pad), ControlLabel(GameAction::ToggleDebugDraw, pad),
						ControlLabel(GameAction::ToggleHelp, pad)));
					hud.hints.push_back(fmt::format("{}: Pause | {}: Fly",
						ControlLabel(GameAction::PauseToggle, pad), ControlLabel(GameAction::CameraLook, pad)));
				}

				if (_character && _scriptEngine->IsMissionActive())
				{
					for (const auto& v : _scriptEngine->GetMissionVehicles())
					{
						const float dist = (v->GetPosition() - _character->GetPosition()).Length();
						hud.hints.push_back(dist < 5.0f ? "Press E to enter vehicle!"
						                                : fmt::format("Vehicle: {:.0f}m", dist));
						break;
					}
				}
			}

			if (_gameState == GameState::Splash)
				hud.banner = "PRESS ENTER TO START";

			_hud->Draw(sprites, font, hud);


			if (_gameState == GameState::Paused)
			{
				const float pauseLineHeight = font != nullptr ? font->GetHeight() : 16.0f;
				const float pauseSpacing = pauseLineHeight + 14.0f;
				const float pauseTop = viewportHeight / 2.0f - 40.0f;

				sprites.DrawText(font, "PAUSED",
					Vector2(centredTextX(font, "PAUSED", viewportWidth), pauseTop - pauseSpacing * 1.5f),
					Vector4(1.0f, 1.0f, 0.0f, 1.0f));

				for (size_t i = 0; i < _pauseMenu->GetButtons().size(); ++i)
				{
					const auto& btn = _pauseMenu->GetButtons()[i];
					const float bw = font != nullptr ? font->MeasureWidth(btn.label) : btn.width;
					_pauseMenu->SetButtonRect(i, centredTextX(font, btn.label, viewportWidth),
					                          pauseTop + i * pauseSpacing, bw, pauseLineHeight);
				}
				_pauseMenu->Update(intent.mouseX, intent.mouseY);
				for (const auto& btn : _pauseMenu->GetButtons())
				{
					Vector4 col = btn.hovered ? Vector4(1.0f, 0.3f, 0.3f, 1.0f)
					                         : Vector4(0.5f, 0.5f, 0.5f, 1.0f);
					sprites.DrawText(font, btn.label, Vector2(btn.x, btn.y), col);
				}
				if (intent.uiClick)
					_pauseMenu->CheckClick(intent.mouseX, intent.mouseY);
				sprites.Flush(proj);
				_window->Swap();
				continue;
			}

			if (_gameState == GameState::MissionComplete)
			{
				if (_scriptEngine->IsNewRecord())
				{
					sprites.DrawText(font, "NEW RECORD!",
						Vector2((viewportWidth / 2.0f) - 60, viewportHeight / 2.0f - 70),
						Vector4(1.0f, 0.84f, 0.0f, 1.0f));
				}

				// In the game's words, when the phrase table is there.
				const std::string& complete = _textBible.Get("MISSION_COMPLETE");
				sprites.DrawText(font, complete.empty() ? "MISSION COMPLETE!" : complete,
					Vector2((viewportWidth / 2.0f) - 100, viewportHeight / 2.0f - 50),
					Vector4(1.0f, 0.84f, 0.0f, 1.0f));
				// The level's last story mission: the level is won too.
				if (_storyIndex >= 0 && _storyIndex + 1 == static_cast<int>(_story.size()))
				{
					const std::string& level = _textBible.Get("LEVEL_COMPLETE");
					const std::string text = level.empty() ? "Level Complete" : level;
					sprites.DrawText(font, text, Vector2(centredTextX(font, text, viewportWidth), viewportHeight / 2.0f - 90),
					                 Vector4(1.0f, 0.5f, 0.1f, 1.0f));
				}

				const float elapsed = _scriptEngine->GetMissionTime();
				std::string stats = fmt::format("Time: {:.1f}s", elapsed);
				sprites.DrawText(font, stats,
					Vector2((viewportWidth / 2.0f) - 50, viewportHeight / 2.0f - 10),
					Vector4(1.0f, 1.0f, 1.0f, 1.0f));

				float best = _scriptEngine->GetBestTime();
				if (best < 999.0f)
				{
					std::string bestText = fmt::format("Best: {:.1f}s", best);
					sprites.DrawText(font, bestText,
						Vector2((viewportWidth / 2.0f) - 40, viewportHeight / 2.0f + 10),
						elapsed <= best ? Vector4(1.0f, 0.84f, 0.0f, 1.0f) : Vector4(0.5f, 0.5f, 0.5f, 1.0f));
				}

				int nextIn = 5 - static_cast<int>(_missionCompleteTimer);
				std::string nextText = fmt::format("Next in {}...", nextIn);
				sprites.DrawText(font, nextText,
					Vector2((viewportWidth / 2.0f) - 60, viewportHeight / 2.0f + 40),
					Vector4(0.6f, 0.6f, 0.6f, 1.0f));
			}

			if (_gameState == GameState::MissionFailed)
			{
				const std::string& term = _textBible.Get("MISSION_FAILED");
				const std::string failed = term.empty() ? "MISSION FAILED!" : term;
				sprites.DrawText(font, failed, Vector2(centredTextX(font, failed, viewportWidth), viewportHeight / 2.0f - 20),
					Vector4(1.0f, 0.2f, 0.2f, 1.0f));
				// Why, and the game's advice for it.
				const std::string reason = _scriptEngine->GetFailureReason();
				if (!reason.empty())
					sprites.DrawText(font, reason, Vector2(centredTextX(font, reason, viewportWidth), viewportHeight / 2.0f + 10),
					                 Vector4(1.0f, 0.6f, 0.6f, 1.0f));
				const std::string hint = _scriptEngine->GetFailureHint();
				if (!hint.empty())
					sprites.DrawText(font, hint, Vector2(centredTextX(font, hint, viewportWidth), viewportHeight / 2.0f + 40),
					                 Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				int nextIn = 5 - static_cast<int>(_missionCompleteTimer);
				std::string nextText = fmt::format("Retrying in {}...", nextIn);
				sprites.DrawText(font, nextText,
					Vector2((viewportWidth / 2.0f) - 60, viewportHeight / 2.0f + 70),
					Vector4(0.6f, 0.6f, 0.6f, 1.0f));
			}
		}

		if (_gameState == GameState::MainMenu)
		{
			auto font = _resourceManager->GetFont("boulder_16");
			SpriteBatch menuSprites;
			const float menuLineHeight = font != nullptr ? font->GetHeight() : 16.0f;
			const float menuSpacing = menuLineHeight + 14.0f;
			const float menuTop = viewportHeight / 2.0f - 40.0f;

			menuSprites.DrawText(font, "donut",
				Vector2(centredTextX(font, "donut", viewportWidth), menuTop - menuSpacing * 1.5f),
				Vector4(1.0f, 0.84f, 0.0f, 1.0f));

			// lay out first, then hit-test: hover and click must use the rects the
			// labels are actually drawn at, not the placeholder passed to AddButton
			for (size_t i = 0; i < _mainMenu->GetButtons().size(); ++i)
			{
				const auto& btn = _mainMenu->GetButtons()[i];
				const float bw = font != nullptr ? font->MeasureWidth(btn.label) : btn.width;
				_mainMenu->SetButtonRect(i, centredTextX(font, btn.label, viewportWidth),
				                         menuTop + i * menuSpacing, bw, menuLineHeight);
			}
			_mainMenu->Update(intent.mouseX, intent.mouseY);
			for (const auto& btn : _mainMenu->GetButtons())
			{
				Vector4 col = btn.hovered ? Vector4(1.0f, 0.84f, 0.0f, 1.0f)
				                         : Vector4(0.8f, 0.8f, 0.8f, 1.0f);
				menuSprites.DrawText(font, btn.label, Vector2(btn.x, btn.y), col);
			}
			if (intent.mouseLeftClick)
				_mainMenu->CheckClick(intent.mouseX, intent.mouseY);
			menuSprites.Flush(proj);
		}

		sprites.Flush(proj);
		if (_gameState == GameState::Splash)
			_frontend->Draw(proj);

		if (!_clean)
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		_window->Swap();
	}
}

void Game::guiModelMenu(Character& character)
{
	ImGui::Begin(fmt::format("Character: {0}", character.GetName()).c_str());

	if (ImGui::BeginCombo("Model", character.GetModelName().c_str()))
	{
		for (auto const& model : models)
		{
			const bool is_selected = character.GetModelName() == model.first;
			if (ImGui::Selectable(model.first.c_str(), is_selected))
			{
				character.LoadModel(model.first);
				character.LoadAnimations(model.second);
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Animation", character.GetAnimName().c_str()))
	{
		for (auto const& anim : character.GetAnimations())
		{
			const bool is_selected = character.GetAnimName() == anim.first;
			if (ImGui::Selectable(anim.first.c_str(), is_selected))
			{
				character.SetAnimation(anim.first);
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	Vector3 pos = character.GetPosition();
	if (ImGui::InputFloat3("Position", &pos[0]))
		character.SetPosition(pos);

	ImGui::End();
}

void Game::imguiMenuBar()
{
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("Warp")) {
		// if (ImGui::BeginMenu("Level 1"))
		// {
		for (auto const& location : _teleports)
		{
			if (ImGui::MenuItem(location.first.c_str()))
			{
				const Vector3& dest = location.second;
				//_worldPhysics->GetCharacterController()->SetPosition(dest);
				_camera->SetPosition(dest);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(location.second.ToString().c_str());
				ImGui::EndTooltip();
			}
		}
		//	ImGui::EndMenu();
		//}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Physics"))
	{
		PhysicsDebugDrawMode mode = _worldPhysics->GetDebugDrawMode();

		ImGui::CheckboxFlags("Draw Wireframe", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawWireframe));
		ImGui::CheckboxFlags("Draw AABB", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawAABB));
		ImGui::CheckboxFlags("Draw Features Text", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawFeaturesText));
		ImGui::CheckboxFlags("Draw Contact Points", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawContactPoints));
		ImGui::CheckboxFlags("Draw Text", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawText));
		ImGui::CheckboxFlags("Fast Wireframe", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::FastWireframe));
		ImGui::CheckboxFlags("Draw Normals", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawNormals));
		ImGui::CheckboxFlags("Draw Frames", reinterpret_cast<unsigned int*>(&mode),
		                     static_cast<unsigned int>(PhysicsDebugDrawMode::DrawFrames));

		_worldPhysics->SetDebugDrawMode(mode);

		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("Audio"))
		_debugAudioWindowOpen = true;

	if (ImGui::MenuItem("Resource Manager"))
		_debugResourceManagerWindowOpen = true;

	if (ImGui::MenuItem("Level Entities"))
		_debugLevelWindowOpen = true;

	if (ImGui::MenuItem("Controls"))
		_showControls = true;

	if (ImGui::MenuItem("About"))
		_debugAboutWindowOpen = true;

	ImGui::EndMainMenuBar();
}

void Game::debugDrawP3D(const P3D::P3DFile& p3d)
{
	ImGui::SetNextWindowSize(ImVec2(330, 400), ImGuiCond_Once);
	ImGui::Begin(p3d.GetFileName().c_str());

	ImGui::SetNextItemOpen(true); // open the root node
	const auto traverse_chunk = [&](const auto& self, const P3D::P3DChunk& chunk) -> void {
		std::ostringstream name;
		name << chunk.GetType();

		if (ImGui::TreeNode(&chunk, "%s", name.str().c_str()))
		{
			ImGui::TextDisabled("Type ID: %x", static_cast<uint32_t>(chunk.GetType()));
			ImGui::TextDisabled("Data Size: %zu bytes", chunk.GetData().size());

			if (!chunk.GetData().empty() && ImGui::TreeNode("Data"))
			{
				const auto& data = chunk.GetData();
				for (std::size_t i = 0; i < data.size(); i += 16)
				{
					ImGui::Text("%04zx: ", i);
					ImGui::SameLine();
					for (std::size_t j = 0; j < 16 && i + j < data.size(); ++j)
					{
						ImGui::Text("%02x ", data[i + j]);
						ImGui::SameLine();
					}
					ImGui::NewLine();
				}
				ImGui::TreePop();
			}

			for (auto& child : chunk.GetChildren()) { self(self, *child); }

			ImGui::TreePop();
		}
	};

	traverse_chunk(traverse_chunk, p3d.GetRoot());

	ImGui::End();
}

const std::string aboutText = R"(donut - copyright 2019-2020 by [Matt Stevens](https://github.com/handsomematt), [Michael Johnson](https://github.com/aylaylay)
# License
donut is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

donut is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see [<https://www.gnu.org/licenses/>](https://www.gnu.org/licenses/).
# Open Source Software
This software uses open source components, whose copyright and other rights belong to their respective owners:

* sdl2 [https://www.libsdl.org/](https://www.libsdl.org/)
* glad [https://github.com/Dav1dde/glad](https://github.com/Dav1dde/glad)
* bullet [https://github.com/bulletphysics/bullet3](https://github.com/bulletphysics/bullet3)
* openal-soft [https://github.com/kcat/openal-soft](https://github.com/kcat/openal-soft)
* dear imgui [https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)
* imgui_markdown [https://github.com/juliettef/imgui_markdown](https://github.com/juliettef/imgui_markdown)
* fmt [https://github.com/fmtlib/fmt/](https://github.com/fmtlib/fmt/)
* stb [https://github.com/nothings/stb](https://github.com/nothings/stb)
)";

void LinkCallback(ImGui::MarkdownLinkCallbackData data_)
{
	std::string url(data_.link, data_.linkLength);
	if (!data_.isImage)
	{
		#ifdef _WIN32
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		#else
		std::string xdgOpenURL = "xdg-open " + url;
		system(xdgOpenURL.c_str());
		#endif
	}
}

static ImGui::MarkdownConfig mdConfig { LinkCallback };

void Game::debugAboutMenu()
{
	if (!_debugAboutWindowOpen)
		return;

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
	const ImVec2 windowSize(600.0f, 520.0f);

	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
	ImGui::SetNextWindowPos(center, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin("About", &_debugAboutWindowOpen, ImGuiWindowFlags_NoResize))
	{
		ImGui::Markdown(aboutText.c_str(), aboutText.length(), mdConfig);
	}
	ImGui::End();
}

void Game::drawControlsWindow()
{
	if (!_showControls)
		return;

	// which action (if any) is waiting for a captured input this session
	static GameAction capturingAction = GameAction::Count;

	// pick up a freshly captured input and rebind+save
	if (Input::IsCapturing())
	{
		InputSource captured;
		if (Input::ConsumeCapturedInput(captured))
		{
			if (capturingAction != GameAction::Count)
			{
				Input::RebindAction(capturingAction, captured);
				Input::SaveKeymap("keymap.conf");
			}
			capturingAction = GameAction::Count;
		}
	}
	else
	{
		capturingAction = GameAction::Count; // capture ended or was cancelled (Esc)
	}

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowSize(ImVec2(440.0f, 540.0f), ImGuiCond_Once);
	ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin("Controls", &_showControls))
	{
		ImGui::TextWrapped("Click a binding, then press a key, mouse button or gamepad input. Esc cancels. Changes are saved to keymap.conf.");
		if (ImGui::Button("Reset to defaults"))
		{
			Input::ResetKeymapToDefault();
			Input::SaveKeymap("keymap.conf");
		}
		ImGui::SameLine();
		if (ImGui::Button("Save now"))
			Input::SaveKeymap("keymap.conf");
		ImGui::Separator();

		const Keymap& km = Input::GetKeymap();
		for (std::size_t i = 0; i < GameActionCount; ++i)
		{
			const auto action = static_cast<GameAction>(i);
			ImGui::PushID(static_cast<int>(i));
			ImGui::TextUnformatted(std::string(ToString(action)).c_str());
			ImGui::SameLine(230.0f);

			std::string label;
			for (const auto& src : BindingsFor(km, action))
			{
				if (!label.empty())
					label += " / ";
				label += KeymapConfig::HumanLabel(src);
			}
			if (label.empty())
				label = "(unbound)";

			const bool isCapturing = Input::IsCapturing() && capturingAction == action;
			if (isCapturing)
				label = "press input...";

			if (ImGui::Button(label.c_str(), ImVec2(180.0f, 0.0f)))
			{
				if (!Input::IsCapturing())
				{
					Input::BeginCapture();
					capturingAction = action;
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
}

} // namespace Donut
