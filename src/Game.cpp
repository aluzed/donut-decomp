// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Game.h"

#include "AI/PathGraph.h"
#include "AI/PedestrianManager.h"
#include "AI/TrafficManager.h"
#include "AnimCamera.h"
#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Character.h"
#include "Core/FpsTimer.h"
#include "Core/Math/Frustum.h"
#include "Core/Math/Math.h"
#include "FreeCamera.h"
#include "FrontendProject.h"
#include "Game/CollectibleManager.h"
#include "Game/GameCamera.h"
#include "Game/GameInput.h"
#include "Game/HitAndRun.h"
#include "Game/SuperSprint.h"
#include "Game/UfoBoss.h"
#include "Game/WaspManager.h"
#include "Input/ExtraPads.h"
#include "Input/Input.h"
#include "Input/Keymap.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Physics/BulletCast.h"
#include "Physics/WorldPhysics.h"
#include "RCL/RCFFile.h"
#include "RCL/RSDFile.h"
#include "Render/BlobShadows.h"
#include "Render/CompositeModel.h"
#include "Render/Font.h"
#include "Render/LineRenderer.h"
#include "Render/OpenGL/FrameBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/ParticleSystem.h"
#include "Render/PostProcess.h"
#include "Render/SceneSkins.h"
#include "Render/Shader.h"
#include "Render/ShadowMap.h"
#include "Render/SimpleMesh.h"
#include "Render/SkidMarks.h"
#include "Render/SkinModel.h"
#include "Render/SpriteBatch.h"
#include "Render/VertexAnimation.h"
#include "Render/imgui/imgui.h"
#include "Render/imgui/imgui_impl_opengl3.h"
#include "Render/imgui/imgui_impl_sdl.h"
#include "Render/imgui/imgui_markdown.h"
#include "ResourceManager.h"
#include "Scripting/Commands.h"
#include "Scripting/ScriptEngine.h"
#include "UI/GameHud.h"
#include "UI/GameMenu.h"
#include "UI/GameText.h"
#include "UI/Hud.h"
#include "UI/LivingRoom.h"
#include "UI/LoadingScreen.h"
#include "UI/MoviePlayer.h"
#include "UI/OptionsMenu.h"
#include "UI/PauseSundayMenu.h"
#include "UI/PhoneBoothMenu.h"
#include "UI/ScreenText.h"
#include "UI/ShopMenu.h"
#include "Window.h"
#include <cctype>
#include <fstream>

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
// Below this speed, holding "back" reverses instead of braking.
constexpr float kReverseThresholdKmh = 2.0f;
// The effects file's wheel sprays (their particle types), the speed a wheel
// throws them up from, and how far back from straight up they go.
constexpr uint32_t kWheelSprayDirt = 10;
constexpr uint32_t kWheelSprayGrass = 11;
constexpr float kWheelSprayKmh = 15.0f;
constexpr float kWheelSprayTilt = -0.9f;
// A footstep every so far, metres (a run's stride), heard at the feet, this far
// under the character's middle.
constexpr float kStride = 1.1f;
constexpr float kFeetBelowCentre = 0.9f;
// How fast the credits roll, pixels a second.
constexpr float kCreditsSpeed = 60.0f;
// The effects file's dust of a dash.
constexpr uint32_t kDashDust = 9;
// The effects file's car damage effect (smoke, then fire), and the health the
// player's car starts giving it off below.
constexpr uint32_t kCarDamageEffect = 14;
constexpr float kCarSmokesBelow = 25.0f;
// A wheel going sideways over the ground faster than this, m/s, skids and
// marks it, black at twice that; so does one braking hard. How wide its mark.
constexpr float kSkidSlide = 3.5f;
constexpr float kSkidWidth = 0.3f;
// Setting off flat out, as the game's sound settings give it (SetPeeloutMin 0.1,
// SetPeeloutMaxTrim 0.8): how hard the tyres spin before they mark and squeal,
// and how loud and dark at most.
constexpr float kPeelOutMin = 0.1f;
constexpr float kPeelOutMaxTrim = 0.8f;
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
	// The save first: the level's cards, gags and wasp cameras are read
	// from it as the level loads.
	_save.Load();

	parseArguments(argc, argv);
	Commands::RunLine("HelloWorld();");

	openWindow();
	loadSystems();
	loadShaders();
	spawnPlayer();

	// load control bindings (writes a default keymap.conf if none exists)
	Input::LoadKeymap("keymap.conf");

	_gameCamera = std::make_unique<GameCamera>();
	_states.Start(_launch.autoStart ? GameState::InGame : GameState::Splash);

	if (_launch.startStory && !_story.empty())
		startStoryMission(std::min(_launch.storyFrom, static_cast<int>(_story.size()) - 1));

	_coins = std::max(0, std::atoi(_save.Get("coins", "0").c_str()));

	buildMenus();
	buildInGameScreens();
}

void Game::parseArguments(int argc, char** argv)
{
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
	//   --language <E|F|G|I|S>  language of the game's own text (the voices', French on a
	//                        French CD; else English)
	//   --no-movies          no movies at start-up or in the story
	//   --minigame <n>[r]    a bonus game race on track n (reversed with r), the computer
	//                        driving all four; the game quits at its standings
	bool languageGiven = false;
	for (int i = 1; i < argc; ++i)
	{
		const std::string_view arg = argv[i];
		if (arg == "--autostart")
			_launch.autoStart = true;
		else if (arg == "--quit-after" && i + 1 < argc)
			_launch.quitAfterSeconds = std::atof(argv[++i]);
		else if (arg == "--mission" && i + 1 < argc)
		{
			_missionScript = argv[++i];
			_launch.missionScriptGiven = true;
		}
		else if (arg == "--autopilot")
			_autopilot = true;
		else if (arg == "--story")
			_launch.startStory = true;
		else if (arg == "--from" && i + 1 < argc)
			_launch.storyFrom = std::max(0, std::atoi(argv[++i]));
		else if (arg == "--clean")
			_launch.clean = true;
		else if (arg == "--no-movies")
			_launch.noMovies = true;
		else if (arg == "--minigame" && i + 1 < argc)
		{
			const std::string_view track = argv[++i];
			_launch.miniGameDemo = std::clamp(std::atoi(track.data()), 1, 7);
			_launch.miniGameDemoReverse = !track.empty() && (track.back() == 'r' || track.back() == 'R');
		}
		else if (arg == "--level" && i + 1 < argc)
			_levelNumber = std::max(1, std::atoi(argv[++i]));
		else if (arg == "--language" && i + 1 < argc)
		{
			_language = static_cast<char>(std::toupper(static_cast<unsigned char>(argv[++i][0])));
			languageGiven = true;
		}
	}
	// Not given, the language the voices speak: the CD's dialogue library is
	// one language's (DIALOGF.RCF, French), and its subtitles with it; the
	// missions' text, the menus and the movies' soundtracks follow, rather than
	// English text over French voices.
	if (!languageGiven)
	{
		for (const char language : {'F', 'G', 'S'})
			if (FileSystem::exists(fmt::format("art/audio/dialog/DIALOG{}.RCF", language)))
				_language = language;
		// Or the one chosen in the options, kept with the other settings (its
		// letter's code: "language 70" for F).
		std::ifstream settings("donut_settings.dat");
		std::string key;
		float value = 0.0f;
		while (settings >> key >> value)
			if (key == "language" && std::string("EFGIS").find(static_cast<char>(value)) != std::string::npos)
				_language = static_cast<char>(value);
	}
}

void Game::openWindow()
{
	const std::string windowTitle = fmt::format("donut [{0}]", kBuildString);

	const int windowWidth = 1280, windowHeight = 960;
	_window = std::make_unique<Window>(windowTitle, windowWidth, windowHeight);

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);
	glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_FALSE);

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(*_window), static_cast<SDL_GLContext>(*_window));
	ImGui_ImplOpenGL3_Init("#version 130");

	Input::InitGamepad();
}

void Game::loadSystems()
{
	_lineRenderer = std::make_unique<LineRenderer>(1000000);
	_worldPhysics = std::make_unique<WorldPhysics>(_lineRenderer.get());

	// init sub classes
	_audioManager = std::make_unique<AudioManager>();
	// The voices' subtitles, heard by machine (tools/transcribe_dialogue.py).
	_audioManager->LoadSubtitles("audio/dialog/subtitles.tsv");
	loadMovieLines("art/audio/dialog/movie_subtitles.tsv");
	_resourceManager = std::make_unique<ResourceManager>();
	_englishBible.Load("art/frontend/scrooby2/resource/txtbible/srr2.txt", 'E');
	_englishBible.LoadCompiled("art/frontend/scrooby2/resource/txtbible/srr2.p3d", 'E');
	_englishBible.LoadCompiled("art/frontend/scrooby/resource/txtbible/srr2.p3d", 'E');
	loadTextBible();
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

	// The game's loading screen, shown while a level loads: the fuse burning
	// down to the dynamite, "LOADING".
	_loading = std::make_unique<LoadingScreen>(*_window, *_resourceManager);
	// The first level too, from the start.
	_loading->Show(_levelNumber);
	loadLevel();
	_gameInput = std::make_unique<GameInput>();
	Input::CaptureTextEntry(this, &Game::OnInputTextEntry);
}

void Game::loadShaders()
{
	const auto skinVertSrc = File::ReadAll("shaders/skin.vert");
	const auto skinFragSrc = File::ReadAll("shaders/skin.frag");
	_skinShaderProgram = std::make_unique<GL::ShaderProgram>(skinVertSrc, skinFragSrc);

	const auto meshVertSrc = File::ReadAll("shaders/mesh.vert");
	const auto meshFragSrc = File::ReadAll("shaders/mesh.frag");
	_meshShader = std::make_unique<GL::ShaderProgram>(meshVertSrc, meshFragSrc);

	_postProcess = std::make_unique<PostProcess>();
	_shadows = std::make_unique<ShadowMap>();
	_blobShadows = std::make_unique<BlobShadows>();

	_playerMesh = SimpleMesh::CreateCapsule(0.3f, 1.8f, Vector4(0.2f, 1.0f, 0.2f, 1.0f), 12);
	_carMesh = SimpleMesh::CreateBox(Vector3(0.9f, 0.5f, 2.2f), Vector4(0.2f, 0.5f, 1.0f, 1.0f));
}

void Game::spawnPlayer()
{
	runMissionScript();

	loadGlobal();
	LoadModel(_playerCharacter, _playerCharacter);
	// In the outfit last worn on the level.
	if (!_wornSkin.empty())
		BuySkin(_wornSkin);

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
}

void Game::buildMenus()
{
	_mainMenu = std::make_unique<GameMenu>();
	// The credits' background (frontend.p3d's CreditsBgd.pag), over which
	// the text bible's CREDITS_00..31 roll.
	if (FileSystem::exists("./art/frontend/scrooby2/frontend.p3d"))
	{
		_creditsPage = std::make_unique<FrontendProject>();
		_creditsPage->LoadP3D("./art/frontend/scrooby2/frontend.p3d", "CreditsBgd.pag");
		for (int block = 0; !_textBible.Get(fmt::format("CREDITS_{:02}", block)).empty(); ++block)
		{
			// A line a backslash.
			const std::string& text = _textBible.Get(fmt::format("CREDITS_{:02}", block));
			std::size_t from = 0;
			for (std::size_t at = text.find('\\'); ; at = text.find('\\', from))
			{
				_creditLines.push_back(text.substr(from, at == std::string::npos ? std::string::npos : at - from));
				if (at == std::string::npos)
					break;
				from = at + 1;
			}
		}
	}
	// The living room the main menu stands over (the game's 3D frontend).
	_livingRoom = std::make_unique<LivingRoom>("./art/frontend/scrooby2/resource/pure3d/camset.p3d");
	// The game's own main menu page: its arrows, and the entry shown between
	// them in its font.
	if (FileSystem::exists("./art/frontend/scrooby2/frontend.p3d"))
	{
		_mainMenuPage = std::make_unique<FrontendProject>();
		_mainMenuPage->LoadP3D("./art/frontend/scrooby2/frontend.p3d", "MainMenu.pag");
	}
	// Each entry's object in the living room glows while it is picked: the
	// television to go back into the game, the newspaper for a new one, the
	// couch (the options' plate of donuts beside it) for the options; the
	// VCR, the scrapbook and the race track are the load game, scrapbook and
	// mini game's, not here. Quitting has none.
	// Picking up the story where it was left, when it was.
	if (const StoryProgress progress = loadStoryProgress();
	    (progress.level > 1 || progress.mission > 0) && levelAvailable(progress.level))
	{
		// Says where: "Continue - LEVEL 2".
		const std::string& level = _textBible.Get(fmt::format("LEVEL_{}", progress.level));
		const std::string& resume = _textBible.Get("RESUME_GAME");
		const std::string continueLabel = resume.empty() ? "Continue" : resume;
		const std::string label = level.empty() ? continueLabel : continueLabel + " - " + level;
		_mainMenuGlows.push_back("tv");
		_mainMenu->AddButton(label, 0, 0, 200, 40, [this, progress]() {
			if (progress.level != _levelNumber)
				switchLevel(progress.level);
			if (progress.mission < static_cast<int>(_story.size()))
				startStoryMission(progress.mission);
			_states.Fire(GameEvent::Play);
		});
	}
	const std::string& newGame = _textBible.Get("NEW_GAME");
	_mainMenuGlows.push_back("homr");
	_mainMenu->AddButton(newGame.empty() ? "New Game" : newGame, 0, 0, 200, 40, [this]() {
		// The story starts on level 1, whatever level is loaded.
		if (_levelNumber != 1 && levelAvailable(1))
			switchLevel(1);
		if (!_story.empty())
			startStoryMission(0);
		_states.Fire(GameEvent::Play);
		// Level 1's introduction (FMV1A) before its first mission.
		if (!_launch.noMovies && !_autopilot)
			playMovies({"FMV1A.RMV"}, GameState::InGame);
	});
	// Any level the scripts were copied for, from its story's start.
	for (int level = 2; level <= 7; ++level)
	{
		if (!levelAvailable(level))
			continue;
		const std::string& name = _textBible.Get(fmt::format("LEVEL_{}", level));
		_mainMenuGlows.push_back("homr");
		_mainMenu->AddButton(name.empty() ? fmt::format("Level {}", level) : name, 0, 0, 200, 40, [this, level]() {
			if (_levelNumber != level)
				switchLevel(level);
			if (!_story.empty())
				startStoryMission(0);
			_states.Fire(GameEvent::Play);
		});
	}
	// The options: the sound's volumes, the window's size; the credits; the
	// movies, with a player.
	_moviePlayer = std::make_unique<MoviePlayer>();
	const bool movies = MoviePlayer::IsAvailable();
	_optionsMenu = std::make_unique<OptionsMenu>(
	    *_audioManager, *_window,
	    [this]() {
		    _states.Fire(_creditsPage ? GameEvent::ShowCredits : GameEvent::ToMainMenu);
		    _creditsTime = 0.0;
		    if (_creditsPage)
			    _audioManager->PlayMusicEvent("Credits", true);
	    },
	    movies ? std::function<bool(int)>([this](int movie) { return movieUnlocked(movie); }) : nullptr,
	    movies ? std::function<void(int)>([this](int movie) { playMovie(movie); }) : nullptr);
	// At start-up, as the game: the studios' logos, then its introduction;
	// a key skips them all.
	if (_states.Is(GameState::Splash) && !_launch.noMovies)
		playMovies({"RADLOGO.RMV", "FOXLOGO.RMV", "GRACIE.RMV", "VUGLOGO.RMV", "INTRO.RMV"}, GameState::Splash, true);
	_phoneBooth = std::make_unique<PhoneBoothMenu>([this](const std::string& car) { callCar(car); });
	// A shop's item had: an outfit worn, a car left by its seller, the player
	// at its wheel.
	_shop = std::make_unique<ShopMenu>([this](const ShopMenu::Item& item) {
		if (!item.car)
			return BuySkin(item.name);
		if (!BuyCar(item.name))
			return false;
		if (_shopSeller)
		{
			Vehicle& vehicle = _scriptEngine->SummonPlayerCar(item.name, _shopSeller->carSpot);
			_health = 100.0f;
			PutPlayerInVehicle(&vehicle);
		}
		return true;
	});
	// The bonus game, the race track's in the living room.
	_miniMenu = std::make_unique<MiniGameMenu>();
	if (_miniMenu->IsLoaded())
	{
		const std::string& bonus = _textBible.Get("MINI_GAME");
		_mainMenuGlows.push_back("mini");
		_mainMenu->AddButton(bonus.empty() ? "Bonus Game" : bonus, 0, 0, 200, 40, [this]() { openMiniGame(); });
	}
	if (_launch.miniGameDemo > 0 && _miniMenu->IsLoaded())
	{
		openMiniGame();
		SuperSprint::Setup demo;
		demo.track = _launch.miniGameDemo;
		demo.laps = 2;
		demo.reverse = _launch.miniGameDemoReverse;
		const auto cars = miniGameCars();
		const char* characters[] = {"homer", "bart", "lisa", "marge"};
		for (int i = 0; i < SuperSprint::kRacers; ++i)
		{
			demo.entries[i].player = -1;
			demo.entries[i].character = characters[i];
			demo.entries[i].car = cars.empty() ? "famil_v" : cars[i % cars.size()].name;
		}
		startSprint(demo);
	}
	_scrapBook = std::make_unique<ScrapBookMenu>([this](const std::string& file) { return presentationPicture(file); });
	if (_scrapBook->IsLoaded())
	{
		const std::string& book = _textBible.Get("SCRAP_BOOK");
		_mainMenuGlows.push_back("book");
		_mainMenu->AddButton(book.empty() ? "Scrapbook" : book, 0, 0, 200, 40,
		                     [this]() { _scrapBook->Open(scrapBookStats()); });
	}
	if (_optionsMenu->IsLoaded())
	{
		const std::string& options = _textBible.Get("OPTIONS");
		_mainMenuGlows.push_back("sofa");
		_mainMenu->AddButton(options.empty() ? "Options" : options, 0, 0, 200, 40,
		                     [this]() { _optionsMenu->Open(false); });
	}
	const std::string& quitGame = _textBible.Get("QUIT_GAME");
	_mainMenuGlows.push_back("");
	_mainMenu->AddButton(quitGame.empty() ? "Quit" : quitGame, 0, 0, 200, 40, [this]() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	});
}

