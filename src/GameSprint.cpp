// The bonus game's side of Game: its menu from the main menu, its races on a
// track loaded in the level's place, their standings.

#include "Game.h"

#include "AI/PathGraph.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Character.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game/GameInput.h"
#include "Game/SuperSprint.h"
#include "Input/ExtraPads.h"
#include "Physics/WorldPhysics.h"
#include "Render/Font.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/SpriteBatch.h"
#include "Render/imgui/imgui.h"
#include "Render/imgui/imgui_impl_opengl3.h"
#include "Render/imgui/imgui_impl_sdl.h"
#include "ResourceManager.h"
#include "UI/LivingRoom.h"
#include "Vehicle.h"
#include "Window.h"

#include <SDL.h>

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

namespace Donut
{

namespace
{
// A race's points, by place: not read from the game, the standings' column
// counted the plain way.
constexpr int kPoints[] = {4, 3, 2, 1};
// The race over, its standings this long after.
constexpr double kStandingsAfter = 2.5;
// The players' colours (P1 to P4) and the computer's.
const Vector4 kPlayerColors[] = {{1.0f, 0.25f, 0.2f, 1.0f},
                                 {0.3f, 0.55f, 1.0f, 1.0f},
                                 {0.3f, 0.9f, 0.3f, 1.0f},
                                 {1.0f, 0.85f, 0.1f, 1.0f}};
const Vector4 kComputerColor(0.85f, 0.85f, 0.85f, 1.0f);
} // namespace

std::vector<MiniGameMenu::Car> Game::miniGameCars() const
{
	// Those had (bought, won, the levels' own of the levels reached) that
	// the menu has a picture of.
	const int reached = loadStoryProgress().level;
	std::vector<MiniGameMenu::Car> cars;
	for (const Reward& reward : rewards("car"))
	{
		if (!reward.unlocked || (reward.how == "defaultcar" && reward.level > reached))
			continue;
		if (!FileSystem::exists(fmt::format("./art/frontend/scrooby2/resource/images/minigame/vehicles/{}.png",
		                                    reward.car)) ||
		    !FileSystem::exists(fmt::format("./art/cars/{}.p3d", reward.car)))
			continue;
		if (std::any_of(cars.begin(), cars.end(), [&](const auto& car) { return car.name == reward.car; }))
			continue;
		// Its four stars summed, halved: 0 to 9 on the page's star.
		const float sum = reward.stars[0] + reward.stars[1] + reward.stars[2] + reward.stars[3];
		cars.push_back({reward.car, std::clamp(static_cast<int>(std::lround(sum / 2.0f)), 0, 9)});
	}
	return cars;
}

int Game::miniGameTracks() const
{
	// Each level's seven cards found unlock the next track ("New bonus game
	// track unlocked!" follows the cards' set complete on the HUD page).
	int tracks = 1;
	for (int level = 1; level < SuperSprint::kTracks; ++level)
	{
		int found = 0;
		for (const std::string& card : _save.Entries("card"))
			if (card.size() >= 6 && card.compare(0, 4, "card") == 0 && card[4] == '0' + level)
				++found;
		if (found >= 7)
			++tracks;
	}
	return tracks;
}

void Game::openMiniGame()
{
	if (!_miniMenu || !_miniMenu->IsLoaded())
		return;
	if (!_extraPads)
		_extraPads = std::make_unique<ExtraPads>();
	_miniTally = {};
	_miniMenu->OpenSetup(miniGameCars(), miniGameTracks());
	_miniStage = MiniStage::Menu;
	_gameState = GameState::MiniGame;
	for (auto& held : _miniHeld)
		held.fill(true);
}

void Game::leaveMiniGame()
{
	_sprint.reset();
	_sprintDrivers.clear();
	// The story's level back, where the track stood.
	if (_trackLoaded)
		switchLevel(_levelNumber);
	_gameState = GameState::MainMenu;
}

std::array<MiniGameMenu::Pad, 4> Game::miniPads(const GameIntent& intent)
{
	std::array<MiniGameMenu::Pad, 4> pads {};
	MiniGameMenu::Pad& first = pads[0];
	first.connected = true;
	first.left = intent.moveLeft > 0.5f;
	first.right = intent.moveRight > 0.5f;
	first.up = intent.moveForward > 0.5f;
	first.down = intent.moveBackward > 0.5f;
	first.confirm = intent.uiConfirm || intent.mouseLeftClick;
	first.back = intent.pauseToggle;
	first.aux = intent.honk;
	if (!_extraPads)
		return pads;
	const auto& extra = _extraPads->Poll();
	for (int i = 0; i < ExtraPads::kCount; ++i)
	{
		const ExtraPads::Pad& pad = extra[i];
		MiniGameMenu::Pad& to = pads[i + 1];
		auto& held = _miniHeld[i + 1];
		const bool now[5] = {pad.a, pad.b, pad.x, pad.start, pad.nitro};
		bool pressed[5];
		for (int b = 0; b < 5; ++b)
		{
			pressed[b] = now[b] && !held[b];
			held[b] = now[b];
		}
		to.connected = pad.connected;
		to.left = pad.left;
		to.right = pad.right;
		to.up = pad.up;
		to.down = pad.down;
		to.confirm = pressed[0];
		to.back = pressed[1];
		to.aux = pressed[2];
		to.start = pressed[3];
	}
	return pads;
}

void Game::startSprint(const SuperSprint::Setup& setup)
{
	_sprint.reset();
	_sprintDrivers.clear();
	// The track in the level's place, once: the next race on it keeps it.
	if (!_trackLoaded || _loadedTrack != setup.track)
	{
		unloadLevel();
		_level = std::make_unique<Level>();
		_level->SetSkyShown(false);
		for (const std::string& file : SuperSprint::TrackFiles(setup.track))
			_level->LoadP3D(file);
		_level->LoadP3D("missions/generic/nitro.p3d");
		_pathGraph = std::make_unique<PathGraph>(*_level, *_worldPhysics);
		_trackLoaded = true;
		_loadedTrack = setup.track;
	}
	_sprint = std::make_unique<SuperSprint>(*_level, *_worldPhysics, *_pathGraph, setup);
	for (const auto& racer : _sprint->GetRacers())
	{
		auto driver = makeNpc(racer.entry.character);
		if (driver)
			for (const auto& [anim, data] : driver->GetAnimations())
				if (anim.size() > 12 && anim.compare(anim.size() - 12, 12, "_in_car_idle") == 0)
					driver->SetAnimation(anim);
		_sprintDrivers.push_back(std::move(driver));
	}
	_miniStage = MiniStage::Race;
	_miniFinishedFor = 0.0;
	_miniNitroHeld = true;
	_audioManager->PlayMusicEvent("SuperSprint");
}

void Game::miniGameFrame(double dt, const GameIntent& intent)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(*_window));
	ImGui::NewFrame();
	ImGui::Render();
	const ImGuiIO& io = ImGui::GetIO();
	const int width = static_cast<int>(io.DisplaySize.x), height = static_cast<int>(io.DisplaySize.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	const Matrix4x4 proj = Matrix4x4::MakeOrtho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
	const std::array<MiniGameMenu::Pad, 4> pads = miniPads(intent);

	if (_miniStage == MiniStage::Menu)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Over the living room, as the main menu.
		if (_livingRoom && _livingRoom->IsLoaded() && _level && !_trackLoaded)
			_livingRoom->Draw(_level->GetWorldShader(), *_skinShaderProgram, _level->GetBillboardShader(),
			                  static_cast<float>(width) / std::max(1, height), dt);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		switch (_miniMenu->DrawSetup(pads, proj))
		{
		case MiniGameMenu::Result::Start: startSprint(_miniMenu->GetSetup()); break;
		case MiniGameMenu::Result::Quit: leaveMiniGame(); break;
		case MiniGameMenu::Result::None: break;
		}
		_window->Swap();
		return;
	}

	if (_miniStage == MiniStage::Race && _sprint)
	{
		// The players' hands on their wheels.
		std::array<SuperSprint::Controls, SuperSprint::kRacers> controls {};
		controls[0].gas = intent.moveForward;
		controls[0].brake = intent.moveBackward;
		controls[0].steer = intent.moveRight - intent.moveLeft;
		controls[0].nitro = intent.boostHeld && !_miniNitroHeld;
		_miniNitroHeld = intent.boostHeld;
		if (_extraPads)
		{
			const auto& extra = _extraPads->Poll();
			for (int i = 0; i < ExtraPads::kCount; ++i)
			{
				controls[i + 1].gas = extra[i].gas;
				controls[i + 1].brake = extra[i].brake;
				controls[i + 1].steer = extra[i].steer;
				controls[i + 1].nitro = extra[i].nitro && !_miniHeld[i + 1][4];
				_miniHeld[i + 1][4] = extra[i].nitro;
			}
		}
		_worldPhysics->Update(static_cast<float>(dt));
		_level->UpdateCollisions();
		_level->Update(dt);
		_sprint->Update(dt, controls);
		for (std::size_t i = 0; i < _sprintDrivers.size(); ++i)
			if (_sprintDrivers[i])
				_sprintDrivers[i]->Update(dt);

		// What is heard: the countdown, a nitro, a pickup, a lap.
		const SuperSprint::Events& events = _sprint->GetEvents();
		if (events.countdownTick > 0)
			_audioManager->PlayRaw(SoundGenerator::Chirp(600, 600, 0.15f, 44100), 44100, 1, 16);
		if (events.started)
			_audioManager->PlayRaw(SoundGenerator::Chirp(1200, 1200, 0.3f, 44100), 44100, 1, 16);
		if (!events.pickedNitro.empty() || !events.pickedWrench.empty())
			_audioManager->PlayRaw(SoundGenerator::Chirp(800, 1400, 0.15f, 44100), 44100, 1, 16);
		if (!events.nitrosUsed.empty())
			_audioManager->PlayRaw(SoundGenerator::Chirp(300, 900, 0.4f, 44100), 44100, 1, 16);

		if (_sprint->GetPhase() == SuperSprint::Phase::Finished)
		{
			if (_miniFinishedFor == 0.0)
			{
				// A player first: the winner's music, else the loser's.
				bool playerWon = false;
				for (const auto& racer : _sprint->GetRacers())
					playerWon = playerWon || (racer.entry.player >= 0 && racer.place == 1 && racer.finished);
				_audioManager->PlayMusicEvent(playerWon ? "Win_SuperSprint" : "Lose_SuperSprint");
			}
			_miniFinishedFor += dt;
			if (_miniFinishedFor > kStandingsAfter)
			{
				// The race's points into the series', then its standings.
				std::vector<MiniGameMenu::Standing> standings;
				for (int i = 0; i < SuperSprint::kRacers; ++i)
				{
					const auto& racer = _sprint->GetRacers()[i];
					MiniTally& tally = _miniTally[i];
					if (racer.finished)
					{
						tally.points += kPoints[racer.place - 1];
						tally.wins += racer.place == 1 ? 1 : 0;
					}
					MiniGameMenu::Standing standing;
					standing.character = racer.entry.character;
					standing.player = racer.entry.player;
					standing.place = racer.finished ? racer.place : 0;
					standing.wins = tally.wins;
					standing.points = tally.points;
					standing.totalTime = racer.finished ? racer.totalTime : -1.0;
					standing.bestLap = racer.bestLap;
					standings.push_back(standing);
				}
				std::stable_sort(standings.begin(), standings.end(),
				                 [](const auto& a, const auto& b) { return a.points > b.points; });
				_miniMenu->OpenSummary(std::move(standings));
				_miniStage = MiniStage::Summary;

			}
		}
		else if (pads[0].back || std::any_of(pads.begin() + 1, pads.end(), [](const auto& p) { return p.start; }))
		{
			_miniMenu->OpenPause();
			_miniStage = MiniStage::Paused;
		}
	}

	drawSprint(width, height);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (_miniStage == MiniStage::Paused)
	{
		MiniGameMenu::Pad pad = pads[0];
		for (int i = 1; i < 4; ++i)
		{
			pad.up = pad.up || pads[i].up;
			pad.down = pad.down || pads[i].down;
			pad.confirm = pad.confirm || pads[i].confirm;
			pad.back = pad.back || pads[i].back;
			pad.start = pad.start || pads[i].start;
		}
		switch (_miniMenu->DrawPause(pad, proj))
		{
		case MiniGameMenu::PauseResult::Continue: _miniStage = MiniStage::Race; break;
		case MiniGameMenu::PauseResult::Quit:
			_sprint.reset();
			_sprintDrivers.clear();
			_miniMenu->ReturnToTracks();
			_miniStage = MiniStage::Menu;
			break;
		case MiniGameMenu::PauseResult::None: break;
		}
	}
	else if (_miniStage == MiniStage::Summary)
	{
		// --minigame: the standings shown a while, and the game closes.
		_miniFinishedFor += dt;
		if (_miniGameDemo > 0 && _miniFinishedFor > kStandingsAfter + 3.0)
		{
			Log::Info("Game: the bonus game's race is over, its standings shown; closing");
			SDL_Event quit;
			quit.type = SDL_QUIT;
			SDL_PushEvent(&quit);
			_miniGameDemo = 0;
		}
		if (_miniMenu->DrawSummary(pads[0], proj))
		{
			_sprint.reset();
			_sprintDrivers.clear();
			_miniMenu->ReturnToTracks();
			_miniStage = MiniStage::Menu;
		}
	}
	_window->Swap();
}

