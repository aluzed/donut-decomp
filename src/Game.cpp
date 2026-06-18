// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Game.h"

#include "AnimCamera.h"
#include "Audio/SoundGenerator.h"
#include "Audio/AudioManager.h"
#include "Character.h"
#include "Core/FpsTimer.h"
#include "Core/Math/Math.h"
#include "FreeCamera.h"
#include "FrontendProject.h"
#include "Game/CollectibleManager.h"
#include "Input/Input.h"
#include "AI/PathGraph.h"
#include "AI/PedestrianManager.h"
#include "AI/TrafficManager.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Physics/BulletCast.h"
#include "Physics/WorldPhysics.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"
#include "Render/Font.h"
#include "Render/LineRenderer.h"
#include "Render/OpenGL/FrameBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/Shader.h"
#include "Render/SimpleMesh.h"
#include "Render/SkinModel.h"
#include "Render/SpriteBatch.h"
#include "UI/GameMenu.h"
#include "Render/imgui/imgui.h"
#include "Render/imgui/imgui_impl_opengl3.h"
#include "Render/imgui/imgui_impl_sdl.h"
#include "Render/imgui/imgui_markdown.h"
#include "ResourceManager.h"
#include "Scripting/Commands.h"
#include "Scripting/ScriptEngine.h"
#include "Window.h"

#include <SDL.h>
#include <fmt/format.h>

#include <array>
#include <iostream>
#include <sstream>
#include <string>