void Game::buildInGameScreens()
{
	_hud = std::make_unique<Hud>();
	_gameHud = std::make_unique<GameHud>();
	_gameHud->LoadMap(_levelNumber);

	// The game's own pause screen: "PAUSED", and its list, of which Continue,
	// Restart Mission and Quit Game are ours.
	if (FileSystem::exists("./art/frontend/scrooby2/ingame.p3d"))
	{
		_pausePage = std::make_unique<FrontendProject>();
		_pausePage->LoadP3D("./art/frontend/scrooby2/ingame.p3d", "PauseBgd.pag");
		_pausePage->LoadP3D("./art/frontend/scrooby2/ingame.p3d", "Pause.pag");
		_pausePage->LoadP3D("./art/frontend/scrooby2/ingame.p3d", "PauseMission.pag");
	}
	_pauseMenu = std::make_unique<GameMenu>();
	_pauseMenu->AddButton("Resume", 0, 0, 200, 40, [this]() {
		_states.Fire(GameEvent::Play);
	});
	_pauseMenu->AddButton("Restart", 0, 0, 200, 40, [this]() {
		_scriptEngine->CleanupMission();
		if (_collectibleManager) _collectibleManager->SpawnOnPath();
		runMissionScript();
		_states.Fire(GameEvent::Play);
	});
	_pauseMenu->AddButton("Quit", 0, 0, 200, 40, [this]() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	});
	_pauseMenu->AddButton("Options", 0, 0, 200, 40, [this]() {
		if (_optionsMenu)
			_optionsMenu->Open(true);
	});
	// Out of the mission: a bonus mission's (or one played again) back to the
	// story where it was; a story mission's back to the drive before it.
	_pauseMenu->AddButton("Abort", 0, 0, 200, 40, [this]() {
		const int to = abortTo();
		if (to < 0)
			return;
		_bonusReturn = -1;
		startStoryMission(to);
		_states.Fire(GameEvent::Play);
	});
	PauseSundayMenu::Actions sunday;
	sunday.resume = [this]() { _states.Fire(GameEvent::Play); };
	sunday.options = [this]() {
		if (_optionsMenu)
			_optionsMenu->Open(true);
	};
	sunday.quit = []() {
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		SDL_PushEvent(&quitEvent);
	};
	// A mission won played again, as a bonus mission is: the story back where
	// it was after it, won or lost, its progress unsaved.
	sunday.replay = [this](int story) {
		_bonusReturn = _storyIndex;
		startStoryMission(story);
		_states.Fire(GameEvent::Play);
	};
	_pauseSunday = std::make_unique<PauseSundayMenu>(std::move(sunday));
}

int Game::abortTo() const
{
	if (_bonusReturn >= 0)
		return _bonusReturn;
	// The drive to it, just before it in the story.
	if (_storyIndex > 0 && _storyIndex < static_cast<int>(_story.size()))
	{
		const std::string& before = _story[_storyIndex - 1];
		if (before.size() > 7 && before.compare(before.size() - 7, 7, "sdi.mfk") == 0 &&
		    _story[_storyIndex].size() > 7 && _story[_storyIndex].compare(_story[_storyIndex].size() - 7, 7, "sdi.mfk") != 0)
			return _storyIndex - 1;
	}
	return -1;
}

PauseSundayMenu::Stats Game::levelStats(int storyWon, int storyTotal) const
{
	// The level's counts.
	PauseSundayMenu::Stats stats;
	int done = 0, all = 0;
	const auto count = [&](const char* name, int have, int total) {
		stats.counts.emplace_back(name, fmt::format("{}/{}", have, total));
		done += std::min(have, total);
		all += total;
	};
	int bonus = 0, races = 0;
	for (const std::string& won : _save.Entries("won"))
	{
		if (won.compare(0, 3, fmt::format("L{}:", _levelNumber)) != 0)
			continue;
		if (won.compare(3, 2, "bm") == 0)
			++bonus;
		else if (won.compare(3, 2, "sr") == 0)
			++races;
	}
	int cars = 0, carsTotal = 0, skins = 0, skinsTotal = 0;
	for (const Reward& reward : rewards("car"))
		if (reward.level == _levelNumber)
		{
			cars += reward.unlocked;
			++carsTotal;
		}
	for (const Reward& reward : rewards("skin"))
		if (reward.level == _levelNumber && reward.how == "forsale")
		{
			skins += reward.unlocked;
			++skinsTotal;
		}
	const ScrapBookMenu::Stats book = scrapBookStats();
	const int first = (_levelNumber - 1) * 8;
	int cardsFound = 0;
	for (int n = 0; n < 7; ++n)
		cardsFound += book.cardsFound.count(first + n) > 0;
	const int gagsFound = _gags.GetFound();
	count("StoryMissions_Unlocked", storyWon, storyTotal);
	count("BonusMissions_Unlocked", std::min(bonus, 1), 1);
	count("StreetRaces_Unlocked", std::min(races, 3), 3);
	count("Cards_Unlocked", cardsFound, 7);
	count("Clothing_Unlocked", skins, skinsTotal);
	count("Vehicles_Unlocked", cars, carsTotal);
	if (_waspManager)
		count("Wasps_Unlocked", _waspManager->GetDestroyed(), _waspManager->GetTotal());
	count("Gags_Unlocked", gagsFound, _gags.GetTotal());
	stats.percent = all > 0 ? done * 100 / all : 0;
	return stats;
}

void Game::bindWorkedObjects()
{
	_workedObjects.clear();
	const auto& triggers = _level->GetTriggers();
	for (std::size_t i = 0; i < triggers.size(); ++i)
	{
		const auto& trigger = triggers[i];
		const std::string& action = trigger.action;
		if (action != "ToggleOnOff" && action != "AutoPlayAnim" && action != "AutomaticDoor" && action != "PlayAnim")
			continue;
		// What it works, and a switch's lever ("switch1": z6switch1), thrown as
		// it is used: each near it (names repeat about the level: Wiggum's
		// doorbell has a switch4 too), and not worked by another trigger.
		const auto nearTrigger = [&](const std::string& name)
		{
			Vector3 at;
			return !name.empty() && _level->GetAnimatedObjectPosition(name, at) && (at - trigger.position).Length() < 60.0f &&
			       std::none_of(_workedObjects.begin(), _workedObjects.end(),
			                    [&](const WorkedObject& w) { return w.object == name || w.lever == name; });
		};
		WorkedObject worked {i, action, _level->FindAnimatedObject(trigger.locator, trigger.actionTarget), {}};
		if (!nearTrigger(worked.object))
			worked.object.clear();
		if (action == "ToggleOnOff" || action == "PlayAnim")
			worked.lever = _level->FindAnimatedObjectEnding(trigger.actionTarget, worked.object, trigger.position);
		if (!nearTrigger(worked.lever))
			worked.lever.clear();
		if (worked.object.empty() && worked.lever.empty())
			continue;
		// They stand still until worked; a door shut.
		if (action != "PlayAnim" && !worked.object.empty())
			_level->DriveAnimatedObject(worked.object, 0.0f, action != "AutomaticDoor");
		Log::Info("Game: {} '{}' works '{}'{}", action, trigger.locator, worked.object,
		          worked.lever.empty() ? "" : fmt::format(", lever '{}'", worked.lever));
		_workedObjects.push_back(std::move(worked));
	}
}

void Game::updateWorkedObjects()
{
	if (!_character)
		return;
	const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
	const auto& triggers = _level->GetTriggers();
	for (const WorkedObject& worked : _workedObjects)
	{
		const bool inside = triggers[worked.trigger].Contains(me);
		if (worked.object.empty())
			continue;
		if (worked.action == "AutoPlayAnim")
			_level->DriveAnimatedObject(worked.object, inside ? 1.0f : 0.0f, true);
		else if (worked.action == "AutomaticDoor")
			_level->DriveAnimatedObject(worked.object, inside ? 1.0f : -1.0f, false);
	}
}

bool Game::useSwitch()
{
	if (!_character || _inVehicle)
		return false;
	const Vector3 me = _character->GetPosition();
	const auto& triggers = _level->GetTriggers();
	for (const WorkedObject& worked : _workedObjects)
	{
		const auto& trigger = triggers[worked.trigger];
		if ((worked.action != "ToggleOnOff" && worked.action != "PlayAnim") ||
		    !(trigger.Contains(me) || (trigger.position - me).Length() < 2.0f))
			continue;
		if (!worked.lever.empty())
			_level->PlayAnimatedObject(worked.lever);
		if (worked.action == "ToggleOnOff" && !worked.object.empty())
		{
			const float speed = _level->GetAnimatedObjectSpeed(worked.object) != 0.0f ? 0.0f : 1.0f;
			_level->DriveAnimatedObject(worked.object, speed, true);
			Log::Info("Game: switch '{}': '{}' {}", trigger.locator, worked.object, speed != 0.0f ? "on" : "off");
		}
		else if (!worked.object.empty())
			_level->PlayAnimatedObject(worked.object);
		playPlayerAnimation("hit_switch", false);
		return true;
	}
	return false;
}

void Game::tutorial(const std::string& event)
{
	if (!_tutorialMode || _autopilot || _save.Entries("tutorial").count(event) ||
	    std::find(_tutorialWaiting.begin(), _tutorialWaiting.end(), event) != _tutorialWaiting.end())
		return;
	_tutorialWaiting.push_back(event);
}

void Game::updateTutorial()
{
	if (!_tutorialMode || _autopilot || !_character)
		return;
	// What has happened since the last look.
	TutorialSeen& seen = _tutorialSeen;
	if (seen.started < 0.0)
		seen.started = _elapsedSeconds;
	if (_elapsedSeconds - seen.started > 6.0)
		tutorial("start");
	if (_inVehicle != seen.inVehicle)
		tutorial(_inVehicle ? "incar" : "outcar");
	seen.inVehicle = _inVehicle;
	if (!_interior.empty() && !seen.inside)
		tutorial("interior");
	seen.inside = !_interior.empty();
	if (seen.coins >= 0 && _coins > seen.coins)
		tutorial("coin");
	seen.coins = _coins;
	const int cards = _collectibleManager ? _collectibleManager->GetCardsFound() : 0;
	if (seen.cards >= 0 && cards > seen.cards)
		tutorial("card");
	seen.cards = cards;
	const int gags = _gags.GetFound();
	if (seen.gags >= 0 && gags > seen.gags)
		tutorial("gag");
	seen.gags = gags;
	const int wasps = _waspManager ? _waspManager->GetDestroyed() : 0;
	if (seen.wasps >= 0 && wasps > seen.wasps)
		tutorial("camera");
	seen.wasps = wasps;
	if (_inVehicle && _health < 50.0f)
		tutorial("damaged");
	if (_inVehicle && _health <= 0.0f)
		tutorial("broken");
	if (_hitAndRun && _hitAndRun->GetMeter() > 0.0f)
		tutorial("hitrun");
	const bool failed = _states.Is(GameState::MissionFailed);
	if (failed && !seen.failed)
		tutorial("failed");
	seen.failed = failed;
	// Near what the level offers: a bonus mission, a street race, a time
	// trial, Gil's cars, a wardrobe.
	if (const BonusGiver* giver = bonusGiverNear())
		tutorial(giver->mission.rfind("bm", 0) == 0 ? "bonus" : giver->mission.rfind("gr", 0) == 0 ? "timetrial" : "race");
	const Vector3 me = _character->GetPosition();
	for (const CarSeller& seller : _carSellers)
		if ((seller.position - me).Length() < 6.0f)
			tutorial("gil");
	for (const auto& trigger : _level->GetTriggers())
		if (trigger.action == "PurchaseSkin" && (trigger.position - me).Length() < 6.0f)
			tutorial("clothes");

	// The next one waiting, when no one is speaking: Bart's lines for it, the
	// PC's from the Xbox's where the consoles have their own.
	if (_audioManager->IsDialoguePlaying())
		return;
	_walkieTalkie = false;
	if (_tutorialWaiting.empty() || !_states.Is(GameState::InGame))
		return;
	const std::string event = _tutorialWaiting.front();
	_tutorialWaiting.erase(_tutorialWaiting.begin());
	std::vector<std::string> lines;
	for (const std::string& name : {event, event + "xbx"})
		if (lines.empty())
			for (const auto& sound : _audioManager->FindSounds("\\c_" + name + "_"))
				if (sound.find("_tutorial_") != std::string::npos)
					lines.push_back(sound);
	std::sort(lines.begin(), lines.end());
	for (const auto& line : lines)
		_audioManager->QueueDialogue(line, AudioManager::DialoguePriority::Gameplay);
	_walkieTalkie = !lines.empty();
	_save.Add("tutorial", event);
	_save.Save();
	Log::Info("Game: tutorial '{}', {} line(s)", event, lines.size());
}

void Game::openSundayPause()
{
	if (!_pauseSunday || !_pauseSunday->IsLoaded())
		return;
	const ScrapBookMenu::Stats book = scrapBookStats();
	const int first = (_levelNumber - 1) * 8;
	// m1 to m7: won or not, and where each is in the story.
	std::vector<PauseSundayMenu::Mission> missions(7);
	int storyWon = 0, storyTotal = 0;
	for (int n = 1; n <= 7; ++n)
	{
		const std::string file = fmt::format("/m{}i.mfk", n);
		for (int i = 0; i < static_cast<int>(_story.size()); ++i)
			if (_story[i].size() >= file.size() && _story[i].compare(_story[i].size() - file.size(), file.size(), file) == 0)
				missions[n - 1].story = i;
		missions[n - 1].won = book.missionsWon.count(first + n - 1) > 0;
		if (missions[n - 1].story >= 0)
		{
			++storyTotal;
			storyWon += missions[n - 1].won;
		}
	}
	PauseSundayMenu::Stats stats = levelStats(storyWon, storyTotal);
	std::vector<int> cards(7, -1);
	for (int n = 0; n < 7; ++n)
		if (book.cardsFound.count(first + n))
			cards[n] = first + n;
	_pauseSunday->Open(_levelNumber, std::move(missions), std::move(stats), std::move(cards));
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
	_sprint.reset();       // the bonus game's cars
	_sprintDrivers.clear();
	_scriptEngine.reset(); // mission vehicles and the race opponent
	_hitAndRun.reset();    // the police car
	_trafficManager.reset(); // its cars' kinematic bodies
	_character.reset();    // its CharacterController unregisters an action
	_npcs.clear();         // NPCs and pedestrians are Characters too
	_ambientNpcs.clear();
	_bonusGivers.clear();
	_carDrivers.clear();   // the mission cars' drivers, Characters as well
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
	_moves.oneShot = false;
	_moves.idleFor = 0.0;
	_moves.airborneFor = 0.0;
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
	_moves.idleFor = 0.0;
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
		// A truck's own, climbed: get_into_car_high_driver and the like.
		std::string step = (_boarding.in ? kIn : kOut)[_boarding.step];
		if (car.IsHigh())
			step.insert(step.size() - 7, "_high");
		if (playPlayerAnimation(step, false) ||
		    playPlayerAnimation((_boarding.in ? kIn : kOut)[_boarding.step], false))
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
	_moves.oneShot = !loop;
	return true;
}

void Game::updateFootsteps(bool onGround)
{
	if (!_character || !_audioManager || !_worldPhysics)
		return;
	const Vector3 feet = _character->GetPosition() - Vector3(0.0f, kFeetBelowCentre, 0.0f);
	// Whose shoes: the character's own script ("homer"), or the one its costume
	// is a variant of ("h_fat" is Homer's).
	if (_footstepScript.empty())
	{
		static const std::pair<char, const char*> kOwners[] = {
		    {'h', "homer"}, {'b', "bart"}, {'l', "lisa"}, {'m', "marge"}, {'a', "apu"}};
		for (const std::string& name : {_character->GetName(), _character->GetModelName()})
			if (!_audioManager->ScriptSoundFile(name, "feet_concrete").empty())
				_footstepScript = name;
		if (_footstepScript.empty() && !_character->GetModelName().empty())
			for (const auto& [initial, owner] : kOwners)
				if (std::tolower(static_cast<unsigned char>(_character->GetModelName()[0])) == initial)
					_footstepScript = owner;
		if (_footstepScript.empty())
			_footstepScript = "homer";
	}
	const auto step = [&](const char* suffix) {
		const char* ground = "feet_concrete";
		switch (_worldPhysics->GetSurfaceUnder(feet))
		{
		case Surface::Metal: ground = "feet_metal"; break;
		case Surface::Wood: ground = "feet_wood"; break;
		case Surface::Grass:
		case Surface::Dirt:
		case Surface::Sand:
		case Surface::Gravel:
		case Surface::Water: ground = "feet_dirt"; break;
		default: break;
		}
		const auto sound = _audioManager->FindScriptSound(_footstepScript, std::string(ground) + suffix);
		if (!sound.file.empty())
			_audioManager->PlayAudio(sound.file, feet, sound.trim, sound.pitch);
	};

	if (onGround && !_moves.footstepWasOnGround)
	{
		// Down from a jump.
		step("_jump");
		_moves.lastFootstepAt = feet;
	}
	_moves.footstepWasOnGround = onGround;
	if (!onGround)
		return;
	// A stride's way along the ground since the last step (driven or not: the
	// autopilot walks without the controls).
	const Vector3 gone = feet - _moves.lastFootstepAt;
	if (gone.X * gone.X + gone.Z * gone.Z < kStride * kStride)
		return;
	// Put back somewhere else (out of a car, into a building): no step for it.
	if (gone.X * gone.X + gone.Z * gone.Z > 4.0f * kStride * kStride)
	{
		_moves.lastFootstepAt = feet;
		return;
	}
	step("");
	_moves.lastFootstepAt = feet;
}