void Game::drawSprint(int width, int height)
{
	if (!_sprint || !_level)
		return;
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// The track's camera: right, up and forward as rows (left-handed, z
	// ahead), its lens across the screen's width.
	const SuperSprint::Camera& camera = _sprint->GetCamera();
	Vector3 forward = camera.look, eye = camera.position, cameraUp = camera.up;
	Vector3 right = cameraUp.Cross(forward).Normalized();
	Vector3 up = forward.Cross(right);
	const Matrix4x4 view(right.X, up.X, forward.X, 0.0f, right.Y, up.Y, forward.Y, 0.0f, right.Z, up.Z, forward.Z, 0.0f,
	                     -right.Dot(eye), -up.Dot(eye), -forward.Dot(eye), 1.0f);
	const float aspect = static_cast<float>(width) / static_cast<float>(std::max(1, height));
	const float tanHalf = std::tan(camera.fov * 0.5f);
	Matrix4x4 projection = Matrix4x4::Zero;
	projection[0][0] = 1.0f / tanHalf;
	projection[1][1] = aspect / tanHalf;
	projection[2][2] = (camera.farPlane + camera.nearPlane) / (camera.farPlane - camera.nearPlane);
	projection[2][3] = 1.0f;
	projection[3][2] = -(2.0f * camera.farPlane * camera.nearPlane) / (camera.farPlane - camera.nearPlane);
	Matrix4x4 viewProj = projection * view;

	_level->Draw(viewProj);
	_level->ApplyLights(*_skinShaderProgram);
	_sprint->Draw(viewProj, _level->GetWorldShader(), static_cast<float>(_elapsedSeconds));
	glDisable(GL_CULL_FACE);
	for (std::size_t i = 0; i < _sprintDrivers.size() && i < _sprint->GetRacers().size(); ++i)
	{
		const auto& racer = _sprint->GetRacers()[i];
		if (_sprintDrivers[i] && racer.lostFor <= 0.0 && seatDriver(*_sprintDrivers[i], *racer.vehicle))
			_sprintDrivers[i]->Draw(viewProj, *_skinShaderProgram, *_resourceManager);
	}
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	_lineRenderer->Discard();

	// Not under the standings.
	if (_miniStage != MiniStage::Summary)
		drawSprintHud(Matrix4x4::MakeOrtho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f), viewProj,
		              width, height);
}