namespace Donut
{

Game* Game::instance = nullptr;

#if _DEBUG
const std::string kBuildString = "DEBUG BUILD";
#else
const std::string kBuildString = "Release Build";
#endif

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                                const void* userParam)
{
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
	        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

Game::Game(int argc, char** argv)
{
	instance = this; // global static :D

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
	_scriptEngine = std::make_unique<ScriptEngine>(*this);

	if (FileSystem::exists("./assets/audio/ambience/ambience.rcf"))
		_audioManager->LoadRCF("./assets/audio/ambience/ambience.rcf");

	if (FileSystem::exists("./art/frontend/scrooby2/resource/fonts/font0_16.p3d"))
	{
		const P3D::P3DFile p3dFont("./art/frontend/scrooby2/resource/fonts/font0_16.p3d");
		_textureFontP3D = P3D::TextureFont::Load(*p3dFont.GetRoot().GetChildren().at(0));

		auto font = std::make_unique<Font>(*_textureFontP3D);
		_resourceManager->AddFont(_textureFontP3D->GetName(), std::move(font));
	}

	_level = std::make_unique<Level>();
	_level->LoadP3D("L1_TERRA.p3d");

	// simpsons house
	_level->LoadP3D("l1z1.p3d");
	_level->LoadP3D("l1r1.p3d");
	_level->LoadP3D("l1r7.p3d");
	_level->LoadP3D("l1r2.p3d");
	_level->LoadP3D("l1r3.p3d");
	_level->LoadP3D("l1r4a.p3d");
	_level->LoadP3D("l1r6.p3d");
	_level->LoadP3D("l1z2.p3d");
	_level->LoadP3D("l1z3.p3d");
	_level->LoadP3D("l1z4.p3d");
	_level->LoadP3D("l1z6.p3d");
	_level->LoadP3D("l1z7.p3d");

	_level->DynaLoadData("l1z1.p3d;l1r1.p3d;l1r7.p3d;");
	_pathGraph = std::make_unique<PathGraph>(*_level);
	_trafficManager = std::make_unique<TrafficManager>(*_level, *_lineRenderer, *_pathGraph);
	_pedestrianManager = std::make_unique<PedestrianManager>(*_worldPhysics);
	_collectibleManager = std::make_unique<CollectibleManager>(*_level, *_lineRenderer);
	_collectibleManager->SpawnOnPath();

	const auto& paths = _level->GetPaths();
	if (!paths.empty())
	{
		const auto* bestPath = &paths[0];
		for (const auto& p : paths)
			if (p.points.size() > bestPath->points.size())
				bestPath = &p;

		srand(42);
		for (size_t i = 0; i < bestPath->points.size(); i += 2)
		{
			const auto& pt = bestPath->points[i];
			Vector3 dir;
			if (i + 1 < bestPath->points.size())
				dir = (bestPath->points[i + 1] - pt).Normalized();
			Vector3 perp(-dir.Z, 0, dir.X);
			float height = 2.0f + (rand() % 10) * 1.5f;
			_buildings.push_back({pt + perp * (15.0f + (rand() % 10) * 2.0f), height});
			_buildings.push_back({pt - perp * (15.0f + (rand() % 10) * 2.0f), height});
		}

		Log::Info("Game: generated {} buildings", _buildings.size());
	}

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
	_buildingMesh = SimpleMesh::CreateBox(Vector3(2.0f, 4.0f, 2.0f), Vector4(0.7f, 0.6f, 0.5f, 1.0f));

	if (FileSystem::exists("scripts/Missions/level01/M1race.con"))
		_scriptEngine->RunFile("scripts/Missions/level01/M1race.con");
	else if (FileSystem::exists("scripts/Missions/level01/m1.con"))
		_scriptEngine->RunFile("scripts/Missions/level01/m1.con");

	loadGlobal();
	LoadModel("homer", "homer");

	_camera = std::make_unique<FreeCamera>();
	_camera->SetPosition(Vector3(228.0f, 5.0f, -174.0f));
	_camera->SetFOV(70.0f);
	_gameState = GameState::Splash;
	_missionCompleteTimer = 0.0;

	_frontend = std::make_unique<FrontendProject>();
	if (FileSystem::exists("./art/frontend/scrooby/bootup.p3d"))
		_frontend->LoadP3D("./art/frontend/scrooby/bootup.p3d");

	_camera->SetZNear(1.0f);
	_camera->SetZFar(100000.0f);

	_mouseLocked = false;

	_mainMenu = std::make_unique<GameMenu>();
	_mainMenu->AddButton("New Game", 0, 0, 200, 40, [this]() {
		_gameState = GameState::InGame;
	});
	_mainMenu->AddButton("Quit", 0, 0, 200, 40, [this]() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	});

	_pauseMenu = std::make_unique<GameMenu>();
	_pauseMenu->AddButton("Resume", 0, 0, 200, 40, [this]() {
		_gameState = GameState::InGame;
	});
	_pauseMenu->AddButton("Restart", 0, 0, 200, 40, [this]() {
		_scriptEngine->CleanupMission();
		if (_collectibleManager) _collectibleManager->SpawnOnPath();
		_scriptEngine->RunFile("scripts/Missions/level01/M1race.con");
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

	// todo: might need to reset our unique_ptrs here in a certain order...

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

	if (anim == "homer")
		_character->SetAnimation("hom_loco_walk");
}

void Game::LockMouse(bool lockMouse)
{
	if (_mouseLocked == lockMouse)
		return;

	_mouseLocked = lockMouse;

	SDL_SetRelativeMouseMode(lockMouse ? SDL_TRUE : SDL_FALSE);

	if (lockMouse)
	{
		SDL_GetMouseState(&_lockedMousePosX, &_lockedMousePosY);
	}

	int w, h;
	SDL_GetWindowSize(static_cast<SDL_Window*>(*_window), &w, &h);
	SDL_WarpMouseInWindow(static_cast<SDL_Window*>(*_window), w / 2, h / 2);

	Input::ResetMouseDelta();
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

std::vector<std::tuple<std::string, Vector3, std::string>> locations {
    {"Simpsons' House", Vector3(220, 3.5, -172), "l1z1.p3d;l1r1.p3d;l1r7.p3d;"},
    {"Kwik E Mart", Vector3(209, 3.6, -285), "l1z2.p3d;l1r1.p3d;l1r2.p3d;"},
    {"Church", Vector3(193.8, -0.9, -570), "l1r2.p3d;l1z2.p3d;l1z3.p3d;"},
    {"Springfield Elementary", Vector3(-11, 0.7, -586), "l1z3.p3d;l1r2.p3d;l1r3.p3d;"},
    {"Burns' Mansion", Vector3(-186, 3.5, -96), "l1z4.p3d;l1r3.p3d;l1r4a.p3d;"},
    {"Stonecutters Tunnel", Vector3(-405, 2, 60), "l1z4.p3d;l1r3.p3d;l1r4a.p3d;"},
    {"Power Plant Interior", Vector3(-80, 0.8, 297), "l1r4a.p3d;l1z6.p3d;l1r6.p3d;"},
    {"Power Plant Parking Lot", Vector3(40, 0, 296), "l1z6.p3d;l1r6.p3d;"},
    {"Tomacco", Vector3(190, -0.7, 425), "l1r6.p3d;l1z6.p3d;l1z7.p3d;"},
    {"Trailer Park", Vector3(391, -2.2, 494), "l1z7.p3d;l1r6.p3d;l1r7.p3d;"},
    {"Cletus' House", Vector3(333.5, -1.8, 356), "l1z7.p3d;l1r6.p3d;l1r7.p3d;"},
    {"Graveyard", Vector3(368, 5.1, 5.4), "l1z1.p3d;l1r1.p3d;l1r7.p3d;"}};

std::vector<std::pair<std::string, std::string>> models {
    {"homer", "homer"}, {"h_evil", "homer"}, {"h_fat", "homer"}, {"h_undr", "homer"},
    {"marge", "marge"}, {"bart", "bart"},    {"apu", "apu"},     {"a_amer", "apu"},
};

void Game::OnInputTextEntry(const std::string& text) {}

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

	Input::CaptureTextEntry(this, &Game::OnInputTextEntry);

	SDL_Event event;
	bool running = true;
	while (running)
	{
		last = now;
		now = SDL_GetPerformanceCounter();

		deltaTime = ((now - last) / (double)SDL_GetPerformanceFrequency());
		timer.Update(deltaTime);

		Input::PreEvent();

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				running = false;

			ImGui_ImplSDL2_ProcessEvent(&event);

			Input::HandleEvent(event);
		}

		LockMouse(Input::IsDown(Button::MouseRight));

		if (_mouseLocked)
		{
			const float mouseDeltaX = Input::GetMouseDeltaX();
			const float mouseDeltaY = Input::GetMouseDeltaY();

			_camera->LookDelta(mouseDeltaX * 0.25f, mouseDeltaY * 0.25f);
		}

		if (_mouseLocked)
		{
			auto inputForce = Vector3(0.0f);
			if (Input::IsDown(Button::KeyW))
				inputForce += Vector3::Forward;
			if (Input::IsDown(Button::KeyS))
				inputForce += Vector3::Backward;
			if (Input::IsDown(Button::KeyA))
				inputForce += Vector3::Left;
			if (Input::IsDown(Button::KeyD))
				inputForce += Vector3::Right;
			if (Input::IsDown(Button::KeySPACE))
				inputForce += Vector3::Up;
			if (Input::IsDown(Button::KeyLCONTROL))
				inputForce += Vector3::Down;
			if (inputForce.LengthSquared() > 0.0f)
			{
				inputForce.Normalize();
				inputForce *= Input::IsDown(Button::KeyLSHIFT) ? 60.0f : 10.0f;
				_camera->Move(inputForce, static_cast<float>(deltaTime));
			}
		}

		if (_character && !_inVehicle)
		{
			auto& ctrl = _character->GetCharacterController();
			auto charMove = Vector3::Zero;
			float padX = Input::GetGamepadAxisX();
			float padY = Input::GetGamepadAxisY();
			if (Input::IsDown(Button::KeyUp) || padY > 0.3f)    charMove += Vector3::Forward;
			if (Input::IsDown(Button::KeyDown) || padY < -0.3f) charMove += Vector3::Backward;
			if (Input::IsDown(Button::KeyLeft) || padX < -0.3f) charMove += Vector3::Left;
			if (Input::IsDown(Button::KeyRight) || padX > 0.3f) charMove += Vector3::Right;

			if (Input::JustPressed(Button::KeyE))
			{
				if (_scriptEngine->IsMissionActive())
				{
					for (auto& v : _scriptEngine->GetMissionVehicles())
					{
						if ((v->GetPosition() - _character->GetPosition()).Length() < 5.0f)
						{
							_inVehicle = true;
							_activeVehicle = v.get();
							Log::Info("Game: entered vehicle '{}'", v->GetName());
							_audioManager->PlayRaw(SoundGenerator::Beep(800, 0.15f), 22050, 1, 16);
							_scriptEngine->CloseObjective();
							_scriptEngine->ShowStageComplete();
							break;
						}
					}
				}
				if (!_inVehicle && ctrl.canJump())
					ctrl.jump(btVector3(0, 0, 0));
			}

			if (Input::JustPressed(Button::KeyT) && _scriptEngine->IsMissionActive())
			{
				for (auto& v : _scriptEngine->GetMissionVehicles())
				{
					SetPlayerPosition(v->GetPosition() + Vector3(0, 2.0f, 5.0f));
					break;
				}
			}

			if (charMove.LengthSquared() > 0.0f)
			{
				charMove.Normalize();
				charMove *= 5.0f;
				Quaternion rot = _character->GetRotation();
				Vector3 worldMove = rot * charMove;
				ctrl.setWalkDirection(BulletCast<btVector3>(worldMove));

				float targetYaw = atan2f(worldMove.X, worldMove.Z);
				_character->SetRotation(Quaternion::MakeFromEuler(Vector3(0, targetYaw, 0)));
			}
			else
			{
				ctrl.setWalkDirection(btVector3(0, 0, 0));
			}
		}

		if (_inVehicle && _activeVehicle)
		{
			if (_scriptEngine->IsMissionActive())
			{
				const auto& checkpoints = _scriptEngine->GetCheckpoints();
				int current = _scriptEngine->GetCurrentCheckpoint();
				if (current >= 0 && current < static_cast<int>(checkpoints.size()))
				{
					float dist = (_activeVehicle->GetPosition() - checkpoints[current]).Length();
					if (dist < 5.0f)
						_scriptEngine->AdvanceCheckpoint();
				}
			}

			if (Input::JustPressed(Button::KeyE))
			{
				_inVehicle = false;
				SetPlayerPosition(_activeVehicle->GetPosition() + Vector3(3.0f, 0, 0));
				_activeVehicle = nullptr;
				Log::Info("Game: exited vehicle");
			}
			else if (Input::JustPressed(Button::KeyH))
			{
				Log::Info("Vehicle: HONK!");
			}
			else if (Input::JustPressed(Button::KeySPACE))
			{
				_activeVehicle->Jump();
				_shakeAmount = 1.0f;
			}
			else
			{
				float throttle = 0, steer = 0, brake = 0;

				float padX = Input::GetGamepadAxisX();
				float padY = Input::GetGamepadAxisY();
				float trigR = Input::GetGamepadTriggerR();
				float trigL = Input::GetGamepadTriggerL();

				if (Input::IsDown(Button::KeyUp) || padY > 0.3f)    throttle = std::max(1.0f, padY);
				if (Input::IsDown(Button::KeyDown) || padY < -0.3f)  brake = std::max(1.0f, -padY);
				if (Input::IsDown(Button::KeyLeft) || padX < -0.3f)  steer = std::min(-1.0f, padX);
				if (Input::IsDown(Button::KeyRight) || padX > 0.3f)  steer = std::max(1.0f, padX);
				if (trigR > 0.3f) throttle = std::max(throttle, trigR);
				if (trigL > 0.3f) brake = std::max(brake, trigL);
				float boost = Input::IsDown(Button::KeyLSHIFT) ? 3.0f : 1.0f;
				_activeVehicle->ApplyInput(throttle, steer, brake, boost);
			}
		}

		auto cameraTransform = animCamera->Update(deltaTime * 35.0);

		if (_inVehicle && _activeVehicle && !_mouseLocked)
		{
			auto vehPos = _activeVehicle->GetPosition();
			auto vehRot = _activeVehicle->GetRotation();
			Vector3 camTarget = vehPos + vehRot * Vector3(0, 1.5f, 0);
			Vector3 targetPos = camTarget + vehRot * Vector3(0, 3.0f, 12.0f);

			float lerpFactor = 1.0f - exp(-8.0f * static_cast<float>(deltaTime));
			if (_smoothCamPos == Vector3::Zero) _smoothCamPos = targetPos;
			_smoothCamPos = _smoothCamPos + (targetPos - _smoothCamPos) * lerpFactor;

			_camera->SetPosition(_smoothCamPos);
			_camera->SetQuaternion(Quaternion::MakeFromEuler(Vector3(-0.2f, vehRot.Euler().Y, 0)));
		}
		else if (_character && !_mouseLocked)
		{
			auto charPos = _character->GetPosition();
			auto rot = _character->GetRotation();
			Vector3 camTarget = charPos + rot * Vector3(0, 1.5f, 0);
			Vector3 targetPos = camTarget + rot * Vector3(0, 3.0f, 8.0f);

			float lerpFactor = 1.0f - exp(-8.0f * static_cast<float>(deltaTime));
			if (_smoothCamPos == Vector3::Zero) _smoothCamPos = targetPos;
			_smoothCamPos = _smoothCamPos + (targetPos - _smoothCamPos) * lerpFactor;

			_camera->SetPosition(_smoothCamPos);
			_camera->SetQuaternion(Quaternion::MakeFromEuler(Vector3(-0.3f, rot.Euler().Y, 0)));
		}
		//_camera->SetPosition(cameraTransform.Translation());

		_worldPhysics->Update(static_cast<float>(deltaTime));

		_audioManager->Update();
		/*
		if (_inVehicle && _activeVehicle)
		{
			// collision damage disabled for debugging
		}
		*/

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
			_missionCompleteTimer += deltaTime;
			if (_missionCompleteTimer > 5.0)
			{
				_gameState = GameState::InGame;
				_missionCompleteTimer = 0.0;
				_scriptEngine->CleanupMission();
				if (_collectibleManager) _collectibleManager->SpawnOnPath();
				if (_inVehicle)
				{
					_inVehicle = false;
					_activeVehicle = nullptr;
				}
				if (FileSystem::exists("scripts/Missions/level01/M1race.con"))
					_scriptEngine->RunFile("scripts/Missions/level01/M1race.con");
			}
		}

		if (_gameState == GameState::Splash)
		{
			if (Input::JustPressed(Button::KeySPACE) || Input::JustPressed(Button::KeyENTER) ||
			    Input::JustPressed(Button::KeyESCAPE))
			{
				_gameState = GameState::MainMenu;
				_missionCompleteTimer = 0.0;
			}
		}

		if (_character && _character->GetPosition().Y < -50.0f)
		{
			Log::Warn("Game: player fell off the map, respawning...");
			_health -= 20.0f;
			if (_health <= 0.0f) _health = 100.0f;
			SetPlayerPosition(_lastSafePos);
		}

		if (_activeVehicle && _inVehicle && _activeVehicle->GetPosition().Y < -50.0f)
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

			if (_showDebug)
			{
				_lineRenderer->DrawSkeleton(_character->GetPosition(), _character->GetSkeleton());
				_lineRenderer->DrawBox(_character->GetPosition(), _character->GetRotation(),
					Vector3(-0.3f, 0.0f, -0.2f), Vector3(0.3f, 1.8f, 0.3f), Vector4(0.2f, 1.0f, 0.2f, 1.0f));
			}
		}
		_level->Update(deltaTime);
		if (_showDebug)
		{
			_trafficManager->Update(deltaTime);
			_trafficManager->Draw();
		}
		else
		{
			_trafficManager->Update(deltaTime);
		}
		_scriptEngine->Update(deltaTime);

		/*
		if (_character != nullptr && _gameState == GameState::InGame)
		{
			if (_level->CheckTrigger(_character->GetPosition(), ""))
			{
				static std::string lastTrigger;
				static double lastTriggerTime = 0;
				lastTriggerTime += deltaTime;
				if (lastTriggerTime > 2.0)
				{
					lastTriggerTime = 0;
					Log::Info("Game: player entered trigger zone");
				}
			}
			static double pedSpawnTimer = 0.0;
			pedSpawnTimer += deltaTime;
			if (pedSpawnTimer > 0.5)
			{
				pedSpawnTimer = 0.0;
				if (_pedestrianManager)
				{
					_pedestrianManager->Spawn(_character->GetPosition(), 40.0f);
					_pedestrianManager->Update(deltaTime, _character->GetPosition());
				}
				if (_collectibleManager)
					_collectibleManager->Update(_character->GetPosition(), 3.0f);
			}
		}
		*/
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

		if (Input::JustPressed(Button::KeyESCAPE))
		{
			if (_gameState == GameState::InGame)
				_gameState = GameState::Paused;
			else if (_gameState == GameState::Paused)
				_gameState = GameState::InGame;
		}

		if (Input::JustPressed(Button::KeyR) && _gameState == GameState::InGame)
		{
			_scriptEngine->ResetBestTime();
			Log::Info("Game: best time reset!");
		}

		if (_gameState == GameState::Paused && Input::JustPressed(Button::KeyQ))
		{
			running = false;
			Log::Info("Game: quit from pause menu");
		}

		if (Input::JustPressed(Button::KeyM) && _gameState == GameState::InGame)
		{
			Log::Info("Game: restarting mission...");
			_scriptEngine->CleanupMission();
			if (_collectibleManager) _collectibleManager->SpawnOnPath();
			if (_inVehicle) { _inVehicle = false; _activeVehicle = nullptr; }
			if (FileSystem::exists("scripts/Missions/level01/M1race.con"))
				_scriptEngine->RunFile("scripts/Missions/level01/M1race.con");
			else if (FileSystem::exists("scripts/Missions/level01/m1.con"))
				_scriptEngine->RunFile("scripts/Missions/level01/m1.con");
		}

		if (Input::JustPressed(Button::Key1))
		{
			_showDebug = !_showDebug;
			Log::Info("Debug draw: {}", _showDebug ? "ON" : "OFF");
		}

		if (Input::JustPressed(Button::KeyF))
		{
			_showHelp = !_showHelp;
		}

		if (_gameState == GameState::Paused)
		{
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			_window->Swap();
			continue;
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

		if (_character != nullptr)
			_character->Update(deltaTime);

		ImGui::Render();

		int viewportWidth = (int)io.DisplaySize.x;
		int viewportHeight = (int)io.DisplaySize.y;

		glViewport(0, 0, viewportWidth, viewportHeight);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glClearColor(0.62f, 0.78f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		_camera->SetAspectRatio(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));

		Matrix4x4 viewMatrix = _camera->GetViewMatrix();
		Matrix4x4 projMatrix = _camera->GetProjectionMatrix();
		Matrix4x4 viewProjection = projMatrix * viewMatrix;

		if (_level != nullptr)
			_level->Draw(viewProjection);

		for (const auto& b : _buildings)
		{
			float h = b.second;
			Vector4 col(0.8f, 0.7f, 0.6f, 1.0f);
			if (h > 10) col = Vector4(0.6f, 0.6f, 0.7f, 1.0f);

			_buildingMesh->Draw(*_meshShader,
				Matrix4x4::MakeTranslate(b.first + Vector3(0, h * 0.5f, 0)),
				viewProjection, col);
		}

		if (_character != nullptr && !_inVehicle)
		{
			_character->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
		}

		if (_scriptEngine->IsMissionActive())
		{
			for (auto& v : _scriptEngine->GetMissionVehicles())
			{
				v->Update(deltaTime);
				if (v->HasModel())
					v->Draw(viewProjection, *_skinShaderProgram, false);
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

		if (!_showDebug)
			_trafficManager->DrawSolid(*_carMesh, *_meshShader, viewProjection);

		if (!_showDebug && _pedestrianManager)
			_pedestrianManager->Draw(*_meshShader, *_playerMesh, viewProjection);

		if (_collectibleManager)
			_collectibleManager->Draw();

		if (_scriptEngine->IsMissionActive() && !_scriptEngine->GetCheckpoints().empty())
		{
			_carMesh->Draw(*_meshShader,
				Matrix4x4::MakeTranslate(_scriptEngine->GetAIPosition()) *
					Matrix4x4(_scriptEngine->GetAIRotation()),
				viewProjection, Vector4(1.0f, 0.2f, 0.2f, 1.0f));
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
		_lineRenderer->Flush(viewProjection);
		glEnable(GL_DEPTH_TEST);

		Matrix4x4 proj = Matrix4x4::MakeOrtho(0.0f, viewportWidth, viewportHeight, 0.0f);

		if (_textureFontP3D != nullptr)
		{
			std::string fps = fmt::format("{0} fps", timer.GetFps());
			auto font = _resourceManager->GetFont("boulder_16");
			sprites.DrawText(font, fps, Vector2(32 + 3, 32 + 3), Vector4(0.0f, 0.0f, 0.0f, 1.0f));
			sprites.DrawText(font, fps, Vector2(32, 32), Vector4(1.0f, 1.0f, 0.0f, 1.0f));

			if (_collectibleManager)
			{
				std::string coins = fmt::format("Coins: {}/{}",
					_collectibleManager->GetCollected(), _collectibleManager->GetTotalAvailable());
				sprites.DrawText(font, coins,
					Vector2(32 + 3, 142 + 3), Vector4(0.0f, 0.0f, 0.0f, 1.0f));
				sprites.DrawText(font, coins,
					Vector2(32, 142), Vector4(1.0f, 0.84f, 0.0f, 1.0f));
			}

			if (_character)
			{
				std::string pos = fmt::format("Pos: {:.1f} {:.1f} {:.1f}",
					_character->GetPosition().X, _character->GetPosition().Y, _character->GetPosition().Z);
				sprites.DrawText(font, pos, Vector2(32, 52), Vector4(1.0f, 1.0f, 1.0f, 1.0f));

				std::string hp = fmt::format("HP: {:.0f}", _health);
				Vector4 hpCol = _health > 50 ? Vector4(0.0f, 1.0f, 0.0f, 1.0f) :
				                _health > 25 ? Vector4(1.0f, 1.0f, 0.0f, 1.0f) :
				                               Vector4(1.0f, 0.0f, 0.0f, 1.0f);
				sprites.DrawText(font, hp, Vector2(viewportWidth - 100.0f, 72), hpCol);
			}

			if (_scriptEngine->IsMissionActive())
			{
				float goTimer = _scriptEngine->GetGoTimer();
				if (goTimer > 0.0f)
				{
					sprites.DrawText(font, "GO!",
						Vector2((viewportWidth / 2.0f) - 20, viewportHeight / 2.0f),
						Vector4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					sprites.DrawText(font, "MISSION ACTIVE", Vector2(32, 72), Vector4(0.0f, 1.0f, 0.0f, 1.0f));
				}

				const auto& objType = _scriptEngine->GetObjectiveType();
				if (!objType.empty())
				{
					std::string objText = fmt::format("Objective: {}", objType);
					sprites.DrawText(font, objText,
						Vector2((viewportWidth / 2.0f) - 100, 32),
						Vector4(1.0f, 0.84f, 0.0f, 1.0f));
				}

				float timeLeft = _scriptEngine->GetStageTimeRemaining();
				if (timeLeft > 0.0f)
				{
					std::string timerText = fmt::format("Time: {:.0f}s", timeLeft);
					sprites.DrawText(font, timerText,
						Vector2(viewportWidth - 130.0f, 32),
						timeLeft < 30.0f ? Vector4(1.0f, 0.3f, 0.3f, 1.0f) : Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}

				if (_scriptEngine->GetTotalLaps() > 1)
				{
					size_t totalCp = _scriptEngine->GetCheckpoints().size();
					std::string lapText = fmt::format("Lap {}/{}  CP {}/{}",
						_scriptEngine->GetCurrentLap() + 1, _scriptEngine->GetTotalLaps(),
						_scriptEngine->GetCurrentCheckpoint(), totalCp);
					sprites.DrawText(font, lapText,
						Vector2((viewportWidth / 2.0f) - 80, 72),
						Vector4(0.0f, 1.0f, 1.0f, 1.0f));
				}

				if (_gameState == GameState::Splash)
				{
					sprites.DrawText(font, "PRESS ENTER TO START",
						Vector2((viewportWidth / 2.0f) - 100, viewportHeight - 80.0f),
						Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}
			}

			if (_inVehicle)
			{
				std::string ctrlText = Input::IsGamepadConnected()
					? "Stick: Drive | A: Exit | X: Horn | B: Boost | Y: Jump"
					: "Arrows: Drive | Shift: Boost | H: Horn | E: Exit";
				sprites.DrawText(font, ctrlText,
					Vector2(32, 92), Vector4(0.5f, 0.8f, 1.0f, 1.0f));
				if (_activeVehicle)
				{
					float spd = _activeVehicle->GetSpeedKmh();
					std::string speedText = fmt::format("{:.0f} km/h", spd);
					Vector4 spdCol = spd > 100 ? Vector4(1.0f, 0.3f, 0.3f, 1.0f) :
					                 spd > 50  ? Vector4(1.0f, 1.0f, 0.3f, 1.0f) :
					                             Vector4(1.0f, 1.0f, 1.0f, 1.0f);
					sprites.DrawText(font, speedText,
						Vector2(viewportWidth - 130.0f, viewportHeight - 50.0f), spdCol);

					float dmgRatio = 1.0f - (_health / 100.0f);
					std::string carHp = fmt::format("Car: {:.0f}%", _health);
					Vector4 dmgCol(dmgRatio, 1.0f - dmgRatio, 0.0f, 1.0f);
					if (_health <= 25.0f)
						dmgCol = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
					sprites.DrawText(font, carHp,
						Vector2(viewportWidth - 130.0f, viewportHeight - 30.0f), dmgCol);
				}
			}
			else if (_gameState == GameState::InGame)
			{
				std::string ctrlText = Input::IsGamepadConnected()
					? "Stick: Move | A: Action | Y: Jump | Back: Restart | Start: Pause"
					: "Arrows: Move | E: Action | M: Restart | ESC: Pause";
				sprites.DrawText(font, ctrlText,
					Vector2(32, 92), Vector4(0.5f, 0.5f, 0.5f, 1.0f));

				if (_showHelp)
				{
					const char* lines[] = {
						"=== CONTROLS ===",
						"Arrows : Move / Drive",
						"E/Space: Jump | Shift: Boost",
						"H: Horn | M: Restart | R: Reset",
						"T: Teleport | 1: Debug | F: Help",
						"ESC: Pause | RightClick: Fly",
					};
					for (int i = 0; i < 6; ++i)
						sprites.DrawText(font, lines[i],
							Vector2(32, 112.0f + i * 18), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}

				if (_character && _scriptEngine->IsMissionActive())
				{
					for (auto& v : _scriptEngine->GetMissionVehicles())
					{
						float dist = (v->GetPosition() - _character->GetPosition()).Length();
						if (dist < 5.0f)
						{
							sprites.DrawText(font, "Press E to enter vehicle!",
								Vector2(32, 112), Vector4(0.0f, 1.0f, 1.0f, 1.0f));
						}
						std::string distText = fmt::format("Vehicle: {:.0f}m", dist);
						sprites.DrawText(font, distText, Vector2(32, 72), Vector4(0.8f, 0.8f, 0.8f, 1.0f));
						break;
					}
				}
			}

			auto* chase = _scriptEngine->GetChaseManager();
			if (chase && chase->IsActive())
			{
				float heat = chase->GetHeat();
				std::string heatText = fmt::format("HEAT: {:.0f}/10", heat);
				Vector4 heatCol = heat > 7 ? Vector4(1.0f, 0.0f, 0.0f, 1.0f) :
				                  heat > 3 ? Vector4(1.0f, 0.5f, 0.0f, 1.0f) :
				                             Vector4(1.0f, 1.0f, 0.0f, 1.0f);
				sprites.DrawText(font, heatText, Vector2(32, 72), heatCol);

				if (chase->IsBusted())
				{
					sprites.DrawText(font, "BUSTED!",
						Vector2((viewportWidth / 2.0f) - 40, viewportHeight / 2.0f - 40),
						Vector4(1.0f, 0.0f, 0.0f, 1.0f));
				}
			}

			if (_gameState == GameState::Paused)
			{
				sprites.DrawText(font, "PAUSED",
					Vector2((viewportWidth / 2.0f) - 40, viewportHeight / 2.0f + 40),
					Vector4(1.0f, 1.0f, 0.0f, 1.0f));
				_pauseMenu->Update(Input::GetMouseX(), Input::GetMouseY());
				for (const auto& btn : _pauseMenu->GetButtons())
				{
					Vector4 col = btn.hovered ? Vector4(1.0f, 0.3f, 0.3f, 1.0f)
					                         : Vector4(0.5f, 0.5f, 0.5f, 1.0f);
					sprites.DrawText(font, btn.label,
						Vector2(viewportWidth / 2.0f - 50.0f,
						        viewportHeight / 2.0f + 20.0f - (&btn - _pauseMenu->GetButtons().data()) * 30.0f),
						col);
				}
				if (Input::JustPressed(Button::MouseLeft))
					_pauseMenu->CheckClick(Input::GetMouseX(), Input::GetMouseY());
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

				sprites.DrawText(font, "STAGE COMPLETE!",
					Vector2((viewportWidth / 2.0f) - 100, viewportHeight / 2.0f - 50),
					Vector4(1.0f, 0.84f, 0.0f, 1.0f));

				float elapsed = _scriptEngine->GetInitialStageTime() - _scriptEngine->GetStageTimeRemaining();
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
				std::string nextText = fmt::format("Next race in {}...", nextIn);
				sprites.DrawText(font, nextText,
					Vector2((viewportWidth / 2.0f) - 60, viewportHeight / 2.0f + 40),
					Vector4(0.6f, 0.6f, 0.6f, 1.0f));
			}

			if (_gameState == GameState::MissionFailed)
			{
				sprites.DrawText(font, "MISSION FAILED!",
					Vector2((viewportWidth / 2.0f) - 100, viewportHeight / 2.0f - 20),
					Vector4(1.0f, 0.2f, 0.2f, 1.0f));
				int nextIn = 5 - static_cast<int>(_missionCompleteTimer);
				std::string nextText = fmt::format("Retrying in {}...", nextIn);
				sprites.DrawText(font, nextText,
					Vector2((viewportWidth / 2.0f) - 60, viewportHeight / 2.0f + 10),
					Vector4(0.6f, 0.6f, 0.6f, 1.0f));
			}
		}

		if (_gameState == GameState::MainMenu)
		{
			auto font = _resourceManager->GetFont("boulder_16");
			SpriteBatch menuSprites;
			menuSprites.DrawText(font, "donut",
				Vector2((viewportWidth / 2.0f) - 30, viewportHeight / 2.0f + 80),
				Vector4(1.0f, 0.84f, 0.0f, 1.0f));
			_mainMenu->Update(Input::GetMouseX(), Input::GetMouseY());
			int btnIndex = 0;
			for (const auto& btn : _mainMenu->GetButtons())
			{
				Vector4 col = btn.hovered ? Vector4(1.0f, 0.84f, 0.0f, 1.0f)
				                         : Vector4(0.8f, 0.8f, 0.8f, 1.0f);
				menuSprites.DrawText(font, btn.label,
					Vector2(viewportWidth / 2.0f - 50.0f,
					        viewportHeight / 2.0f + 40.0f - btnIndex * 30.0f),
					col);
				++btnIndex;
			}
			if (Input::JustPressed(Button::MouseLeft))
				_mainMenu->CheckClick(Input::GetMouseX(), Input::GetMouseY());
			menuSprites.Flush(proj);
		}

		sprites.Flush(proj);
		if (_gameState == GameState::Splash)
			_frontend->Draw(proj);

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
		for (auto const& location : locations)
		{
			if (ImGui::MenuItem(std::get<0>(location).c_str()))
			{
				const Vector3& dest = std::get<1>(location);
				//_worldPhysics->GetCharacterController()->SetPosition(dest);
				_camera->SetPosition(dest);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(std::get<1>(location).ToString().c_str());
				ImGui::TextUnformatted(std::get<2>(location).c_str());
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#endif

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

} // namespace Donut