void Game::updatePlayerAnimation(double dt, bool moving, bool onGround)
{
	if (_boarding.car)
		return;
	// The idle ones: Homer scratches, checks his shoes, dances.
	static const char* const kIdles[] = {"idle_scratch", "idle_bumshake", "idle_check_shoes", "idle_dancing",
	                                     "idle_donut",   "idle_fat",      "idle_chili",       "idle_scuzzy"};

	_moves.airborneFor = onGround ? 0.0 : _moves.airborneFor + dt;
	const bool wasFalling = _moves.airborneFor == 0.0 && _character->GetAnimName() == _playerAnimations["jump_idle_in_air"];

	// In the air for more than a moment (a drop off a kerb is not a jump), or
	// off the ground once a jump's take-off has played.
	const bool tookOff = _character->GetAnimName() == _playerAnimations["jump_idle_take_off"] &&
	                     _character->IsAnimationFinished();
	const bool falling = _character->GetAnimName() == _playerAnimations["jump_idle_in_air"];
	// A kick plays through before the fall takes over.
	const bool kicking = _character->GetAnimName() == _playerAnimations["jump_kick"] && !_character->IsAnimationFinished();
	if (kicking && !onGround)
		return;
	if (_moves.airborneFor > 0.25 || (!onGround && (tookOff || falling)))
	{
		_moves.idleFor = 0.0;
		playPlayerAnimation("jump_idle_in_air", true);
		return;
	}
	if (wasFalling)
	{
		if (_dust)
			_dust->Burst(10, _character->GetPosition() - Vector3(0.0f, 0.9f, 0.0f));
		playPlayerAnimation("jump_idle_land", false);
		return;
	}
	// A one-shot plays through, unless the player sets off: a landing or an
	// idle is cut short by moving, a victory too.
	if (_moves.oneShot && !_character->IsAnimationFinished() && !moving)
		return;
	if (moving)
	{
		_moves.idleFor = 0.0;
		if (!(_moves.dashing && playPlayerAnimation("loco_dash", true)) && !playPlayerAnimation("loco_run", true))
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
	if (company && _audioManager->IsDialoguePlaying() && _elapsedSeconds >= _moves.gestureAt)
	{
		std::vector<std::string> gestures;
		for (const auto& [what, name] : _playerAnimations)
			if (what.rfind("dialogue_", 0) == 0)
				gestures.push_back(what);
		_moves.gestureAt = _elapsedSeconds + 2.0 + (rand() % 15) / 10.0;
		_moves.idleFor = 0.0;
		if (!gestures.empty() && playPlayerAnimation(gestures[static_cast<std::size_t>(rand()) % gestures.size()], false))
			return;
	}

	_moves.idleFor += dt;
	if (_moves.idleFor > 8.0)
	{
		_moves.idleFor = 0.0;
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

void Game::drawPausePage(const GameIntent& intent, const Matrix4x4& proj)
{
	// The page's entries, top to bottom, and which of ours each is (-1: none,
	// shown greyed and passed over).
	static const std::pair<const char*, int> kEntries[] = {
	    {"Continue", 0}, {"RestartMission", 1}, {"AbortMission", 4}, {"Options", 3}, {"QuitGame", 2}};
	// Abort only where there is somewhere to go back to.
	const bool canAbort = abortTo() >= 0;
	constexpr int kCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
	const auto step = [&](int from, int by) {
		int at = from;
		for (int i = 0; i < kCount; ++i)
		{
			at = (at + by + kCount) % kCount;
			if (kEntries[at].second >= 0 && (kEntries[at].second != 4 || canAbort))
				return at;
		}
		return from;
	};
	if (kEntries[_pauseChoice].second < 0 || (kEntries[_pauseChoice].second == 4 && !canAbort))
		_pauseChoice = step(_pauseChoice, 1);
	const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
	if (up && !_menuUpHeld)
		_pauseChoice = step(_pauseChoice, -1);
	if (down && !_menuDownHeld)
		_pauseChoice = step(_pauseChoice, 1);
	_menuUpHeld = up;
	_menuDownHeld = down;
	// The mouse over an entry picks it.
	bool clickedEntry = false;
	for (int i = 0; i < kCount; ++i)
	{
		float x, y, w, h;
		if (kEntries[i].second >= 0 && (kEntries[i].second != 4 || canAbort) &&
		    _pausePage->GetRect(kEntries[i].first, x, y, w, h) && intent.mouseX >= x &&
		    intent.mouseX <= x + w && intent.mouseY >= y && intent.mouseY <= y + h)
		{
			// Moved over it, not merely left there when the screen opened.
			if (_pauseMouseX >= 0 && (intent.mouseX != _pauseMouseX || intent.mouseY != _pauseMouseY))
				_pauseChoice = i;
			clickedEntry = clickedEntry || (intent.uiClick && _pauseChoice == i);
		}
	}
	_pauseMouseX = intent.mouseX;
	_pauseMouseY = intent.mouseY;
	for (int i = 0; i < kCount; ++i)
	{
		if (kEntries[i].second < 0 || (kEntries[i].second == 4 && !canAbort))
			_pausePage->SetTextColor(kEntries[i].first, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		else if (i == _pauseChoice)
			_pausePage->SetTextColor(kEntries[i].first, Vector4(1.0f, 0.84f, 0.0f, 1.0f));
		else
			_pausePage->ResetTextColor(kEntries[i].first);
	}
	_pausePage->Draw(proj);
	if (intent.uiConfirm || clickedEntry)
	{
		const int button = kEntries[_pauseChoice].second;
		if (button >= 0 && button < static_cast<int>(_pauseMenu->GetButtons().size()) &&
		    _pauseMenu->GetButtons()[button].action)
		{
			_pauseChoice = 0;
			_pauseMouseX = _pauseMouseY = -1;
			_pauseMenu->GetButtons()[button].action();
		}
	}
}

void Game::drawTransition(const Matrix4x4& proj, int width, int height)
{
	float closed = 0.0f;
	const ScriptEngine::Transition transition = _scriptEngine->GetTransition(closed);
	if (transition == ScriptEngine::Transition::None || closed <= 0.0f)
		return;
	// Black, and an iris: a disc clear in a black square, as wide as the
	// screen's diagonal when open and none when shut, black about it.
	if (!_blackTexture)
	{
		const uint8_t pixel[] = {0, 0, 0, 255};
		_blackTexture = std::make_unique<Texture>(pixel, 1, 1, "transition_black");
		constexpr int kSize = 256;
		std::vector<uint8_t> iris(kSize * kSize * 4, 0);
		for (int y = 0; y < kSize; ++y)
			for (int x = 0; x < kSize; ++x)
			{
				const float dx = x + 0.5f - kSize * 0.5f, dy = y + 0.5f - kSize * 0.5f;
				const float edge = std::sqrt(dx * dx + dy * dy) - (kSize * 0.5f - 2.0f);
				iris[(y * kSize + x) * 4 + 3] = static_cast<uint8_t>(std::clamp(edge, 0.0f, 1.0f) * 255.0f);
			}
		_irisTexture = std::make_unique<Texture>(iris.data(), kSize, kSize, "transition_iris");
	}
	SpriteBatch batch;
	if (transition == ScriptEngine::Transition::Fade)
		batch.Draw(_blackTexture.get(), Vector2(0.0f, 0.0f), Vector2(static_cast<float>(width), static_cast<float>(height)),
		           Vector4(1.0f, 1.0f, 1.0f, closed));
	else
	{
		const float radius = (1.0f - closed) * std::sqrt(static_cast<float>(width * width + height * height)) * 0.5f;
		const float cx = width * 0.5f, cy = height * 0.5f;
		const float left = cx - radius, top = cy - radius, right = cx + radius, bottom = cy + radius;
		const Vector4 white(1.0f, 1.0f, 1.0f, 1.0f);
		batch.Draw(_irisTexture.get(), Vector2(left, top), Vector2(radius * 2.0f, radius * 2.0f), white);
		const float w = static_cast<float>(width), h = static_cast<float>(height);
		if (top > 0.0f)
			batch.Draw(_blackTexture.get(), Vector2(0.0f, 0.0f), Vector2(w, top), white);
		if (bottom < h)
			batch.Draw(_blackTexture.get(), Vector2(0.0f, bottom), Vector2(w, h - bottom), white);
		if (left > 0.0f)
			batch.Draw(_blackTexture.get(), Vector2(0.0f, top), Vector2(left, bottom - top), white);
		if (right < w)
			batch.Draw(_blackTexture.get(), Vector2(right, top), Vector2(w - right, bottom - top), white);
	}
	batch.Flush(proj);
}

void Game::drawSubtitle(const Matrix4x4& proj, int width, int height)
{
	if (_optionsMenu && !_optionsMenu->AreSubtitlesShown())
		return;
	if (const AudioManager::Subtitle* subtitle = _audioManager->GetSubtitle())
		drawCaption(subtitle->speaker, subtitle->text, proj, width, height);
}

void Game::drawCaption(const std::string& speaker, const std::string& text, const Matrix4x4& proj, int width,
                       int height)
{
	const Font* font = _resourceManager->GetFont("boulder_16");
	if (!font)
		return;
	// Low on the screen, in the middle, as many lines as it takes within
	// four fifths of its width, the speaker's name before it in yellow; a
	// shadow under it to read it over anything.
	const std::string prefix = speaker.empty() ? "" : speaker + ": ";
	// Clear of the game HUD's radar, bottom right: half the screen wide, left
	// of its middle; over a movie, the screen's whole width.
	const bool radar = _gameHud && _gameHud->IsLoaded() && !_states.Is(GameState::Movie);
	const float most = width * (radar ? 0.5f : 0.8f);
	const float middle = width * (radar ? 0.42f : 0.5f);
	std::vector<std::string> lines(1, prefix);
	std::istringstream words(text);
	for (std::string word; words >> word;)
	{
		const std::string longer = lines.back().empty() || lines.back() == prefix ? lines.back() + word : lines.back() + " " + word;
		if (font->MeasureWidth(longer) > most && lines.back() != prefix && !lines.back().empty())
			lines.push_back(word);
		else
			lines.back() = longer;
	}
	const float lineHeight = font->GetHeight() + 4.0f;
	// Above the objective's box while it is up.
	const float bottom = _gameHud && _gameHud->IsObjectiveShown() && !_states.Is(GameState::Movie) ? 0.66f : 0.86f;
	float y = height * bottom - lineHeight * (lines.size() - 1);
	SpriteBatch batch;
	for (std::size_t i = 0; i < lines.size(); ++i, y += lineHeight)
	{
		const std::string& line = lines[i];
		const float x = middle - font->MeasureWidth(line) * 0.5f;
		batch.DrawText(font, line, Vector2(x + 2.0f, y + 2.0f), Vector4(0.0f, 0.0f, 0.0f, 0.8f));
		if (i == 0 && !prefix.empty())
		{
			batch.DrawText(font, prefix, Vector2(x, y), Vector4(1.0f, 0.84f, 0.0f, 1.0f));
			batch.DrawText(font, line.substr(prefix.size()), Vector2(x + font->MeasureWidth(prefix), y),
			               Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		else
			batch.DrawText(font, line, Vector2(x, y), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	batch.Flush(proj);
}

void Game::drawCredits(const GameIntent& intent, const Matrix4x4& proj, double dt, int width, int height)
{
	_creditsTime += dt;
	_creditsPage->Draw(proj);
	const Font* font = _resourceManager->GetFont("boulder_16");
	if (!font)
		return;
	// Up from the bottom of the screen, a line at a time.
	const float line = font->GetHeight() + 6.0f;
	const float top = height - static_cast<float>(_creditsTime) * kCreditsSpeed;
	SpriteBatch batch;
	for (std::size_t i = 0; i < _creditLines.size(); ++i)
	{
		const float y = top + i * line;
		if (y < -line || y > height)
			continue;
		const std::string& text = _creditLines[i];
		batch.DrawText(font, text, Vector2((width - font->MeasureWidth(text)) * 0.5f, y), Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	batch.Flush(proj);
	// Over when the last line has gone, or skipped; not on the frame it
	// begins.
	const bool over = top + _creditLines.size() * line < 0.0f;
	if (over || (_creditsTime > 0.5 && (intent.uiConfirm || intent.pauseToggle)))
	{
		_states.Fire(GameEvent::ToMainMenu);
		_menuShown = false;
	}
}

namespace
{
// View Movies' movies (movies/, as the CDs have them): the introduction, then
// Movie1 to Movie8.
const char* const kMovieFiles[] = {"INTRO", "FMV1A", "FMV2", "FMV3", "FMV4", "FMV5", "FMV6", "FMV7", "FMV8"};
} // namespace

bool Game::movieUnlocked(int movie)
{
	const StoryProgress progress = loadStoryProgress();
	const std::size_t cards = _save.Entries("card").size();
	const std::string reckoned = fmt::format("{} {} {}", progress.level, progress.mission, cards);
	if (reckoned != _moviesUnlockedFor)
	{
		_moviesUnlockedFor = reckoned;
		// The introduction and level 1's, which open the game, always. A
		// level's movie once the story is past the step that plays it (its
		// script's SetFMVInfo("fmv2.rmv"), in a mission's or in the drive to
		// it); the bonus movie, the Itchy & Scratchy Movie, with all 49 cards
		// found, as its ticket is.
		_moviesUnlocked.assign(std::size(kMovieFiles), false);
		_moviesUnlocked[0] = _moviesUnlocked[1] = true;
		_moviesUnlocked[8] = cards >= 49;
		for (int level = 1; level <= 7; ++level)
		{
			const std::string directory = fmt::format("scripts/level{:02}", level);
			std::ifstream script(directory + "/level.mfk");
			int entry = 0;
			for (std::string line; std::getline(script, line);)
			{
				const std::size_t at = line.find("AddMission(");
				const std::size_t comment = line.find("//");
				const std::size_t open = line.find('"', at);
				const std::size_t close = open == std::string::npos ? open : line.find('"', open + 1);
				if (at == std::string::npos || (comment != std::string::npos && comment < at) || close == std::string::npos)
					continue;
				const std::string id = line.substr(open + 1, close - open - 1);
				// The drive to it, then the mission, each a step of the story.
				for (const std::string& part : {id + "sd", id})
				{
					std::ifstream file(fmt::format("{}/{}i.mfk", directory, part));
					if (!file)
						continue;
					const bool passed = level < progress.level || (level == progress.level && entry < progress.mission);
					for (std::string text; std::getline(file, text);)
					{
						const std::size_t fmv = text.find("SetFMVInfo(\"");
						const std::size_t remark = text.find("//");
						if (fmv == std::string::npos || (remark != std::string::npos && remark < fmv))
							continue;
						std::string name = text.substr(fmv + 12, text.find('"', fmv + 12) - fmv - 12);
						std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
						for (std::size_t i = 2; i + 1 < std::size(kMovieFiles); ++i)
							if (name == std::string(kMovieFiles[i]) + ".RMV" && passed)
								_moviesUnlocked[i] = true;
					}
					++entry;
				}
			}
		}
	}
	return movie >= 0 && movie < static_cast<int>(_moviesUnlocked.size()) && _moviesUnlocked[movie];
}

void Game::playMovie(int movie)
{
	if (movie >= 0 && movie < static_cast<int>(std::size(kMovieFiles)))
		playMovies({fmt::format("{}.RMV", kMovieFiles[movie])}, GameState::MainMenu);
}

bool Game::PlayStoryMovie(const std::string& file, bool stopMusic)
{
	if (_launch.noMovies || _autopilot || !playMovies({file}, GameState::InGame))
		return false;
	_movies.stopMusic = stopMusic;
	return true;
}

bool Game::IsMoviePlaying() const
{
	return _states.Is(GameState::Movie);
}

bool Game::playMovies(std::vector<std::string> files, GameState after, bool skipTogether)
{
	// The first that opens: its soundtrack in the game's language
	// (English, French, German, Spanish), at the music's volume. The
	// scripts' names are lower case, the CDs' upper.
	if (!_moviePlayer)
		return false;
	const int track = _language == 'F' ? 1 : _language == 'G' || _language == 'D' ? 2 : _language == 'S' ? 3 : 0;
	while (!files.empty())
	{
		std::string upper = files.front();
		std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
		files.erase(files.begin());
		const std::string path = "movies/" + upper;
		if (!FileSystem::exists(path) ||
		    !_moviePlayer->Open(path, track, _audioManager->GetChannelVolume(AudioManager::Channel::Music)))
			continue;
		// A movie with fewer soundtracks plays its first.
		_movies.playing = {upper, _movieLines.count({upper, track}) ? track : 0};
		if (!_states.Is(GameState::Movie))
		{
			_movies.after = after;
			_movies.skipTogether = skipTogether;
			_movies.stopMusic = false;
			_audioManager->StopMusic();
			_states.Fire(GameEvent::PlayMovie);
		}
		_movies.next = std::move(files);
		return true;
	}
	return false;
}

void Game::loadMovieLines(const std::string& path)
{
	std::ifstream table(path);
	std::size_t count = 0;
	for (std::string line; std::getline(table, line);)
	{
		std::istringstream fields(line);
		std::string movie, track, start, end, text;
		if (!std::getline(fields, movie, '\t') || !std::getline(fields, track, '\t') ||
		    !std::getline(fields, start, '\t') || !std::getline(fields, end, '\t') || !std::getline(fields, text))
			continue;
		text = AudioManager::ToLatin1(text);
		if (text.empty())
			continue;
		_movieLines[{movie, std::atoi(track.c_str())}].push_back({std::atof(start.c_str()), std::atof(end.c_str()), text});
		++count;
	}
	if (count > 0)
		Log::Info("Game: {} movie lines' subtitles from {}", count, path);
}

void Game::drawMovie(const GameIntent& intent, const Matrix4x4& proj, double dt, int width, int height)
{
	_moviePlayer->Update(dt);
	_moviePlayer->Draw(proj, width, height);
	// What is said now, subtitled as the game's lines are.
	if (!_optionsMenu || _optionsMenu->AreSubtitlesShown())
		if (const auto lines = _movieLines.find(_movies.playing); lines != _movieLines.end())
		{
			const double now = _moviePlayer->GetTime();
			for (const MovieLine& line : lines->second)
				if (now >= line.start && now <= line.end + 0.6)
				{
					drawCaption("", line.text, proj, width, height);
					break;
				}
		}
	// Over at its end, or skipped (not on the frame it begins): the next, or
	// back to where it was played from. To View Movies with the show's
	// theme; to the game with the level's music, unless the movie stopped it.
	const bool skipped = _moviePlayer->GetTime() > 0.5 && (intent.uiConfirm || intent.pauseToggle);
	if (_moviePlayer->IsPlaying() && !skipped)
		return;
	_moviePlayer->Stop();
	if (skipped && _movies.skipTogether)
		_movies.next.clear();
	if (!_movies.next.empty() && playMovies(std::move(_movies.next), _movies.after, _movies.skipTogether))
		return;
	_states.Fire(eventTo(_movies.after));
	if (_movies.after == GameState::MainMenu)
		_menuMusic = false;
	else if (_movies.after == GameState::InGame && !_movies.stopMusic)
		_audioManager->ResumeMusicBase();
}

Texture* Game::presentationPicture(const std::string& path)
{
	if (path.empty())
		return nullptr;
	// A sprite of DDS tiles, named after its file: "mis01_01.png".
	const std::string file = path.substr(path.find_last_of('/') + 1);
	const std::string name = file.substr(0, file.find('.')) + ".png";
	if (Texture* texture = _resourceManager->GetTexture(name))
		return texture;
	if (_missingPictures.count(name) || !FileSystem::exists(path))
	{
		_missingPictures.insert(name);
		return nullptr;
	}
	const P3D::P3DFile p3d(path);
	for (const auto& chunk : p3d.GetRoot().GetChildren())
		if (chunk->IsType(P3D::ChunkType::Sprite))
			_resourceManager->LoadTexture(*P3D::Sprite::Load(*chunk));
	Texture* texture = _resourceManager->GetTexture(name);
	if (!texture)
		_missingPictures.insert(name);
	return texture;
}

void Game::scriptedCamera(double deltaTime)
{
	ScriptEngine::CameraMove move;
	if (_scriptEngine->TakeCameraMove(move))
		_gameCamera->PlayMove(
		    AnimCamera::LoadP3D(fmt::format("art/missions/{}/{}.p3d", levelDirectory(), move.file), move.camera));
	if (_gameCamera->UpdateMove(deltaTime))
		return;

	ScriptEngine::ConversationShot shot;
	if (!_character || _inVehicle || !_scriptEngine->GetConversationShot(shot))
		return;
	// The one talked to: a mission's character, or one standing about (a race's
	// or a bonus mission's giver).
	const Character* other = nullptr;
	for (const auto* people : {&_npcs, &_ambientNpcs})
		if (const auto found = people->find(shot.other); found != people->end() && !other)
			other = found->second.get();
	if (!other)
		return;
	// Over the shoulder of the one facing the subject: "pc" frames the player's
	// character, "npc" the other; "far" takes in both, "near" the subject alone.
	const bool player = shot.shot.rfind("pc", 0) == 0;
	// A head's height over the capsule's middle (0.875m over the feet).
	const Vector3 head(0.0f, 0.6f, 0.0f);
	const Vector3 subject = (player ? _character->GetPosition() : other->GetPosition()) + head;
	const Vector3 facing = (player ? other->GetPosition() : _character->GetPosition()) + head;
	_gameCamera->FrameConversation(subject, facing, shot.shot.find("near") != std::string::npos, player,
	                               shot.hasBestSide ? &shot.bestSide : nullptr);
	if (const std::string now = shot.shot + "/" + shot.other; now != _conversationShot)
	{
		_conversationShot = now;
		Log::Info("Game: conversation shot '{}' with '{}'", shot.shot, shot.other);
	}
}

void Game::SetLanguage(char language)
{
	if (language == _language)
		return;
	_language = language;
	loadTextBible();
	// The pages loaded already show their text again from it.
	FrontendProject::RefreshAllTexts();
	Log::Info("Game: the game's text in {} from now on", GameLanguageName(_language));
}

void Game::loadTextBible()
{
	// The export (its rows by term), then both compiled bibles for the PC's
	// own terms: scrooby2's, which the pages come from, has most of them in
	// French as "???"; scrooby's has them translated.
	_textBible = TextBible();
	_textBible.Load("art/frontend/scrooby2/resource/txtbible/srr2.txt", _language);
	_textBible.LoadCompiled("art/frontend/scrooby2/resource/txtbible/srr2.p3d", _language);
	_textBible.LoadCompiled("art/frontend/scrooby/resource/txtbible/srr2.p3d", _language);
	if (_language != 'E')
		_textBible.SetFallback(&_englishBible);
}

GameEvent Game::eventTo(GameState state)
{
	switch (state)
	{
	case GameState::InGame: return GameEvent::Play;
	case GameState::Splash: return GameEvent::ShowSplash;
	case GameState::Credits: return GameEvent::ShowCredits;
	case GameState::LevelEnd: return GameEvent::LevelOver;
	default: return GameEvent::ToMainMenu;
	}
}

void Game::AddShake(float amount)
{
	_gameCamera->AddShake(amount);
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
	_carDamage.lookAge += dt;
	if (model == _carDamage.lookModel && stage == _carDamage.lookStage && (stage == 0 || _carDamage.lookAge < 2.0))
		return;
	_carDamage.lookAge = 0.0;

	auto& resources = GetResourceManager();
	auto show = [&](const std::string& car, int parts) {
		for (int i = 0; i < 4; ++i)
		{
			const std::string norm = car + kParts[i] + "Norm.bmp";
			resources.ShowTextureAs(norm, i < parts ? car + kParts[i] + "Dam.bmp" : norm);
		}
	};
	// The car left behind is mended, to the eye.
	if (!_carDamage.lookModel.empty() && _carDamage.lookModel != model)
		show(_carDamage.lookModel, 0);
	if (!model.empty())
		show(model, stage);
	if (model != _carDamage.lookModel || stage != _carDamage.lookStage)
		Log::Debug("Game: '{}' shows {} damaged part(s), health {:.0f}", model, stage, _health);
	_carDamage.lookModel = model;
	_carDamage.lookStage = stage;
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
		_carDamage.vehicle = nullptr;
		return;
	}
	const Vector3 velocity = _activeVehicle->GetVelocity();
	const Vector3 position = _activeVehicle->GetPosition();
	if (_carDamage.vehicle == _activeVehicle)
	{
		// A car moved further than its speed explains was teleported, not hit.
		const float moved = (position - _carDamage.position).Length();
		const float flat = Vector3(velocity.X - _carDamage.velocity.X, 0.0f, velocity.Z - _carDamage.velocity.Z).Length();
		if (moved < 5.0f && flat > kImpactThreshold)
		{
			// As tough as the car's script makes it (SetHitPoints).
			const float damage = (flat - kImpactThreshold) * kDamagePerMs * _activeVehicle->GetDamageScale();
			_health = std::max(0.0f, _health - damage);
			AddShake(std::min(1.0f, flat * 0.05f));
			if (_sparks)
				_sparks->Burst(static_cast<int>(std::min(30.0f, flat * 2.0f)), position + Vector3(0.0f, 0.5f, 0.0f));
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
	_carDamage.vehicle = _activeVehicle;
	_carDamage.velocity = velocity;
	_carDamage.position = position;
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
	_messages.alert = text.empty() ? fallback : text;
	_messages.alertUntil = _elapsedSeconds + 3.0;
	_messages.alertTerm = term;
}

void Game::updateHitAndRun(double dt)
{
	// Indoors the police wait. Nor do they chase --autopilot, whose teleports
	// across a target's path are not the player's driving: in the unattended
	// level 4 run they rammed the traffic into a police chase.
	// The chase's music gives way when the chase is over, however it ended:
	// the police evaded, lost indoors, the meter reset.
	if (_hitAndRunMusic && (!_hitAndRun || !_hitAndRun->IsChasing()))
	{
		_hitAndRunMusic = false;
		_audioManager->ResumeMusicBase();
	}
	// The police cars' siren (their horn, the sound script's "siren"), louder
	// as the nearest comes; none indoors, none once they have given up.
	{
		static const std::string siren = [this] {
			const auto found = _audioManager->FindSounds("carsound\\siren.rsd");
			return found.empty() ? std::string() : found.front();
		}();
		const bool heard = _hitAndRun && _hitAndRun->IsChasing() && _interior.empty() && _character && !siren.empty();
		const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
		                                                : (_character ? _character->GetPosition() : Vector3::Zero);
		const float nearest = heard ? _hitAndRun->NearestCop(me) : 1.0e9f;
		_audioManager->SetLoop("siren", heard && nearest < 200.0f ? siren : std::string(),
		                       std::clamp(1.0f - nearest / 200.0f, 0.15f, 0.83f));
	}
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
	if (_scriptEngine->TakeHitNRun())
		_hitAndRun->Add(100.0f, "the mission");

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
	// Those in the way of a car coming at them run clear of it.
	if (driving && std::abs(speed) > 20.0f && _pedestrianManager && _interior.empty())
		_pedestrianManager->Scare(player, 7.0f, _activeVehicle->GetVelocity());
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
		_hitAndRunMusic = _audioManager->PlayMusicEvent("Hit_and_Run", true);
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
		// A mission under way plays its own ending.
		_hitAndRunMusic = false;
		if (!_audioManager->PlayMusicEvent("Hit_and_Run_Caught", true))
			_audioManager->ResumeMusicBase();
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
	_wornSkin = skin;
	savePurchases();
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

bool Game::IsPlayerInBossBeam() const
{
	return _ufo && _inVehicle && _activeVehicle && _interior.empty() && _ufo->InBeam(_activeVehicle->GetPosition());
}

bool Game::GetBossGround(Vector3& position) const
{
	if (!_ufo || _ufo->IsDestroyed())
		return false;
	position = _ufo->GetGroundPosition();
	return true;
}

void Game::HitBoss(bool last)
{
	if (!_ufo)
		return;
	// The barrel goes up the beam from the car; it blows up against the hull
	// as it gets there (bossBlast).
	const Vector3 from = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
	                                                  : (_character ? _character->GetPosition() : _ufo->GetGroundPosition());
	_ufo->Launch(from, last);
}

void Game::bossBlast(const Vector3& blast, bool last)
{
	if (_sparks)
		_sparks->Burst(last ? 60 : 30, blast);
	if (_dust)
		_dust->Burst(last ? 30 : 15, blast);
	AddShake(last ? 1.0f : 0.6f);
	_audioManager->PlayAudio(last ? "sound\\soundfx\\collisions\\car_explosion_04b.rsd"
	                              : "sound\\soundfx\\collisions\\car_explosion_01b.rsd",
	                         blast);
}

ScrapBookMenu::Stats Game::scrapBookStats() const
{
	ScrapBookMenu::Stats stats;
	stats.cards = static_cast<int>(_save.Entries("card").size());
	// Each card found by its name, "card<level><n>" (card12: level 1's
	// second), at its place in the deck: eight a level.
	for (const std::string& line : _save.Entries("card"))
		if (line.size() >= 6 && line.compare(0, 4, "card") == 0 && std::isdigit(line[4]) && std::isdigit(line[5]) &&
		    line[4] >= '1' && line[5] >= '1')
			stats.cardsFound.insert((line[4] - '1') * 8 + (line[5] - '1'));
	stats.gags = static_cast<int>(_save.Entries("gag").size());
	stats.wasps = static_cast<int>(_save.Entries("wasp").size());
	// The bonus missions and street races won ("L1:bm1", "L2:sr3"); the cars
	// and clothes bought, every level's.
	stats.bonusMissions = 0;
	stats.streetRaces = 0;
	for (const std::string& won : _save.Entries("won"))
	{
		const std::string id = won.substr(won.find(':') + 1);
		if (id.compare(0, 2, "bm") == 0)
			++*stats.bonusMissions;
		else if (id.compare(0, 2, "sr") == 0)
			++*stats.streetRaces;
		// Its place in the missions' gallery: the level's eighth.
		if (id.compare(0, 2, "bm") == 0 && won.size() > 1 && std::isdigit(won[1]))
			stats.missionsWon.insert((won[1] - '1') * 8 + 7);
	}
	// The vehicles, rewards.mfk's BindReward("<car>", "<file>", "car",
	// "<how>", <level>...): a level's own car is the player's from the start;
	// the bonus mission's with the bonus mission won; the street races' with
	// all three won; those for sale bought. The clothes, its "skin" ones for
	// sale, three a level, bought.
	stats.vehicles = 0;
	stats.clothing = 0;
	for (const Reward& reward : rewards("skin"))
		if (reward.how == "forsale")
		{
			stats.skinList.emplace_back(reward.car, reward.unlocked);
			*stats.clothing += reward.unlocked;
		}
	for (const Reward& reward : rewards("car"))
	{
		stats.vehicleList.emplace_back(reward.car, reward.unlocked);
		*stats.vehicles += reward.unlocked;
	}
	// The story's missions: each level's AddMission list. The save
	// keeps the level reached and where in its story, the drives to the
	// missions (<id>sd) counted in, as startStoryMission plays them.
	const StoryProgress progress = loadStoryProgress();
	for (int level = 1; level <= 7; ++level)
	{
		const std::string directory = fmt::format("level{:02}", level);
		std::ifstream script(fmt::format("scripts/{}/level.mfk", directory));
		int entry = 0;
		for (std::string line; std::getline(script, line);)
		{
			const std::size_t at = line.find("AddMission(");
			const std::size_t comment = line.find("//");
			const std::size_t open = line.find('"', at);
			const std::size_t close = open == std::string::npos ? open : line.find('"', open + 1);
			if (at == std::string::npos || (comment != std::string::npos && comment < at) || close == std::string::npos)
				continue;
			const std::string id = line.substr(open + 1, close - open - 1);
			++stats.storyMissionsTotal;
			if (FileSystem::exists(fmt::format("scripts/{}/{}sdi.mfk", directory, id)))
				++entry;
			// Won: on a level before the one reached, or past it in its story.
			if (level < progress.level || (level == progress.level && entry < progress.mission))
			{
				++stats.storyMissions;
				// m1 to m7 have their places in the gallery.
				if (id.size() == 2 && id[0] == 'm' && id[1] >= '1' && id[1] <= '7')
					stats.missionsWon.insert((level - 1) * 8 + (id[1] - '1'));
			}
			++entry;
		}
	}
	return stats;
}

void Game::updateLivingRoomCamera()
{
	if (!_livingRoom || !_optionsMenu)
		return;
	// The room's camera flight (FE_CameraController) is a string of round
	// trips, each 60 frames: out from the room's view, or from the options'
	// plate of donuts, to an object, and back. A page opening plays the way
	// out, closing it the way back.
	// Out to `to`, back from `back` (where the way out is the way back) to
	// the trip's end.
	struct Trip
	{
		OptionsMenu::Screen screen;
		float from, to, back, end;
	};
	static const Trip kTrips[] = {
	    {OptionsMenu::Screen::Options, 80.0f, 110.0f, 110.0f, 140.0f},  // the room to the donuts
	    {OptionsMenu::Screen::Display, 460.0f, 490.0f, 490.0f, 520.0f}, // the donuts to the television's screen
	    // To the game pad by the television: at the trip's middle (560) the
	    // camera is against it, the view all purple; three frames before,
	    // the pad fills it.
	    {OptionsMenu::Screen::Controller, 530.0f, 557.0f, 563.0f, 590.0f},
	    {OptionsMenu::Screen::Sound, 660.0f, 690.0f, 690.0f, 720.0f}, // to Krusty's radio
	    {OptionsMenu::Screen::Movies, 460.0f, 490.0f, 490.0f, 520.0f}, // the television, as Display
	};
	const auto trip = [](OptionsMenu::Screen screen) -> const Trip* {
		for (const Trip& t : kTrips)
			if (t.screen == screen)
				return &t;
		return nullptr;
	};
	// The scrapbook: out to the album (321-350), its pages read there, the
	// border and its sparkles about them, and back (350-380).
	if (_scrapBook)
	{
		const ScrapBookMenu::Screen book = _scrapBook->GetScreen();
		const bool open = book != ScrapBookMenu::Screen::Closed;
		if (open != (_livingRoomBook != ScrapBookMenu::Screen::Closed))
			_livingRoom->Play(open ? 321.0f : 350.0f, open ? 350.0f : 380.0f);
		_livingRoomBook = book;
		_livingRoom->SetBorder(open);
		if (open)
			_livingRoom->SetGlow({});
	}
	const OptionsMenu::Screen screen = _optionsMenu->GetScreen();
	// The options over the room: no entry of the main menu's is picked.
	if (screen != OptionsMenu::Screen::Closed)
		_livingRoom->SetGlow({});
	if (screen == _livingRoomScreen)
		return;
	const bool deeper = static_cast<int>(screen) > static_cast<int>(_livingRoomScreen);
	if (const Trip* t = trip(deeper ? screen : _livingRoomScreen))
	{
		if (deeper)
			_livingRoom->Play(t->from, t->to);
		else
			_livingRoom->Play(t->back, t->end);
	}
	_livingRoomScreen = screen;
}

void Game::drawMainMenuPage(const GameIntent& intent, const Matrix4x4& proj)
{
	const auto& buttons = _mainMenu->GetButtons();
	_menuChoice = std::min(_menuChoice, buttons.size() - 1);
	// One entry at a time, turned with left and right (or a click on an
	// arrow), taken with the confirm key (or a click on it).
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	const auto clicked = [&](const char* name) {
		float x, y, w, h;
		return intent.mouseLeftClick && _mainMenuPage->GetRect(name, x, y, w, h) && intent.mouseX >= x &&
		       intent.mouseX <= x + w && intent.mouseY >= y && intent.mouseY <= y + h;
	};
	if ((left && !_menuLeftHeld) || clicked("MainMenu_LArrow"))
		_menuChoice = (_menuChoice + buttons.size() - 1) % buttons.size();
	if ((right && !_menuRightHeld) || clicked("MainMenu_RArrow"))
		_menuChoice = (_menuChoice + 1) % buttons.size();
	_menuLeftHeld = left;
	_menuRightHeld = right;
	// The PC page's text (it has "QUIT GAME") shows ours; the console one,
	// at the same place, nothing.
	// The game's own words fit its box ("RESUME GAME"); which level it resumes
	// ("- LEVEL 2") does not.
	if (_livingRoom)
		_livingRoom->SetGlow(_menuChoice < _mainMenuGlows.size() ? _mainMenuGlows[_menuChoice] : std::string());
	const std::string& label = buttons[_menuChoice].label;
	_mainMenuPage->SetText("MainMenu_PC", label.substr(0, label.find(" - ")));
	_mainMenuPage->SetText("MainMenu", " ");
	_mainMenuPage->Draw(proj);
	// Not on the frame it opens: the key that left the splash screen would
	// take the first entry at once.
	if (_menuShown && (intent.uiConfirm || clicked("MainMenu_PC")) && buttons[_menuChoice].action)
	{
		_menuShown = false;
		buttons[_menuChoice].action();
		return;
	}
	_menuShown = true;
}

void Game::restorePurchases()
{
	// This level's, as bought in an earlier session.
	_ownedCars = _save.Entries(fmt::format("car:{}", _levelNumber));
	_ownedSkins = _save.Entries(fmt::format("skin:{}", _levelNumber));
	_currentCar = _save.Get(fmt::format("current-car:{}", _levelNumber));
	_wornSkin = _save.Get(fmt::format("worn-skin:{}", _levelNumber));
}

void Game::savePurchases()
{
	// An unattended run's are free, not bought.
	if (_autopilot)
		return;
	for (const auto& car : _ownedCars)
		_save.Add(fmt::format("car:{}", _levelNumber), car);
	for (const auto& skin : _ownedSkins)
		_save.Add(fmt::format("skin:{}", _levelNumber), skin);
	if (!_currentCar.empty())
		_save.Set(fmt::format("current-car:{}", _levelNumber), _currentCar);
	if (!_wornSkin.empty())
		_save.Set(fmt::format("worn-skin:{}", _levelNumber), _wornSkin);
	_save.Save();
}

bool Game::BuyCar(const std::string& car)
{
	if (car.empty() || _ownedCars.count(car))
	{
		if (!car.empty())
		{
			_currentCar = car;
			savePurchases();
		}
		return true;
	}
	const int price = CarPrice(car);
	if (!_autopilot && price > 0 && !SpendCoins(price))
		return false;
	_ownedCars.insert(car);
	_currentCar = car;
	Log::Info("Game: bought '{}' for {} coins{}", car, _autopilot ? 0 : price, _autopilot ? " (autopilot)" : "");
	savePurchases();
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

void Game::AddCoins(int amount)
{
	_coins += amount;
	saveCoins();
}

void Game::saveCoins() const
{
	// Coins an unattended run walks into were not earned either.
	if (_autopilot)
		return;
	auto& save = const_cast<SaveGame&>(_save);
	save.Set("coins", std::to_string(_coins));
	save.Save();
}

std::unique_ptr<Character> Game::makeNpc(const std::string& name) const
{
	// The game's character files are named in six letters at most: Milhouse is
	// milhou_m.p3d, Kearney kearne_m.p3d, Skinner skinne_m.p3d. Looked up by
	// the full name only, every one of them was left out of its mission.
	std::string file = name;
	if (!FileSystem::exists(fmt::format("art/chars/{}_m.p3d", file)) && name.size() > 6)
		file = name.substr(0, 6);
	// Without a model they stay positions for "talkto" only.
	if (!FileSystem::exists(fmt::format("art/chars/{}_m.p3d", file)))
		return nullptr;

	auto npc = std::make_unique<Character>(name);
	npc->LoadModel(file);
	// npd_m.p3d and the like are a skeleton without a body: drawing one
	// crashed level 1 m5 the moment its "npd" joined the scene.
	if (!npc->HasMesh())
	{
		Log::Warn("Game: '{}' has no body to show, not placed", name);
		return nullptr;
	}
	npc->LoadAnimations(file);
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
	// that root, not the origin, at the seat, its height too: without it the
	// driver sat half a metre up, standing out of the car, Marge's hair
	// through its roof. Facing the car's way: the model faces -Z, hence the
	// half turn (as at its door); the car's own way had every driver sitting
	// backwards, over the back seat.
	Vector3 root = Vector3::Zero;
	if (const auto* joint = character.FindJoint("Motion_Root"))
		root = joint->pose.Translation();
	const Quaternion facing = car.GetRotation() * Quaternion(0.0f, 1.0f, 0.0f, 0.0f);
	character.SetRotation(facing);
	character.FollowController(seat - facing * root + Vector3(0.0f, character.CentreHeight(), 0.0f));
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
		// The mission's driver, or else the car script's own (SetDriver).
		std::string name = _scriptEngine->GetVehicleDriver(car);
		if (name.empty() && car->GetDefaultDriver() != "none" && _scriptEngine->IsDrivenByAI(car))
			name = car->GetDefaultDriver();
		if (name.empty())
			continue;
		present.insert(car);
		if (car == _activeVehicle || (car->GetPosition() - _gameCamera->View().GetPosition()).Length() > 120.0f)
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

void Game::saveGags() const
{
	if (_autopilot)
		return;
	auto& save = const_cast<SaveGame&>(_save);
	for (const std::string& id : _gags.GetFoundIds()) save.Add("gag", id);
	save.Save();
}

void Game::playGag(GagManager::Gag& gag)
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
	// Its movie, if it has one, in place of its sound.
	const bool movie = !gag.movie.empty() && PlayStoryMovie(gag.movie, false);
	if (!sounds.empty() && !movie)
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
		const int found = _gags.GetFound(), total = _gags.GetTotal();
		const std::string& word = _textBible.Get("GAGS");
		_messages.card = fmt::format("{} {}/{}", word.empty() ? "Gags" : word, found, total);
		_messages.cardFound = -1;
		_messages.itemsCount = fmt::format("{}/{}", found, total);
		_messages.itemsDone = found == total ? "GAGS_COMPLETE" : "";
		if (found == total)
			_messages.card += " - " + _textBible.Get("GAGS_COMPLETE");
		_messages.cardUntil = _elapsedSeconds + 3.0;
		saveGags();
	}
}

bool Game::useGag()
{
	if (!_character || _inVehicle)
		return false;
	const Vector3 me = _character->GetPosition();
	if (GagManager::Gag* gag = _gags.FindByHand(me, _interior))
	{
		playGag(*gag);
		return true;
	}
	if (const GagManager::MovieGag* gag = _gags.FindMovieGag(me))
	{
		useMovieGag(*gag);
		return true;
	}
	if (const GagManager::CardGag* gag = _gags.FindCardGag(me, _interior))
	{
		useCardGag(*gag);
		return true;
	}
	return false;
}

void Game::useCardGag(const GagManager::CardGag& gag)
{
	if (_save.Entries("ticket").count("itchy") > 0)
		return;
	// Every card of every level: the ticket is the player's.
	if (_save.Entries("card").size() >= 49)
	{
		playConversation(gag.accept);
		if (!_autopilot)
		{
			_save.Add("ticket", "itchy");
			_save.Save();
		}
		Log::Info("Game: the cards traded for the Itchy & Scratchy Movie ticket");
		return;
	}
	// His terms the first time, then a sneer.
	playConversation(_cardTermsHeard ? gag.refusal : gag.terms);
	_cardTermsHeard = true;
}

bool Game::playConversation(const std::string& name)
{
	std::vector<std::pair<int, std::string>> lines;
	for (const auto& sound : _audioManager->FindSounds("\\c_" + name + "_"))
	{
		const std::size_t at = sound.find("\\c_" + name + "_") + 4 + name.size();
		lines.emplace_back(std::atoi(sound.c_str() + at), sound);
	}
	std::sort(lines.begin(), lines.end());
	for (const auto& [n, sound] : lines) _audioManager->QueueDialogue(sound, AudioManager::DialoguePriority::Gameplay);
	return !lines.empty();
}

void Game::useMovieGag(const GagManager::MovieGag& gag)
{
	// The ticket is Comic Book Guy's, for a whole set of cards (useCardGag).
	if (_save.Entries("ticket").count("itchy") > 0 && PlayStoryMovie(gag.movie, false))
		return;
	// One of the refusals (c_aztec1 to c_aztec4), its lines in turn.
	std::map<std::string, std::vector<std::pair<int, std::string>>> refusals;
	for (const auto& sound : _audioManager->FindSounds("\\c_" + gag.refusal))
	{
		const std::size_t at = sound.find("\\c_" + gag.refusal) + 3;
		const std::size_t line = sound.find('_', at);
		if (line == std::string::npos)
			continue;
		refusals[sound.substr(at, line - at)].emplace_back(std::atoi(sound.c_str() + line + 1), sound);
	}
	if (refusals.empty())
		return;
	auto picked = std::next(refusals.begin(), rand() % static_cast<int>(refusals.size()));
	std::sort(picked->second.begin(), picked->second.end());
	for (const auto& [n, sound] : picked->second)
		_audioManager->QueueDialogue(sound, AudioManager::DialoguePriority::Gameplay);
	Log::Info("Game: no ticket for '{}', refused ({})", gag.movie, picked->first);
}

void Game::updateCarSmoke(double dt)
{
	// The car the player has, and only while it is one of the cars there are.
	const Vehicle* car = nullptr;
	if (_activeVehicle && _scriptEngine->IsMissionActive())
		for (const auto& vehicle : _scriptEngine->GetMissionVehicles())
			if (vehicle.get() == _activeVehicle)
				car = _activeVehicle;
	if (car != _carSmokeVehicle)
	{
		_carSmoke.reset();
		_carSmokeVehicle = car;
	}
	if (!car)
		return;
	const bool smoking = _health < kCarSmokesBelow;
	if (smoking && !_carSmoke && _level)
		if (const ParticleFactory* damage = _level->GetHitParticleFactory(kCarDamageEffect))
			_carSmoke = std::make_unique<ParticleSystem>(*damage);
	if (_carSmoke)
		_carSmoke->Update(dt, Matrix4x4::MakeTranslate(car->GetHoodPosition()), -1.0f, smoking);
}

void Game::updateWheelSprays(double dt)
{
	// The cars there are now; those gone take their sprays with them.
	std::vector<const Vehicle*> cars;
	if (_scriptEngine->IsMissionActive())
		for (const auto& vehicle : _scriptEngine->GetMissionVehicles())
			cars.push_back(vehicle.get());
	for (auto it = _wheelSprays.begin(); it != _wheelSprays.end();)
		it = std::find(cars.begin(), cars.end(), it->first) == cars.end() ? _wheelSprays.erase(it) : std::next(it);

	const ParticleFactory* grass = _level ? _level->GetHitParticleFactory(kWheelSprayGrass) : nullptr;
	const ParticleFactory* dirt = _level ? _level->GetHitParticleFactory(kWheelSprayDirt) : nullptr;
	// How hard the player's car skids, and whether any of it on the road: its
	// tyres squeal there, scrape on loose ground.
	float playerSkid = 0.0f;
	bool playerSkidsOnRoad = false;
	for (const Vehicle* vehicle : cars)
	{
		const bool moving = std::fabs(vehicle->GetSpeedKmh()) > kWheelSprayKmh;
		// Every wheel skidding marks the ground, with the ground's own mark.
		for (int wheel = 0; wheel < 4 && _skidMarks; ++wheel)
		{
			const bool player = vehicle == _activeVehicle && _inVehicle;
			const uint64_t track = reinterpret_cast<uintptr_t>(vehicle) * 4 + wheel;
			Vector3 contact;
			const float slide = vehicle->GetWheelSlide(wheel);
			const char* mark = nullptr;
			switch (vehicle->GetWheelSurface(wheel))
			{
			case Surface::Grass: mark = "grass_skidmark_shader"; break;
			case Surface::Dirt:
			case Surface::Sand:
			case Surface::Gravel: mark = "dirt_skidmark_shader"; break;
			case Surface::Water: break;
			default: mark = "skid_m"; break;
			}
			// The rear wheels spinning as it sets off flat out, too.
			const float peel = wheel >= 2 ? vehicle->GetPeelOut() * kPeelOutMaxTrim : 0.0f;
			const float strength = std::max(
			    {vehicle->IsBrakingHard() ? 0.8f : 0.0f, std::min(1.0f, slide / (kSkidSlide * 2.0f)), peel});
			const bool skids = slide > kSkidSlide || vehicle->IsBrakingHard() || peel > kPeelOutMin;
			if (mark && skids && vehicle->GetWheelContact(wheel, contact))
			{
				_skidMarks->Lay(track, contact, vehicle->GetWheelNormal(wheel), vehicle->GetWheelAxle(wheel), kSkidWidth,
				                mark, strength);
				if (player)
				{
					playerSkid = std::max(playerSkid, strength);
					playerSkidsOnRoad = playerSkidsOnRoad || std::string(mark) == "skid_m";
				}
			}
			else
				_skidMarks->Lift(track);
		}
		for (int k = 0; k < 2; ++k)
		{
			// The rear wheels (2, 3).
			Vector3 contact;
			const bool touching = vehicle->GetWheelContact(2 + k, contact);
			const Surface surface = vehicle->GetWheelSurface(2 + k);
			const bool onGrass = moving && touching && surface == Surface::Grass;
			const bool onDirt = moving && touching &&
			                    (surface == Surface::Dirt || surface == Surface::Sand || surface == Surface::Gravel);
			auto& spray = _wheelSprays[vehicle][k];
			if (onGrass && !spray.grass && grass)
				spray.grass = std::make_unique<ParticleSystem>(*grass);
			if (onDirt && !spray.dirt && dirt)
				spray.dirt = std::make_unique<ParticleSystem>(*dirt);
			// The emitters throw straight up their own Y: tipped back, off the
			// tyre and behind the car.
			const Matrix4x4 placement =
			    Matrix4x4::MakeTranslate(contact) *
			    Matrix4x4(vehicle->GetRotation() * Quaternion(Vector3(1.0f, 0.0f, 0.0f), kWheelSprayTilt));
			if (spray.grass)
				spray.grass->Update(dt, placement, -1.0f, onGrass);
			if (spray.dirt)
				spray.dirt->Update(dt, placement, -1.0f, onDirt);
		}
	}
	if (_audioManager)
	{
		// The game's sound settings name the clips (SetSkidRoadClipName "skid",
		// SetSkidDirtClipName "dirt_skid").
		static AudioManager::ScriptSound road, dirt;
		if (road.file.empty())
			for (const char* script : {"carsound", "car_tune", "global"})
			{
				if (road.file.empty())
					road = _audioManager->FindScriptSound(script, "skid");
				if (dirt.file.empty())
					dirt = _audioManager->FindScriptSound(script, "dirt_skid");
			}
		const auto& clip = !playerSkidsOnRoad && !dirt.file.empty() ? dirt : road;
		_audioManager->SetLoop("skid", playerSkid > 0.0f ? clip.file : "", clip.trim * (0.4f + 0.6f * playerSkid));
	}
}

void Game::placeEvent(uint32_t event)
{
	// The place's reverb, as the game's reverb controller has it (its event
	// handler, 0x4ce140): the events it listens to are the places' (7 to 46,
	// 50 to 62, 67 to 76); those of a reverb set it, the others take it off.
	const bool place = (event >= 7 && event <= 46) || (event >= 50 && event <= 62) || (event >= 67 && event <= 76);
	if (!place)
		return;
	static const std::map<uint32_t, const char*> kReverbs = {
	    {20, "stadiumtunnel"}, {34, "krustylu"},     {37, "stonecuttertunnel"}, {38, "stonecutterhall"},
	    {39, "sewers"},        {40, "burns_tunnel"}, {41, "pproom1"},           {42, "pproom2"},
	    {43, "pproom3"},       {44, "PP_tunnel_01"}, {45, "PP_tunnel_02"},      {46, "mansion_interior"},
	};
	const auto reverb = kReverbs.find(event);
	_audioManager->SetReverb(reverb != kReverbs.end() ? reverb->second : "");
}

void Game::updateGags()
{
	if (!_character)
		return;
	const Vector3 me = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
	_gags.Update(me, _interior, _elapsedSeconds, [this](GagManager::Gag& gag) { playGag(gag); });
	// The gags not yet found glint now and then.
	if (_sparks && _elapsedSeconds - _sparkledAt > 0.6)
	{
		_sparkledAt = _elapsedSeconds;
		for (const Vector3& at : _gags.GetSparkles(me, _interior, 40.0f, _save.Entries("ticket").count("itchy") > 0))
			_sparks->Burst(2, at + Vector3(0.0f, 0.8f, 0.0f));
	}
}

std::vector<Game::Reward> Game::rewards(const std::string& kind) const
{
	// rewards.mfk's BindReward("<name>", "<file>", "car"|"skin", "<how>",
	// <level>[, <price>, "<shop>"]) and SetCarAttributes("<car>", speed,
	// acceleration, toughness, stability). A level's own car and skin are the
	// player's from the start; the bonus mission's car with the bonus mission
	// won; the street races' with all three won; those for sale bought.
	std::vector<Reward> found;
	std::map<std::string, std::array<float, 4>> stars;
	std::ifstream file("scripts/rewards.mfk");
	for (std::string line; std::getline(file, line);)
	{
		const std::size_t comment = line.find("//");
		const std::size_t bind = line.find("BindReward("), attributes = line.find("SetCarAttributes(");
		const std::size_t at = bind != std::string::npos ? bind : attributes;
		if (at == std::string::npos || (comment != std::string::npos && comment < at))
			continue;
		std::vector<std::string> quoted;
		for (std::size_t open = line.find('"', at); open != std::string::npos; open = line.find('"', open + 1))
		{
			const std::size_t close = line.find('"', open + 1);
			if (close == std::string::npos)
				break;
			quoted.push_back(line.substr(open + 1, close - open - 1));
			open = close;
		}
		if (attributes != std::string::npos && quoted.size() == 1)
		{
			std::array<float, 4> values {};
			std::istringstream numbers(line.substr(line.find(',', at) + 1));
			for (float& value : values)
			{
				numbers >> value;
				numbers.ignore(8, ',');
			}
			stars[quoted[0]] = values;
			continue;
		}
		if (quoted.size() < 4 || quoted[2] != kind)
			continue;
		const std::size_t afterHow = line.find(quoted[3]) + quoted[3].size() + 1;
		Reward reward;
		reward.kind = kind;
		reward.car = quoted[0];
		reward.how = quoted[3];
		// The numbers after the way it is had: its level, then its price.
		std::istringstream numbers(line.substr(line.find(',', afterHow) + 1));
		numbers >> reward.level;
		numbers.ignore(8, ',');
		numbers >> reward.price;
		if (quoted.size() > 4)
			reward.shop = quoted[4];
		if (kind == "skin")
			reward.unlocked = reward.how == "defaultskin" ||
			                  _save.Entries(fmt::format("skin:{}", reward.level)).count(reward.car) > 0 ||
			                  (reward.level == _levelNumber && _ownedSkins.count(reward.car) > 0);
		else
		{
			int won = 0;
			for (const std::string& entry : _save.Entries("won"))
				if (entry.compare(0, 3, fmt::format("L{}:", reward.level)) == 0 &&
				    entry.compare(3, 2, reward.how == "bonusmission" ? "bm" : "sr") == 0)
					++won;
			reward.unlocked = reward.how == "defaultcar" || (reward.how == "bonusmission" && won > 0) ||
			                  (reward.how == "streetrace" && won >= 3) ||
			                  _save.Entries(fmt::format("car:{}", reward.level)).count(reward.car) > 0 ||
			                  (reward.level == _levelNumber && _ownedCars.count(reward.car) > 0);
		}
		found.push_back(reward);
	}
	for (Reward& reward : found)
		if (const auto attributes = stars.find(reward.car); attributes != stars.end())
			std::copy(attributes->second.begin(), attributes->second.end(), reward.stars);
	return found;
}

bool Game::openShop()
{
	if (!_character || _inVehicle || !_shop || !_shop->IsLoaded() || _autopilot)
		return false;
	const Vector3 me = _character->GetPosition();
	// A car seller within reach: their shop's cars of this level.
	_shopSeller = nullptr;
	for (const CarSeller& seller : _carSellers)
		if ((seller.position - me).Length() < 2.5f)
			_shopSeller = &seller;
	bool wardrobe = false;
	if (!_shopSeller)
		for (const auto& trigger : _level->GetTriggers())
			if (trigger.action == "PurchaseSkin" && (trigger.Contains(me) || (trigger.position - me).Length() < 2.0f))
				wardrobe = true;
	if (!_shopSeller && !wardrobe)
		return false;
	std::vector<ShopMenu::Item> items;
	for (const Reward& reward : rewards(_shopSeller ? "car" : "skin"))
	{
		if (reward.level != _levelNumber || reward.how != "forsale" ||
		    (_shopSeller ? reward.shop != _shopSeller->shop : reward.shop != "interior"))
			continue;
		ShopMenu::Item item {reward.car, reward.price, reward.unlocked, _shopSeller != nullptr};
		std::copy(std::begin(reward.stars), std::end(reward.stars), item.stars);
		items.push_back(item);
	}
	if (items.empty())
		return false;
	Log::Info("Game: shop '{}', {} on sale", _shopSeller ? _shopSeller->shop : "interior", items.size());
	_shop->Open(std::move(items));
	_states.Fire(GameEvent::OpenShop);
	return true;
}

void Game::callCar(const std::string& car)
{
	// The car chosen, the player's from now on, brought round; repaired.
	_states.Fire(GameEvent::Play);
	_currentCar = car;
	savePurchases();
	Vehicle& vehicle = _scriptEngine->SummonPlayerCar(car, _phoneBoothRoad);
	_health = 100.0f;
	PutPlayerInVehicle(&vehicle);
	_audioManager->PlayRaw(SoundGenerator::Beep(1200, 0.2f), 22050, 1, 16);
	Log::Info("Game: phone booth sends '{}' to ({:.0f}, {:.0f}, {:.0f}), repaired", car, _phoneBoothRoad.X,
	          _phoneBoothRoad.Y, _phoneBoothRoad.Z);
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

		const int node = _pathGraph->FindNearestRoadNode(trigger.position);
		const Vector3 road = node >= 0 ? _pathGraph->GetNodes()[node].position : trigger.position;
		// The game's car select, every car it rewards, from the one driven.
		if (_phoneBooth && _phoneBooth->IsLoaded() && !_autopilot)
		{
			std::vector<PhoneBoothMenu::Car> cars;
			for (const Reward& reward : rewards("car"))
			{
				PhoneBoothMenu::Car car {reward.car, reward.level, reward.unlocked};
				std::copy(std::begin(reward.stars), std::end(reward.stars), car.stars);
				cars.push_back(car);
			}
			_phoneBoothRoad = road;
			_phoneBooth->Open(std::move(cars), GetPlayerCarModel());
			_states.Fire(GameEvent::OpenPhoneBooth);
			Log::Info("Game: phone booth '{}'", trigger.locator);
			return true;
		}
		// Unattended: the car the mission gave the player.
		Vehicle* car = _scriptEngine->GetPlayerCar();
		if (car == nullptr)
		{
			Log::Info("Game: phone booth '{}', but no car to send", trigger.locator);
			return true;
		}
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
		// `pos` is where the capsule's middle goes (a metre over the ground,
		// the way the scripts' and the autopilot's moves give it).
		_character->SetCentre(pos);
		Log::Info("Game: player teleported to ({:.1f}, {:.1f}, {:.1f})", pos.X, pos.Y, pos.Z);
	}
}

void Game::PlacePlayerOnFoot(const Vector3& pos, bool faces, const Vector3& lookAt)
{
	_inVehicle = false;
	_activeVehicle = nullptr;
	_boarding = {};
	SetPlayerPosition(pos);
	// A character model faces -Z, hence the half turn.
	if (faces && _character)
		_character->SetRotation(
		    Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(lookAt.X - pos.X, lookAt.Z - pos.Z) + 3.14159265f, 0.0f)));
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

void Game::Run()
{
	// measure our delta time
	uint64_t now = SDL_GetPerformanceCounter();
	uint64_t last = 0;
	double deltaTime = 0.0;

	FpsTimer timer;

	SpriteBatch sprites(1024);

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
		if (_launch.quitAfterSeconds > 0.0 && _elapsedSeconds >= _launch.quitAfterSeconds)
		{
			Log::Info("Game: --quit-after {:.0f}s reached, closing", _launch.quitAfterSeconds);
			running = false;
		}

		// The bonus game runs on its own: its menu, its track, its cars.
		if (_states.Is(GameState::MiniGame))
		{
			LockMouse(false);
			miniGameFrame(deltaTime, intent);
			continue;
		}

		updateOnFoot(deltaTime, intent);
		updateCheckpoints();
		updateDriving(intent);
		updateCamera(deltaTime);

		// Pause has to stop the world, not just hide it: this used to keep running
		// every frame while paused, so physics and scripts ticked behind the overlay.
		const bool simulate = !_states.Is(GameState::Paused) && !_states.Is(GameState::Movie) &&
		                      !_states.Is(GameState::PhoneBooth) && !_states.Is(GameState::Shop) &&
		                      !_states.Is(GameState::LevelEnd);

		if (simulate)
		{
			_worldPhysics->Update(static_cast<float>(deltaTime));
			if (_level)
				_level->UpdateCollisions();
		}

		updateAudio(deltaTime);
		updateCarImpacts(deltaTime, simulate);
		_gameCamera->ApplyShake(deltaTime);
		updateMissionResult(deltaTime);

		// The PC game has no title screen to press a key on: its movies over,
		// it is at its main menu. Without the menu's page, the old "press
		// enter" over the level.
		if (_states.Is(GameState::Splash) &&
		    (intent.uiConfirm || intent.pauseToggle || (_mainMenuPage && !_mainMenu->GetButtons().empty())))
			_states.Fire(GameEvent::ToMainMenu);

		updateFallOff();
		drawGuides();
		updateWorld(deltaTime, simulate);
		updateGameplay(deltaTime, simulate);

		drawDebugWindows();
		if (!handleIntents(intent))
			running = false;
		drawDebugOverlays();

		updateCharacters(deltaTime);

		ImGui::Render();

		const int viewportWidth = static_cast<int>(ImGui::GetIO().DisplaySize.x);
		const int viewportHeight = static_cast<int>(ImGui::GetIO().DisplaySize.y);

		renderScene(deltaTime, viewportWidth, viewportHeight);

		Matrix4x4 proj = Matrix4x4::MakeOrtho(0.0f, viewportWidth, viewportHeight, 0.0f);

		if (drawHud(sprites, intent, proj, deltaTime, static_cast<int>(timer.GetFps()), viewportWidth, viewportHeight))
			continue;

		drawScreens(sprites, intent, proj, deltaTime, viewportWidth, viewportHeight);

		if (!_launch.clean)
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		_window->Swap();
	}
}

void Game::updateOnFoot(double deltaTime, GameIntent& intent)
{
	// Third-person camera: capture the mouse during on-foot gameplay so its
	// motion orbits the camera around the character. The camera no longer
	// follows the character's facing — only the mouse turns it.
	// The game settings' mouse: whether it looks round, how much (a half
	// is 0.005 a pixel), either way inverted.
	const OptionsMenu::Mouse mouse = _optionsMenu ? _optionsMenu->GetMouse() : OptionsMenu::Mouse();
	const bool onFoot = (_states.Is(GameState::InGame)) && _character && !_inVehicle;
	LockMouse(onFoot && mouse.look);
	if (onFoot && mouse.look && _gameInput->IsMouseLocked())
	{
		const float across = 0.0005f + 0.009f * mouse.sensitivityX, upDown = 0.0005f + 0.009f * mouse.sensitivityY;
		// Mouse up looks up.
		_gameCamera->Orbit(intent.mouseDX * across * (mouse.invertX ? -1.0f : 1.0f),
		                   -intent.mouseDY * upDown * (mouse.invertY ? -1.0f : 1.0f));
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
		if (intent.interact &&
		    (usePhoneBooth() || openShop() || ringDoorbell() || useSwitch() || useGag() || startBonusMission()))
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
			// Something to hit within reach (a crate, a wasp come low): the
			// character's strike, not a jump. It counts as an attack for the
			// wasps, as a jump does.
			const Vector3 me = _character->GetPosition();
			const bool target = _level->HasBreakableNear(me, 2.2f) || _level->HasKickablePropNear(me, 1.8f) ||
			                    (_waspManager && _waspManager->AnyNear(me, 2.5f));
			if (!_inVehicle && !_boarding.car && ctrl.onGround() && target && playPlayerAnimation("break", false))
			{
				_moves.lastJumpAt = _elapsedSeconds;
				if (_level->BreakObjectsNear(me, 2.2f) > 0)
					AddShake(0.2f);
				// A mailbox, a bin kicked over.
				_level->KickPropsNear(me, 1.8f);
			}
			else if (!_inVehicle && !_boarding.car && ctrl.canJump())
			{
				ctrl.jump(btVector3(0, 0, 0));
				playPlayerAnimation("jump_idle_take_off", false);
				_moves.lastJumpAt = _elapsedSeconds;
				_moves.kickedThisJump = false;
				_moves.doubleJumped = false;
			}
			// In the air, again: up once more, as high again ("quand tu es
			// dans les airs, réappuie sur la touche A pour sauter plus loin",
			// the tutorial says) -- the wasps on a roof's edge in reach.
			else if (!_inVehicle && !_boarding.car && !ctrl.onGround() && !_moves.doubleJumped && !_moves.kickedThisJump)
			{
				ctrl.jumpAgain();
				playPlayerAnimation("jump_idle_take_off", false);
				_moves.lastJumpAt = _elapsedSeconds;
				_moves.doubleJumped = true;
			}
			// Then a kick: once a jump. It hits a wasp in reach (the attack
			// lasts a second more) and breaks what it lands on.
			else if (!_inVehicle && !_boarding.car && !ctrl.onGround() && !_moves.kickedThisJump &&
			         playPlayerAnimation("jump_kick", false))
			{
				_moves.kickedThisJump = true;
				_moves.lastJumpAt = _elapsedSeconds;
				if (_level->BreakObjectsNear(_character->GetPosition(), 2.0f, true, "STOMP") > 0)
					AddShake(0.2f);
				_level->KickPropsNear(_character->GetPosition(), 1.8f);
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
			Vector3 worldMove = Quaternion::MakeFromEuler(Vector3(0.0f, _gameCamera->GetYaw(), 0.0f)) * charMove;
			// A dash with the boost held: faster, and in Homer's dash.
			_moves.dashing = intent.boostHeld;
			ctrl.setWalkDirection(BulletCast<btVector3>(worldMove * (_moves.dashing ? 8.0f : 5.0f)));

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
		_moves.dashKicksUpDust = moving && _moves.dashing && ctrl.onGround();
	}
}

void Game::updateCheckpoints()
{
	// Checkpoints are reached on foot as well as at the wheel: a mission's
	// "goto" is often a walk to a door.
	if (_scriptEngine->IsMissionActive() && _character)
	{
		const Vector3 playerPos = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
		const auto& checkpoints = _scriptEngine->GetCheckpoints();
		const int current = _scriptEngine->GetCurrentCheckpoint();
		if (current >= 0 && current < static_cast<int>(checkpoints.size()) &&
		    (playerPos - checkpoints[current]).Length() < 5.0f)
			_scriptEngine->AdvanceCheckpoint();
	}
}

void Game::updateDriving(const GameIntent& intent)
{
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
			_gameCamera->AddShake(1.0f);
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
}

void Game::updateCamera(double deltaTime)
{
	_gameCamera->SetPhysics(_worldPhysics.get());
	if (_inVehicle && _activeVehicle && !_gameInput->IsMouseLocked())
		_gameCamera->FollowVehicle(*_activeVehicle, deltaTime);
	else if (_character)
		_gameCamera->OrbitCharacter(_character->GetPosition(), deltaTime);
	// The script's cameras over the player's: a mission's camera move as it
	// starts, then the conversation's shots, line by line.
	if (_states.Is(GameState::InGame))
		scriptedCamera(deltaTime);
}

void Game::updateAudio(double deltaTime)
{
	// The listener is the camera; it was never placed, and every sound given
	// a position was heard from the world's origin, if at all. OpenAL takes
	// right as forward x up; in this world +X is on the right looking down
	// +Z, which takes "up" as -Y to come out the same way round.
	{
		const Vector3 forward = _gameCamera->View().GetOrientation().Inverse() * Vector3::Forward;
		_audioManager->SetListenerPosition(_gameCamera->View().GetPosition(), forward, Vector3(0.0f, -1.0f, 0.0f));
	}
	_audioManager->Update();
	// The place's background: the level's outdoors, or a small room indoors.
	// The game picks it by zone with a script (ambience.rms) not read here.
	{
		static const char* const kOutdoors[] = {"suburb_day_loop",   "city_busy_loop",  "squidport_amb_loop_02",
		                                        "suburb_night_loop", "city_night_loop", "squidport_night_loop",
		                                        "halloween_amb_loop"};
		const int level = std::clamp(_levelNumber, 1, 7);
		std::string indoors = _interior;
		std::transform(indoors.begin(), indoors.end(), indoors.begin(), [](unsigned char c) { return std::tolower(c); });
		_audioManager->PlayAmbience(_interior.empty()                          ? kOutdoors[level - 1]
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
}

void Game::updateCarImpacts(double deltaTime, bool simulate)
{
	if (simulate)
	{
		// The car smashes what it runs into: crates, vending machines, gas
		// pumps play their breaking and stay broken. Those with a body break
		// as the car hits it (Level::updateBreakingBodies); the others, as
		// it comes by.
		if (_inVehicle && _activeVehicle && std::abs(_activeVehicle->GetSpeedKmh()) > 15.0f)
			if (const int broke = _level->BreakObjectsNear(_activeVehicle->GetPosition(), 2.5f, false, "VEHICLE_HIT");
			    broke > 0)
			{
				AddShake(0.3f);
				_audioManager->PlayRaw(SoundGenerator::Chirp(220.0f, 80.0f, 0.2f, 44100), 44100, 1, 16);
				Log::Info("Game: {} object(s) smashed", broke);
			}
		updateCarDamage();
		updateCarDamageLook(deltaTime);
		if (_states.Is(GameState::InGame))
			updateHitAndRun(deltaTime);
	}
}

void Game::updateMissionResult(double deltaTime)
{
	if (_states.Is(GameState::MissionComplete) || _states.Is(GameState::MissionFailed))
	{
		// Won on foot, the player celebrates: the big one for the level's last
		// story mission.
		if (_states.GetResultTime() == 0.0 && _states.Is(GameState::MissionComplete) && !_inVehicle)
		{
			const bool levelWon = _storyIndex >= 0 && _storyIndex + 1 == static_cast<int>(_story.size());
			if (!levelWon || !playPlayerAnimation("victory_large", false))
				playPlayerAnimation("victory_small", false);
		}
		const bool failed = _states.Is(GameState::MissionFailed);
		if (_states.UpdateResult(deltaTime) && _states.Fire(GameEvent::ResultOver))
		{
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
					const auto nextLevel = [this]()
					{
						switchLevel(_levelNumber + 1);
						if (!_story.empty())
							startStoryMission(0);
						_states.Fire(GameEvent::Play);
					};
					// The level's end: its counts, all its story won, then on.
					if (_pauseSunday && _pauseSunday->IsLoaded() && !_autopilot)
					{
						int missions = 0;
						for (const std::string& entry : _story)
							missions += entry.size() > 7 && entry.compare(entry.size() - 7, 7, "sdi.mfk") != 0;
						_pauseSunday->OpenLevelEnd(_levelNumber, levelStats(missions, missions), nextLevel);
						_states.Fire(GameEvent::LevelOver);
					}
					else
						nextLevel();
				}
				else if (levelWon)
				{
					// The last level's last mission: the story is over. It used to
					// replay that mission, for ever.
					Log::Info("Game: the story is over, level {} won", _levelNumber);
					_scriptEngine->CleanupMission();
					// The credits roll, then the main menu.
					_states.Fire(_creditsPage ? GameEvent::ShowCredits : GameEvent::ToMainMenu);
					_creditsTime = 0.0;
					if (_creditsPage)
						_audioManager->PlayMusicEvent("Credits");
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
					if (_collectibleManager)
						_collectibleManager->SpawnOnPath();
					runMissionScript();
				}
			}
		}
	}
}

void Game::updateFallOff()
{
	if (_character && _character->GetPosition().Y < _fallOffY)
	{
		Log::Warn("Game: player fell off the map, respawning...");
		_health -= 20.0f;
		if (_health <= 0.0f)
			_health = 100.0f;
		SetPlayerPosition(_safe.onFoot);
	}

	if (_activeVehicle && _inVehicle && _activeVehicle->GetPosition().Y < _fallOffY && _safe.car == _activeVehicle)
	{
		const int node = _pathGraph ? _pathGraph->FindNearestRoadNode(_safe.carPos) : -1;
		const Vector3 road = node >= 0 ? _pathGraph->GetNodes()[node].position : _safe.carPos;
		Log::Warn("Game: vehicle fell off the map, put back on the road at ({:.1f}, {:.1f}, {:.1f})", road.X, road.Y, road.Z);
		_activeVehicle->Teleport(road, Quaternion::MakeFromEuler(Vector3(0.0f, _safe.carYaw, 0.0f)));
		// Known safe again only once it is back on its wheels.
		_safe.car = nullptr;
	}
	else if (_activeVehicle && _inVehicle && !_activeVehicle->IsAirborne() && _activeVehicle->GetPosition().Y > _fallOffY)
	{
		_safe.car = _activeVehicle;
		_safe.carPos = _activeVehicle->GetPosition();
		_safe.carYaw = _activeVehicle->GetRotation().Yaw();
	}

	if (_activeVehicle && _inVehicle && _activeVehicle->GetPosition().Y < _fallOffY)
	{
		Log::Warn("Game: vehicle fell off the map, exiting...");
		_inVehicle = false;
		_activeVehicle = nullptr;
		SetPlayerPosition(_safe.onFoot);
	}

	if (_character && _character->GetCharacterController().onGround())
		_safe.onFoot = _character->GetPosition();
}

void Game::drawGuides()
{
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
			_lineRenderer->DrawBox(_character->GetPosition(), _character->GetRotation(), Vector3(-0.3f, 0.0f, -0.2f),
			                       Vector3(0.3f, 1.8f, 0.3f), Vector4(0.2f, 1.0f, 0.2f, 1.0f));
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
			Vector4 cpCol = i == currentCp  ? Vector4(1.0f, 1.0f, 0.0f, 1.0f)
			                : i < currentCp ? Vector4(0.0f, 1.0f, 0.0f, 1.0f)
			                                : Vector4(1.0f, 0.0f, 0.0f, 1.0f);
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
				_lineRenderer->DrawLine(from + (to - from) * t1, from + (to - from) * t2, Vector4(1.0f, 1.0f, 1.0f, 0.3f));
			}
		}
	}
}

void Game::updateWorld(double deltaTime, bool simulate)
{
	_level->Update(deltaTime);
	// A mailbox, a lamp post knocked over: its crash, and the sparks off a
	// metal one.
	// What a broken crate, a vending machine gives: coins, spilled about it.
	for (const auto& drop : _level->TakeCoinDrops())
		if (_collectibleManager)
			_collectibleManager->DropCoins(drop.position, drop.coins);
	for (const auto& [sound, effect, where] : _level->TakeKnockedProps())
	{
		// The stars off a gravestone have no sprite of their own in the
		// effects files: a few of the sparks stand in for them.
		if (effect == "eSpark" && _sparks)
			_sparks->Burst(20, where);
		else if (effect == "eStars" && _sparks)
			_sparks->Burst(8, where);
		const auto clip = _audioManager->FindScriptSound("collide", sound);
		const std::string& file = clip.file;
		Log::Debug("Game: a prop knocked over at ({:.1f}, {:.1f}, {:.1f}), '{}' ({}), '{}'", where.X, where.Y, where.Z, sound,
		           file.empty() ? "no sound" : file, effect);
		if (!file.empty())
			_audioManager->PlayAudio(file, where, clip.trim, clip.pitch);
	}
	// A stage that asks for no traffic gets empty streets: neither moved,
	// drawn nor run into.
	const bool traffic = !_scriptEngine->IsTrafficSuppressed();
	_trafficManager->SetPresent(traffic);
	// As many cars as the script allows (SetMaxTraffic), and the district's
	// people on the pavements (UsePedGroup).
	_trafficManager->SetMaxCars(_scriptEngine->GetMaxTraffic());
	if (_pedestrianManager)
		_pedestrianManager->UseGroup(_scriptEngine->GetPedGroup());
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
		for (const auto& vehicle : _scriptEngine->GetMissionVehicles()) obstacles.push_back(vehicle->GetPosition());
		if (_hitAndRun)
			for (const Vehicle* cop : _hitAndRun->GetCops()) obstacles.push_back(cop->GetPosition());
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
}

void Game::updateGameplay(double deltaTime, bool simulate)
{
	// Re-enabled (REGR-001): db9c8db commented this out to isolate a crash on "press
	// up"; 9860a4f fixed that same day (uninitialised Character::_rotation feeding the
	// follow camera, plus stepUp/stepDown/onGround bugs) but the block was never restored.
	if (_character != nullptr && _states.Is(GameState::InGame))
	{
		// Triggers fire on the transition, once, from wherever the player
		// actually is -- the car, when driving. This used to log "entered
		// trigger zone" every two seconds for as long as the player stood in
		// one, without saying which.
		const Vector3 playerPos = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
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
			if (event.entered && event.trigger->type == Level::LocatorType::Event)
				placeEvent(event.trigger->event);
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
				_messages.card = _textBible.Get(fmt::format("CARD_TITLE_{:02}", index));
				_messages.cardFound = index;
				_messages.itemsCount.clear();
				_messages.itemsDone =
				    _collectibleManager->GetCardsFound() == _collectibleManager->GetCardsTotal() ? "CARD_LEVEL_COMPLETE" : "";
				if (_collectibleManager->GetCardsFound() == _collectibleManager->GetCardsTotal())
				{
					const std::string& all = _textBible.Get("CARD_LEVEL_COMPLETE");
					_messages.card += _messages.card.empty() ? all : " - " + all;
				}
				_messages.cardUntil = _elapsedSeconds + 4.0;
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
		updateTutorial();
		updateWorkedObjects();
		if (_ufo)
		{
			_ufo->Update(deltaTime);
			Vector3 blast;
			bool lastBarrel = false;
			while (_ufo->TakeBlast(blast, lastBarrel)) bossBlast(blast, lastBarrel);
		}
		// The wasp cameras: a jump at one, or a car driven into it,
		// destroys it, for coins.
		if (_waspManager && _character && _interior.empty())
		{
			const bool driving = _inVehicle && _activeVehicle;
			const Vector3 at = driving ? _activeVehicle->GetPosition() : _character->GetPosition();
			// A jump, not merely being off the ground: a teleport sets the
			// player down from a little above, and destroyed a wasp nearby.
			const bool attacking =
			    driving ? std::abs(_activeVehicle->GetSpeedKmh()) > 10.0f : _elapsedSeconds - _moves.lastJumpAt < 1.0;
			const auto wasps = _waspManager->Update(deltaTime, at, attacking, driving ? 3.5f : 2.5f);
			for (const auto& [sound, where] : wasps.sounds) _audioManager->PlayAudio(sound, where);
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
				_messages.card = fmt::format("Wasp cameras {}/{}", _waspManager->GetDestroyed(), _waspManager->GetTotal());
				_messages.cardFound = -1;
				_messages.itemsCount = fmt::format("{}/{}", _waspManager->GetDestroyed(), _waspManager->GetTotal());
				_messages.itemsDone = _waspManager->GetDestroyed() == _waspManager->GetTotal() ? "WASPS_COMPLETE" : "";
				_messages.cardUntil = _elapsedSeconds + 3.0;
			}
		}
		if (_collectibleManager)
		{
			const Vector3 collector = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition();
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
}

void Game::drawDebugWindows()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(*_window));
	ImGui::NewFrame();

	imguiMenuBar();

	// if (_character != nullptr)
	//	guiModelMenu(*_character);

	// ImGui::ShowDemoWindow();

	if (_debugWindows.resourceManager)
		_resourceManager->ImGuiDebugWindow(&_debugWindows.resourceManager);

	if (_debugWindows.level)
		_level->ImGuiDebugWindow(&_debugWindows.level);

	if (_debugWindows.audio)
		_audioManager->DebugGUI(&_debugWindows.audio);

	debugAboutMenu();
	drawControlsWindow();
}

bool Game::handleIntents(const GameIntent& intent)
{
	bool keepRunning = true;
	// Escape in the options goes back a page, not out of the pause; nor
	// in the pause between missions, which has pages of its own.
	if (intent.pauseToggle && !(_optionsMenu && _optionsMenu->IsOpen()) && !(_pauseSunday && _pauseSunday->IsOpen()))
	{
		if (_states.Is(GameState::InGame))
		{
			_states.Fire(GameEvent::Pause);
			// Driving about between missions (the story at a drive, no
			// bonus mission on): the pause of the Sunday drive.
			const bool drive = _bonusReturn < 0 && _storyIndex >= 0 && _storyIndex < static_cast<int>(_story.size()) &&
			                   _story[_storyIndex].size() > 7 &&
			                   _story[_storyIndex].compare(_story[_storyIndex].size() - 7, 7, "sdi.mfk") == 0;
			if (drive)
				openSundayPause();
		}
		else if (_states.Is(GameState::Paused))
			_states.Fire(GameEvent::Play);
	}

	if (intent.resetBest && _states.Is(GameState::InGame))
	{
		_scriptEngine->ResetBestTime();
		Log::Info("Game: best time reset!");
	}

	if (_states.Is(GameState::Paused) && intent.quit)
	{
		keepRunning = false;
		Log::Info("Game: quit from pause menu");
	}

	if (intent.restart && _states.Is(GameState::InGame))
	{
		Log::Info("Game: restarting mission...");
		_scriptEngine->CleanupMission();
		if (_collectibleManager)
			_collectibleManager->SpawnOnPath();
		if (_inVehicle)
		{
			_inVehicle = false;
			_activeVehicle = nullptr;
		}
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
	return keepRunning;
}

void Game::drawDebugOverlays()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 8.0f, io.DisplaySize.y - 8.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.35f);
	if (ImGui::Begin("Camera position overlay", NULL,
	                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
	                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
	{
		auto const& camPos = _gameCamera->View().GetPosition();
		auto const& camRot = _gameCamera->View().GetOrientation();
		ImGui::Text("Camera Position: %s", camPos.ToString().c_str());
		ImGui::Text("Camera Orientation: %s", camRot.ToString().c_str());
		ImGui::Text("Meshes drawn: %d, culled: %d", CullStats::drawn, CullStats::culled);

		float fov = _gameCamera->View().GetFOV();
		if (ImGui::SliderFloat("FOV", &fov, 0.0f, 120.0f))
			_gameCamera->View().SetFOV(fov);
	}
	ImGui::End();

	// Post-FX debug controls (FX-002 vignette, etc.).
	ImGui::SetNextWindowBgAlpha(0.35f);
	ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_Once);
	// Under the minimap: ImGui's default corner sat on the HUD's coin count.
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 240.0f, 175.0f), ImGuiCond_Once);
	if (ImGui::Begin("Post-FX", NULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
	{
		_postProcess->DebugControls();
		_shadows->DebugControls();
		ImGui::Checkbox("Ground shadows", &_blobShadows->enabled);
	}
	ImGui::End();
}

void Game::updateCharacters(double deltaTime)
{
	if (_character != nullptr)
		_character->Update(deltaTime);
	if (_character && !_inVehicle && !_boarding.car)
		updateFootsteps(_character->GetCharacterController().onGround());
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
	const auto talkingNear = [&](const Vector3& position)
	{ return speaking && (position - _character->GetPosition()).Length() < 8.0f; };
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
					giver.npc->SetRotation(
					    Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f, 0.0f)));
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
					npc->SetRotation(
					    Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(toPlayer.X, toPlayer.Z) + 3.14159265f, 0.0f)));
			}
			npc->Update(deltaTime);
		}
}