void Game::drawSprintHud(const Matrix4x4& proj, const Matrix4x4& viewProj, int width, int height)
{
	const Font* font = _resourceManager->GetFont("boulder_16");
	if (!font || !_sprint)
		return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	SpriteBatch batch;
	const float scale = std::max(1.0f, static_cast<float>(height) / 480.0f);
	const auto text = [&](const std::string& line, Vector2 at, const Vector4& color, float size, bool centred) {
		if (centred)
			at.X -= font->MeasureWidth(line) * size * 0.5f;
		batch.DrawText(font, line, at + Vector2(2.0f, 2.0f) * size, Vector4(0.0f, 0.0f, 0.0f, color.W * 0.8f), size);
		batch.DrawText(font, line, at, color, size);
	};
	const auto phrase = [this](const char* key, const char* otherwise) {
		const std::string& found = _textBible.Get(key);
		return found.empty() ? std::string(otherwise) : found;
	};
	const char* places[] = {"1ST", "2ND", "3RD", "4TH"};

	// Over each car, who drives it.
	for (const auto& racer : _sprint->GetRacers())
	{
		if (racer.lostFor > 0.0)
			continue;
		const Vector3 above = racer.vehicle->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);
		const Vector4 clip = viewProj * Vector4(above.X, above.Y, above.Z, 1.0f);
		if (clip.W <= 0.0f)
			continue;
		const Vector2 at((clip.X / clip.W * 0.5f + 0.5f) * width, (0.5f - clip.Y / clip.W * 0.5f) * height);
		const bool player = racer.entry.player >= 0;
		text(player ? fmt::format("P{}", racer.entry.player + 1) : std::string("CPU"), at - Vector2(0.0f, 14.0f * scale),
		     player ? kPlayerColors[racer.entry.player] : kComputerColor, scale * (player ? 1.0f : 0.7f), true);
	}

	// Each player's corner: place, lap, nitros.
	const int laps = _sprint->GetLaps();
	for (const auto& racer : _sprint->GetRacers())
	{
		const int p = racer.entry.player;
		if (p < 0)
			continue;
		const bool rightSide = p % 2 == 1, bottom = p >= 2;
		const float x = rightSide ? width - 200.0f * scale : 20.0f * scale;
		const float y = bottom ? height - 90.0f * scale : 20.0f * scale;
		const Vector4& color = kPlayerColors[p];
		text(fmt::format("P{}  {}", p + 1, phrase(places[racer.place - 1], places[racer.place - 1])), Vector2(x, y),
		     color, scale * 1.4f, false);
		const int lap = std::min(laps, racer.laps + 1);
		text(fmt::format("{} {}/{}", phrase("LAPS", "LAPS"), lap, laps), Vector2(x, y + 26.0f * scale),
		     Vector4(1.0f, 1.0f, 1.0f, 1.0f), scale, false);
		text(fmt::format("NITRO x{}", racer.nitros), Vector2(x, y + 46.0f * scale),
		     racer.nitroFor > 0.0 ? Vector4(1.0f, 0.6f, 0.1f, 1.0f) : Vector4(0.6f, 0.85f, 1.0f, 1.0f), scale, false);
		if (racer.health < 0.999f)
			text(fmt::format("{}%", static_cast<int>(racer.health * 100.0f)), Vector2(x + 130.0f * scale, y + 46.0f * scale),
			     Vector4(1.0f, 0.4f, 0.3f, 1.0f), scale, false);
	}

	// The countdown, then GO!.
	const Vector2 middle(width * 0.5f, height * 0.4f);
	if (_sprint->GetPhase() == SuperSprint::Phase::Countdown)
		text(std::to_string(static_cast<int>(std::ceil(_sprint->GetCountdown()))), middle,
		     Vector4(1.0f, 0.9f, 0.2f, 1.0f), scale * 4.0f, true);
	else if (_sprint->GetRaceTime() < 1.0)
		text(phrase("GO", "GO!"), middle, Vector4(0.3f, 1.0f, 0.3f, 1.0f), scale * 4.0f, true);
	batch.Flush(proj);
}

} // namespace Donut