void Game::drawBlobShadows(Matrix4x4& viewProjection)
{
	if (!_blobShadows->enabled)
		return;
	_blobShadows->Clear();
	// On the ground under it, as dark as it is near: a character in the air
	// casts a smaller, paler one, gone at 6 m.
	const auto onGround = [&](const Vector3& centre, float aboveFeet, Vector3& ground, float& fade)
	{
		float height = 0.0f;
		if (!_worldPhysics->FindGroundHeight(centre, 0.5f, 30.0f, height, true))
			return false;
		fade = std::clamp(1.0f - std::max(0.0f, centre.Y - aboveFeet - height) / 6.0f, 0.0f, 1.0f);
		ground = Vector3(centre.X, height + 0.02f, centre.Z);
		return fade > 0.0f;
	};
	const auto person = [&](const Vector3& centre, float aboveFeet)
	{
		Vector3 ground;
		float fade = 0.0f;
		if (onGround(centre, aboveFeet, ground, fade))
			_blobShadows->AddDisc(ground, 0.7f * (0.6f + 0.4f * fade), 0.6f * fade);
	};
	std::set<const Vehicle*> cars;
	const auto car = [&](const Vehicle* vehicle)
	{
		if (!vehicle || !cars.insert(vehicle).second)
			return;
		Vector3 ground;
		float fade = 0.0f, halfWidth = 0.0f, halfLength = 0.0f;
		if (!onGround(vehicle->GetPosition(), 0.5f, ground, fade))
			return;
		vehicle->GetFootprint(halfWidth, halfLength);
		_blobShadows->AddBox(ground, vehicle->GetRotation().Yaw(), halfWidth * 1.2f, halfLength * 1.1f, 0.6f * fade);
	};
	if (_character && !_inVehicle)
		person(_character->GetPosition(), _character->CentreHeight());
	for (auto& [name, npc] : _npcs) person(npc->GetPosition(), npc->CentreHeight());
	for (auto& [name, npc] : _ambientNpcs)
		if (_npcs.find(name) == _npcs.end())
			person(npc->GetPosition(), npc->CentreHeight());
	if (_bonusReturn < 0)
		for (auto& giver : _bonusGivers)
			if (giver.npc)
				person(giver.npc->GetPosition(), giver.npc->CentreHeight());
	if (_pedestrianManager && _interior.empty())
		for (const Vector3& at : _pedestrianManager->GetBodyPositions()) person(at, 0.9f);
	car(_activeVehicle);
	car(_scriptEngine->GetPlayerCar());
	if (_scriptEngine->IsMissionActive())
		for (auto& vehicle : _scriptEngine->GetMissionVehicles()) car(vehicle.get());
	if (_hitAndRun)
		for (const Vehicle* cop : _hitAndRun->GetCops()) car(cop);
	if (_trafficManager && !_scriptEngine->IsTrafficSuppressed() && _interior.empty())
		for (const auto& traffic : _trafficManager->GetCars())
		{
			if (traffic.model < 0)
				continue;
			float halfWidth = 0.0f, halfLength = 0.0f;
			_trafficManager->GetFootprint(traffic, halfWidth, halfLength);
			_blobShadows->AddBox(traffic.grounded + Vector3(0.0f, 0.02f, 0.0f), traffic.rotation.Yaw(), halfWidth * 1.2f,
			                     halfLength * 1.1f, 0.6f);
		}
	_blobShadows->Draw(_level->GetWorldShader(), viewProjection);
}

void Game::renderShadows()
{
	if (!_level)
		return;
	GL::ShaderProgram& world = _level->GetWorldShader();
	Vector3 sun;
	if (!_shadows->IsOn() || !_character || !_level->GetSunDirection(sun))
	{
		ShadowMap::Disable(world);
		return;
	}
	ShadowMap::Disable(world);
	_shadows->Begin(_inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : _character->GetPosition(), sun);
	Matrix4x4 lightViewProj = _shadows->GetLightViewProj();
	drawShadowCasters(lightViewProj);
	_shadows->End();
	_shadows->Apply(world);
}

void Game::drawShadowCasters(Matrix4x4& lightViewProj)
{
	if (!_inVehicle)
		_character->Draw(lightViewProj, *_skinShaderProgram, *_resourceManager);
	for (auto* people : {&_npcs, &_ambientNpcs})
		for (auto& [name, npc] : *people) npc->Draw(lightViewProj, *_skinShaderProgram, *_resourceManager);
	GL::ShaderProgram& world = _level->GetWorldShader();
	if (_scriptEngine->IsMissionActive())
		for (auto& vehicle : _scriptEngine->GetMissionVehicles())
			if (vehicle->HasModel())
				vehicle->Draw(lightViewProj, world, true);
	if (Vehicle* parked = _scriptEngine->GetPlayerCar(); parked && parked->HasModel())
		parked->Draw(lightViewProj, world, true);
	if (_hitAndRun)
		for (Vehicle* cop : _hitAndRun->GetCops()) cop->Draw(lightViewProj, world, true);
	if (!_scriptEngine->IsTrafficSuppressed() && _interior.empty())
		_trafficManager->DrawModels(world, lightViewProj);
}

void Game::renderScene(double deltaTime, int viewportWidth, int viewportHeight)
{
	// The 3D scene into the post-process frame buffer (FX-000); the HUD and
	// ImGui are drawn afterwards on the screen, not filtered.
	renderShadows();
	_postProcess->BeginScene(viewportWidth, viewportHeight);

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

	_gameCamera->View().SetAspectRatio(static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight));

	Matrix4x4 viewMatrix = _gameCamera->View().GetViewMatrix();
	Matrix4x4 projMatrix = _gameCamera->View().GetProjectionMatrix();
	Matrix4x4 viewProjection = projMatrix * viewMatrix;
	// The world turned as the camera sees it, for the cars' reflections.
	if (_level)
	{
		GL::ShaderProgram& world = _level->GetWorldShader();
		world.Bind();
		world.SetUniformValue("viewRotation", Matrix3x3(viewMatrix[0][0], viewMatrix[0][1], viewMatrix[0][2], viewMatrix[1][0],
		                                                viewMatrix[1][1], viewMatrix[1][2], viewMatrix[2][0], viewMatrix[2][1],
		                                                viewMatrix[2][2]));
	}

	// Counted afresh each frame, shown the next (the overlay is built first).
	CullStats::drawn = CullStats::culled = 0;
	drawPeople(viewProjection);

	drawVehicles(viewProjection, deltaTime);

	drawEffects(viewProjection, deltaTime);

	glDisable(GL_DEPTH_TEST);
	if (_launch.clean)
		_lineRenderer->Discard();
	else
		_lineRenderer->Flush(viewProjection);
	glEnable(GL_DEPTH_TEST);

	// Onto the screen through the effects (the scene as drawn when they are off).
	_postProcess->EndScene(viewportWidth, viewportHeight, viewProjection);
}

bool Game::drawHud(SpriteBatch& sprites, const GameIntent& intent, const Matrix4x4& proj, double deltaTime, int fps,
                   int viewportWidth, int viewportHeight)
{
	// Not under the game's own main menu page: its entry stands where the
	// mission's objective is.
	if (_textureFontP3D != nullptr && !(_states.Is(GameState::MainMenu) && _mainMenuPage) &&
	    !(_states.Is(GameState::Credits) && _creditsPage) && !_states.Is(GameState::Movie) &&
	    !_states.Is(GameState::LevelEnd) && _scriptEngine->IsHudShown())
	{
		auto font = _resourceManager->GetFont("boulder_16");

		HudState hud;
		hud.viewportWidth = static_cast<float>(viewportWidth);
		hud.viewportHeight = static_cast<float>(viewportHeight);
		hud.fps = _launch.clean ? 0 : fps;
		hud.health = _health;
		hud.inVehicle = _inVehicle;
		hud.boost = intent.boostHeld ? 0.0f : 1.0f; // placeholder, see UI-HUD

		if (_inVehicle && _activeVehicle)
			hud.speedKmh = _activeVehicle->GetSpeedKmh();

		// The game's HUD has its own call-outs for alerts, cards, counts
		// and a stage complete; the old one shows them as banners.
		const bool gameHud = _gameHud && _gameHud->IsLoaded();
		if (_scriptEngine->GetStagesAnnounced() != _messages.stagesAnnounced)
		{
			_messages.stagesAnnounced = _scriptEngine->GetStagesAnnounced();
			_messages.stageCompleteUntil = _elapsedSeconds + 2.0;
		}
		if (gameHud)
		{
			hud.stageComplete = _elapsedSeconds < _messages.stageCompleteUntil;
			hud.walkieTalkie = _walkieTalkie;
			if (_elapsedSeconds < _messages.alertUntil)
				hud.alertTerm = _messages.alertTerm;
			if (_elapsedSeconds < _messages.cardUntil)
			{
				hud.cardFound = _messages.cardFound;
				hud.itemsCount = _messages.itemsCount;
				hud.itemsDone = _messages.itemsDone;
			}
		}
		if (_collectibleManager)
		{
			hud.coins = _coins;
			if (!gameHud && !_messages.alert.empty() && _elapsedSeconds < _messages.alertUntil && hud.banner.empty())
			{
				hud.banner = _messages.alert;
				hud.bannerColour = Vector4(1.0f, 0.25f, 0.2f, 1.0f);
			}
			if (!gameHud && !_messages.card.empty() && _elapsedSeconds < _messages.cardUntil && hud.banner.empty())
			{
				hud.banner = _messages.card;
				hud.bannerColour = Vector4(0.4f, 0.7f, 1.0f, 1.0f);
			}
			hud.coinsTotal = 0;
		}
		// A race or bonus mission on offer, at hand.
		if (const BonusGiver* giver = bonusGiverNear())
		{
			const std::string& title = _textBible.Get(fmt::format("MISSION_TITLE_L{}_M{}", _levelNumber,
			                                                      giver->mission == "sr1"   ? 8
			                                                      : giver->mission == "sr2" ? 9
			                                                      : giver->mission == "sr3" ? 10
			                                                      : giver->mission == "gr1" ? 11
			                                                                                : 12));
			hud.hints.push_back(fmt::format("E: {}", title.empty() ? giver->mission : title));
		}
		hud.cardsTotal = 7;
		if (_collectibleManager)
			hud.cards = _collectibleManager->GetCardsFound();

		fillMissionHud(hud);

		fillRadarHud(hud, gameHud);

		fillHintHud(hud, intent.gamepadConnected, gameHud);

		if (_states.Is(GameState::Splash))
			hud.banner = "PRESS ENTER TO START";

		// Not under the game's pause screen, whose title stands where the
		// objective does.
		if (!(_states.Is(GameState::Paused) && _pausePage))
		{
			if (gameHud)
			{
				_gameHud->Draw(hud, proj, deltaTime);
				_hud->DrawCallouts(sprites, font, hud);
				DrawHints(sprites, font, hud.hints, hud.hintColour, hud.fps, viewportWidth, viewportHeight);
			}
			else
				_hud->Draw(sprites, font, hud);
		}

		if (_states.Is(GameState::Paused) && _pausePage)
		{
			sprites.Flush(proj);
			if (_optionsMenu && _optionsMenu->IsOpen())
				_optionsMenu->Draw(intent, proj);
			else if (_pauseSunday && _pauseSunday->IsOpen())
				_pauseSunday->Draw(intent, proj);
			else
				drawPausePage(intent, proj);
			_window->Swap();
			return true;
		}
		if (_states.Is(GameState::Paused))
		{
			DrawTextMenu(sprites, font, *_pauseMenu, GameText("pause", _language), Vector4(1.0f, 1.0f, 0.0f, 1.0f),
			             Vector4(0.5f, 0.5f, 0.5f, 1.0f), Vector4(1.0f, 0.3f, 0.3f, 1.0f), viewportWidth, viewportHeight,
			             intent.mouseX, intent.mouseY);
			if (intent.uiClick)
				_pauseMenu->CheckClick(intent.mouseX, intent.mouseY);
			sprites.Flush(proj);
			_window->Swap();
			return true;
		}

		drawResultScreen(sprites, font, viewportWidth, viewportHeight);
	}
	return false;
}

void Game::drawScreens(SpriteBatch& sprites, const GameIntent& intent, const Matrix4x4& proj, double deltaTime,
                       int viewportWidth, int viewportHeight)
{
	// The main menu has the show's theme (music02.rcf), the level's music
	// back after it.
	if ((_states.Is(GameState::MainMenu) || (_states.Is(GameState::Movie) && _movies.after == GameState::MainMenu)) !=
	    _menuMusic)
	{
		_menuMusic = !_menuMusic;
		static const std::string kTheme = "sound\\music\\simpsons_theme.rsd";
		if (!_menuMusic)
			_audioManager->ResumeMusicBase();
		else if (!_audioManager->FindSounds(kTheme).empty())
			_audioManager->PlayMusic(kTheme);
	}
	if (_states.Is(GameState::Credits) && _creditsPage)
		drawCredits(intent, proj, deltaTime, viewportWidth, viewportHeight);
	if (_states.Is(GameState::Movie))
		drawMovie(intent, proj, deltaTime, viewportWidth, viewportHeight);
	if (_states.Is(GameState::MainMenu) && _mainMenuPage && !_mainMenu->GetButtons().empty())
	{
		// The menu stands over the Simpsons' living room, not the level.
		updateLivingRoomCamera();
		if (_livingRoom && _livingRoom->IsLoaded() && _level)
			_livingRoom->Draw(_level->GetWorldShader(), *_skinShaderProgram, _level->GetBillboardShader(),
			                  static_cast<float>(viewportWidth) / std::max(1, viewportHeight), deltaTime);
		if (_optionsMenu && _optionsMenu->IsOpen())
			_optionsMenu->Draw(intent, proj);
		else if (_scrapBook && _scrapBook->IsOpen())
			_scrapBook->Draw(intent, proj);
		else
			drawMainMenuPage(intent, proj);
	}
	else if (_states.Is(GameState::MainMenu))
	{
		auto font = _resourceManager->GetFont("boulder_16");
		SpriteBatch menuSprites;
		DrawTextMenu(menuSprites, font, *_mainMenu, "donut", Vector4(1.0f, 0.84f, 0.0f, 1.0f), Vector4(0.8f, 0.8f, 0.8f, 1.0f),
		             Vector4(1.0f, 0.84f, 0.0f, 1.0f), viewportWidth, viewportHeight, intent.mouseX, intent.mouseY);
		if (intent.mouseLeftClick)
			_mainMenu->CheckClick(intent.mouseX, intent.mouseY);
		menuSprites.Flush(proj);
	}

	if (_states.Is(GameState::InGame) || _states.Is(GameState::Paused) || _states.Is(GameState::MissionComplete) ||
	    _states.Is(GameState::MissionFailed))
		drawSubtitle(proj, viewportWidth, viewportHeight);
	sprites.Flush(proj);
	if (_states.Is(GameState::InGame) || _states.Is(GameState::Paused))
		drawTransition(proj, viewportWidth, viewportHeight);
	if (_states.Is(GameState::PhoneBooth))
	{
		_phoneBooth->Draw(intent, proj);
		if (!_phoneBooth->IsOpen() && _states.Is(GameState::PhoneBooth))
			_states.Fire(GameEvent::Play);
	}
	if (_states.Is(GameState::LevelEnd))
		_pauseSunday->Draw(intent, proj);
	if (_states.Is(GameState::Shop))
	{
		_shop->Draw(intent, proj);
		if (!_shop->IsOpen())
			_states.Fire(GameEvent::Play);
	}
}

void Game::drawPeople(Matrix4x4& viewProjection)
{
	// The one saying a line moves their jaw with it; the others keep it shut.
	{
		std::string speaker;
		const float open = _audioManager->GetMouthOpen(speaker);
		const auto mouth = [&](Character* character)
		{
			if (!character)
				return;
			std::string name = character->GetName();
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
			character->SetMouthOpen(!speaker.empty() && name == speaker ? open : 0.0f);
		};
		mouth(_character.get());
		for (auto& [name, npc] : _npcs) mouth(npc.get());
		for (auto& [name, npc] : _ambientNpcs) mouth(npc.get());
		for (auto& giver : _bonusGivers) mouth(giver.npc.get());
		for (auto& [car, driver] : _carDrivers) mouth(driver.get());
	}
	if (_level != nullptr)
	{
		_level->Draw(viewProjection);
		drawBlobShadows(viewProjection);
		// The characters lit by the level's lights, as the cars are.
		_level->ApplyLights(*_skinShaderProgram);
		// The blending the level's glows and particles left (added, one
		// on one) is not the characters': Homer was drawn added onto the
		// street behind him, pale and see-through.
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

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
	for (auto& [name, npc] : _npcs) npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
	for (auto& [name, npc] : _ambientNpcs)
		if (_npcs.find(name) == _npcs.end())
			npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
	if (_bonusReturn < 0)
		for (auto& giver : _bonusGivers)
			if (giver.npc)
				giver.npc->Draw(viewProjection, *_skinShaderProgram, *_resourceManager);
	glEnable(GL_CULL_FACE);
}

void Game::drawVehicles(Matrix4x4& viewProjection, double deltaTime)
{
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
				_carMesh->Draw(*_meshShader, Matrix4x4::MakeTranslate(v->GetPosition()) * Matrix4x4(v->GetRotation()),
				               viewProjection, Vector4(0.2f, 0.5f, 1.0f, 1.0f));
				Vector3 beaconBase = v->GetPosition() + Vector3(0, 1.0f, 0);
				_lineRenderer->DrawLine(beaconBase, beaconBase + Vector3(0, 20.0f, 0), Vector4(0.0f, 1.0f, 1.0f, 1.0f));

				if (_inVehicle && v.get() == _activeVehicle)
				{
					float speed = v->GetSpeedKmh();
					Vector3 exhaustPos = v->GetPosition() + v->GetRotation() * Vector3(-0.5f, -0.2f, -2.2f);
					Vector3 exhaustDir = v->GetRotation() * Vector3(0, 0.1f, -0.5f);
					for (int e = 0; e < 3; ++e)
					{
						Vector3 offset(rand() % 10 * 0.02f - 0.1f, rand() % 10 * 0.02f, rand() % 10 * 0.02f);
						_lineRenderer->DrawLine(exhaustPos + offset, exhaustPos + exhaustDir * (0.5f + speed * 0.02f) + offset,
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
}

void Game::drawEffects(Matrix4x4& viewProjection, double deltaTime)
{
	if (_collectibleManager)
		_collectibleManager->Draw(_level->GetWorldShader(), viewProjection, static_cast<float>(_elapsedSeconds));

	_gags.Draw(_level->GetWorldShader(), *_skinShaderProgram, viewProjection, _gameCamera->View().GetPosition(), _interior,
	           _elapsedSeconds);
	if (_waspManager && _interior.empty())
		_waspManager->Draw(_level->GetWorldShader(), _level->GetBillboardShader(), viewProjection);
	if (_ufo && _interior.empty())
		_ufo->Draw(_level->GetWorldShader(), _level->GetBillboardShader(), viewProjection);
	if (_skidMarks)
	{
		GL::ShaderProgram& world = _level->GetWorldShader();
		world.Bind();
		world.SetUniformValue("viewProj", viewProjection);
		_skidMarks->Draw(world);
	}
	if (_sparks && _dust)
	{
		_sparks->Update(deltaTime, Matrix4x4::Identity);
		_dust->Update(deltaTime, Matrix4x4::Identity);
		updateWheelSprays(deltaTime);
		updateCarSmoke(deltaTime);
		// The dust a dash kicks up, the level's own (its effects file's type 9).
		if (_moves.dashKicksUpDust && !_dashDust && _level)
			if (const ParticleFactory* dash = _level->GetHitParticleFactory(kDashDust))
				_dashDust = std::make_unique<ParticleSystem>(*dash);
		if (_dashDust && _character)
			_dashDust->Update(deltaTime,
			                  Matrix4x4::MakeTranslate(_character->GetPosition() - Vector3(0.0f, kFeetBelowCentre, 0.0f)) *
			                      Matrix4x4(_character->GetRotation()),
			                  -1.0f, _moves.dashKicksUpDust && !_inVehicle);
		GL::ShaderProgram& billboards = _level->GetBillboardShader();
		billboards.Bind();
		billboards.SetUniformValue("viewProj", viewProjection);
		const GLboolean blending = glIsEnabled(GL_BLEND);
		_sparks->Draw(billboards);
		_dust->Draw(billboards);
		for (auto& [vehicle, sprays] : _wheelSprays)
			for (auto& spray : sprays)
				for (auto* system : {spray.grass.get(), spray.dirt.get()})
					if (system)
						system->Draw(billboards);
		if (_dashDust)
			_dashDust->Draw(billboards);
		if (_carSmoke)
			_carSmoke->Draw(billboards);
		if (!blending)
			glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (_scriptEngine->IsMissionActive() && !_scriptEngine->GetCheckpoints().empty())
	{
		_carMesh->Draw(*_meshShader,
		               Matrix4x4::MakeTranslate(_scriptEngine->GetAIPosition()) * Matrix4x4(_scriptEngine->GetAIRotation()),
		               viewProjection, Vector4(1.0f, 0.2f, 0.2f, 1.0f));
	}

	if (_hitAndRun && _interior.empty())
	{
		GL::ShaderProgram& worldShader = _level->GetWorldShader();
		for (Vehicle* cop : _hitAndRun->GetCops())
		{
			cop->Draw(viewProjection, worldShader, true);
			cop->Draw(viewProjection, worldShader, false);
			cop->DrawLights(viewProjection, _level->GetBillboardShader());
		}
	}

	if (_scriptEngine->IsMissionActive())
	{
		auto* chase = _scriptEngine->GetChaseManager();
		if (chase)
		{
			for (auto& cop : chase->GetCopCars())
			{
				_carMesh->Draw(*_meshShader, Matrix4x4::MakeTranslate(cop->GetPosition()) * Matrix4x4(cop->GetRotation()),
				               viewProjection, Vector4(0.1f, 0.1f, 0.1f, 1.0f));
			}
		}
	}
}

void Game::fillMissionHud(HudState& hud)
{
	if (_scriptEngine->IsMissionActive())
	{
		hud.missionActive = true;
		// The mission's title card, for its first few seconds.
		constexpr double kTitleCardSeconds = 4.0;
		if (_states.Is(GameState::InGame) && _elapsedSeconds - _missionLoadedAt < kTitleCardSeconds)
		{
			hud.banner = _scriptEngine->GetMissionTitle();
			hud.bannerColour = Vector4(1.0f, 0.84f, 0.0f, 1.0f);
			hud.bannerDetail = _scriptEngine->GetMissionTitle(true);
			hud.bannerPicture = presentationPicture(_scriptEngine->GetPresentationBitmap());
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

		// The other cars of the mission: the player's own has its own icon.
		for (const auto& v : _scriptEngine->GetMissionVehicles())
			if (v.get() != _scriptEngine->GetPlayerCar() && v.get() != _activeVehicle)
				hud.missionBlips.emplace_back(v->GetPosition().X, v->GetPosition().Z);

		const auto& cps = _scriptEngine->GetCheckpoints();
		for (const auto& cp : cps) hud.checkpointBlips.emplace_back(cp.X, cp.Z);

		// Wherever the objective wants the player -- a checkpoint, a
		// destination, an item to pick up, someone to talk to -- not only
		// a race's next checkpoint, which left every other objective
		// without a marker.
		Vector3 target;
		if (_scriptEngine->GetObjectiveTarget(target))
		{
			const Vector3 from = _inVehicle && _activeVehicle ? _activeVehicle->GetPosition()
			                                                  : (_character ? _character->GetPosition() : Vector3::Zero);
			hud.hasNextCheckpoint = true;
			hud.nextCheckpointXZ = Vector2(target.X, target.Z);
			hud.nextCheckpointDistance = (target - from).Length();
			hud.nextCheckpointLabel = _scriptEngine->GetObjectiveTargetLabel();
		}
	}
}

void Game::fillRadarHud(HudState& hud, bool gameHud)
{
	if (_hitAndRun && (_hitAndRun->GetMeter() > 0.0f || _hitAndRun->IsChasing()))
	{
		hud.chaseActive = true;
		hud.heat = _hitAndRun->GetMeter() / 10.0f;
		for (const Vehicle* cop : _hitAndRun->GetCops())
			hud.policeBlips.emplace_back(cop->GetPosition().X, cop->GetPosition().Z);
	}
	auto* chase = _scriptEngine->GetChaseManager();
	if (chase != nullptr && chase->IsActive())
	{
		hud.chaseActive = true;
		hud.heat = chase->GetHeat();
		hud.busted = chase->IsBusted();
		if (hud.busted && gameHud)
			hud.alertTerm = "BUSTED";
		else if (hud.busted)
			hud.banner = "BUSTED!";
		for (const auto& cop : chase->GetCopCars()) hud.policeBlips.emplace_back(cop->GetPosition().X, cop->GetPosition().Z);
	}

	const Vector3 playerPos =
	    _inVehicle && _activeVehicle ? _activeVehicle->GetPosition() : (_character ? _character->GetPosition() : Vector3::Zero);
	hud.playerXZ = Vector2(playerPos.X, playerPos.Z);
	// For the game's radar: the stage's icon, the player's car while
	// on foot, the phone booths, shops, bonus missions and races.
	hud.objectiveIcon = _scriptEngine->GetHUDIcon();
	if (const Vehicle* car = _scriptEngine->GetPlayerCar(); car && !_inVehicle)
	{
		hud.showPlayerCar = true;
		hud.playerCarXZ = Vector2(car->GetPosition().X, car->GetPosition().Z);
	}
	if (_level)
		for (const auto& trigger : _level->GetTriggers())
		{
			if (trigger.action == "SummonVehiclePhone")
				hud.phoneBooths.emplace_back(trigger.position.X, trigger.position.Z);
			else if (trigger.action == "PurchaseSkin")
				hud.shops.emplace_back(trigger.position.X, trigger.position.Z);
		}
	for (const CarSeller& seller : _carSellers) hud.shops.emplace_back(seller.position.X, seller.position.Z);
	for (const BonusGiver& giver : _bonusGivers)
		(giver.mission.rfind("bm", 0) == 0 ? hud.bonusMissions : hud.streetRaces)
		    .emplace_back(giver.position.X, giver.position.Z);
	// The way the player faces: the car's heading at the wheel (it used
	// to be the on-foot camera's, whatever the car did).
	hud.playerHeading = _inVehicle && _activeVehicle ? _activeVehicle->GetRotation().Yaw() : _gameCamera->GetYaw();
	if (_pathGraph)
		for (const auto& node : _pathGraph->GetNodes())
		{
			const float dx = node.position.X - playerPos.X, dz = node.position.Z - playerPos.Z;
			if (!node.shortcut && dx * dx + dz * dz < hud.radarRange * hud.radarRange * 2.0f)
				hud.roadPoints.emplace_back(node.position.X, node.position.Z);
		}
}

void Game::fillHintHud(HudState& hud, bool pad, bool gameHud)
{
	// The game's HUD has no control hints of its own: only the help
	// overlay's and those that say what is at hand.
	if (_inVehicle)
	{
		hud.hintColour = Vector4(0.5f, 0.8f, 1.0f, 1.0f);
		if (!gameHud)
			hud.hints.push_back(fmt::format("{}: Drive | {}: Boost | {}: Horn | {}: Exit",
			                                ControlLabel(GameAction::MoveForward, pad), ControlLabel(GameAction::Boost, pad),
			                                ControlLabel(GameAction::Honk, pad), ControlLabel(GameAction::Interact, pad)));
	}
	else if (_states.Is(GameState::InGame))
	{
		if (!gameHud)
			hud.hints.push_back(fmt::format("{}: Move | {}: Action | {}: Restart | {}: Pause",
			                                ControlLabel(GameAction::MoveForward, pad), ControlLabel(GameAction::Interact, pad),
			                                ControlLabel(GameAction::RestartMission, pad),
			                                ControlLabel(GameAction::PauseToggle, pad)));

		if (_showHelp)
		{
			hud.hints.push_back("=== CONTROLS ===");
			hud.hints.push_back(fmt::format("{}: Move/Drive", ControlLabel(GameAction::MoveForward, pad)));
			hud.hints.push_back(fmt::format("{}: Action | {}: Boost", ControlLabel(GameAction::Interact, pad),
			                                ControlLabel(GameAction::Boost, pad)));
			hud.hints.push_back(fmt::format("{}: Horn | {}: Restart | {}: Reset", ControlLabel(GameAction::Honk, pad),
			                                ControlLabel(GameAction::RestartMission, pad),
			                                ControlLabel(GameAction::ResetBestTime, pad)));
			hud.hints.push_back(fmt::format("{}: Jump | {}: Debug | {}: Help", ControlLabel(GameAction::VehicleJump, pad),
			                                ControlLabel(GameAction::ToggleDebugDraw, pad),
			                                ControlLabel(GameAction::ToggleHelp, pad)));
			hud.hints.push_back(fmt::format("{}: Pause | {}: Fly", ControlLabel(GameAction::PauseToggle, pad),
			                                ControlLabel(GameAction::CameraLook, pad)));
		}

		if (_character && _scriptEngine->IsMissionActive())
		{
			for (const auto& v : _scriptEngine->GetMissionVehicles())
			{
				const float dist = (v->GetPosition() - _character->GetPosition()).Length();
				if (dist < 5.0f)
					hud.hints.push_back(
					    fmt::format(fmt::runtime(GameText("hint.getin", _language)), ControlLabel(GameAction::Interact, pad)));
				else if (!gameHud)
					hud.hints.push_back(fmt::format(fmt::runtime(GameText("hint.vehicle", _language)), dist));
				break;
			}
		}
	}
}

void Game::drawResultScreen(SpriteBatch& sprites, Font* font, int viewportWidth, int viewportHeight)
{
	if (_states.Is(GameState::MissionComplete) || _states.Is(GameState::MissionFailed))
	{
		MissionResult result;
		result.complete = _states.Is(GameState::MissionComplete);
		result.language = _language;
		// In the game's words, when the phrase table is there.
		const std::string& term = _textBible.Get(result.complete ? "MISSION_COMPLETE" : "MISSION_FAILED");
		result.title = !term.empty() ? term : GameText(result.complete ? "result.complete" : "result.failed", _language);
		result.newRecord = _scriptEngine->IsNewRecord();
		if (result.complete && _storyIndex >= 0 && _storyIndex + 1 == static_cast<int>(_story.size()))
		{
			const std::string& level = _textBible.Get("LEVEL_COMPLETE");
			result.levelComplete = level.empty() ? GameText("result.level", _language) : level;
		}
		result.elapsed = _scriptEngine->GetMissionTime();
		result.best = _scriptEngine->GetBestTime();
		result.reason = _scriptEngine->GetFailureReason();
		result.hint = _scriptEngine->GetFailureHint();
		result.nextIn = static_cast<int>(GameStateMachine::kResultSeconds) - static_cast<int>(_states.GetResultTime());
		DrawMissionResult(sprites, font, result, viewportWidth, viewportHeight);
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
				_gameCamera->View().SetPosition(dest);
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
		_debugWindows.audio = true;

	if (ImGui::MenuItem("Resource Manager"))
		_debugWindows.resourceManager = true;

	if (ImGui::MenuItem("Level Entities"))
		_debugWindows.level = true;

	if (ImGui::MenuItem("Controls"))
		_debugWindows.controls = true;

	if (ImGui::MenuItem("About"))
		_debugWindows.about = true;

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
			// The exporter's settings: what made the file, when, how.
			if (chunk.IsType(P3D::ChunkType::ExportInfo))
			{
				const P3D::ExportInfo info(chunk);
				const auto text = [](std::string s) { return s.substr(0, s.find('\0')); };
				for (const auto& entry : info.GetStrings())
					ImGui::Text("%s: %s", text(entry->GetName()).c_str(), text(entry->GetValue()).c_str());
				for (const auto& entry : info.GetIntegers())
					ImGui::Text("%s: %u", text(entry->GetName()).c_str(), entry->GetValue());
			}

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
	if (!_debugWindows.about)
		return;

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
	const ImVec2 windowSize(600.0f, 520.0f);

	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
	ImGui::SetNextWindowPos(center, ImGuiCond_Once, ImVec2(0.5f, 0.5f));
	if (ImGui::Begin("About", &_debugWindows.about, ImGuiWindowFlags_NoResize))
	{
		ImGui::Markdown(aboutText.c_str(), aboutText.length(), mdConfig);
	}
	ImGui::End();
}

void Game::drawControlsWindow()
{
	if (!_debugWindows.controls)
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
	if (ImGui::Begin("Controls", &_debugWindows.controls))
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
