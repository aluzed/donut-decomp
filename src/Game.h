// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Level.h"
#include "UI/TextBible.h"

#include <array>
#include <map>
#include <set>

#include "Core/Math/Fwd.h"
#include "Core/Math/Vector3.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

enum class GameState
{
	Splash,
	MainMenu,
	Loading,
	InGame,
	Paused,
	MissionComplete,
	MissionFailed,
	Credits
};

class Window;
class AudioManager;
class ResourceManager;
class LineRenderer;
class Level;
class WorldPhysics;
class FreeCamera;
class Character;
class ScriptEngine;
class Vehicle;
class CompositeModel;
class SceneSkins;
struct VertexAnimation;
class WaspManager;
class UfoBoss;
class ParticleFactory;
class ParticleSystem;
class TrafficManager;
class PathGraph;
class PedestrianManager;
class CollectibleManager;
class FrontendProject;
class SimpleMesh;
class Hud;
class GameMenu;
class GameInput;

namespace P3D
{
class P3DFile;
class TextureFont;
} // namespace P3D

namespace GL
{
class ShaderProgram;
class FrameBuffer;
class VertexBuffer;
class VertexBinding;
}

class Game
{
public:
	Game(int argc, char** argv);
	~Game();

	Window& GetWindow() const { return *_window; }

	void Run();
	void LoadModel(const std::string&, const std::string&);

	AudioManager& GetAudioManager() { return *_audioManager; }
	ResourceManager& GetResourceManager() { return *_resourceManager; }
	WorldPhysics& GetWorldPhysics() { return *_worldPhysics; }
	LineRenderer& GetLineRenderer() { return *_lineRenderer; }
	Level& GetLevel() { return *_level; }
	const PathGraph& GetPathGraph() const { return *_pathGraph; }

	ScriptEngine& GetScriptEngine() { return *_scriptEngine; }

	GameState GetState() const { return _gameState; }
	void SetState(GameState state) { _gameState = state; }

	void LockMouse(bool lockMouse);
	void SetPlayerPosition(const Vector3& pos);
	Vector3 GetPlayerPosition() const;
	// The vehicle the player is currently driving, or nullptr on foot. Lets the
	// AI skip a car the player has taken over instead of fighting it for control.
	const Vehicle* GetPlayerVehicle() const { return _inVehicle ? _activeVehicle : nullptr; }
	// Seats the player at the wheel of `vehicle`, as getting in by hand does.
	void PutPlayerInVehicle(Vehicle* vehicle);
	// At a phone booth, on foot: the player's car is brought to the road by it,
	// repaired, with the player at the wheel. False away from a booth.
	bool usePhoneBooth();
	// At a door with a bell, on foot: whoever lives there answers, in one of
	// their recorded lines. False away from one.
	bool ringDoorbell();
	// At a gag worked by hand, on foot: it plays. False away from one.
	bool useGag();
	const TextBible& GetTextBible() const { return _textBible; }
	// A mission's NPC, standing at `position`, when their model has been
	// extracted (art/chars/<name>_m.p3d). Placing one already there moves them.
	void PlaceNpc(const std::string& name, const Vector3& position);
	void ClearNpcs();
	// The interior the player is in ("KwikEMart"), empty outdoors.
	const std::string& GetInterior() const { return _interior; }
	// Whether there is an interior behind the door of that name to go into.
	bool CanEnterInterior(const std::string& name) const;
	// The player's car damage meter, 100 when whole (the HUD's "CAR" bar).
	float GetCarHealth() const { return _health; }
	// The player's coins, kept across sessions in donut_coins.dat.
	int GetCoins() const { return _coins; }
	// Takes `amount` from the purse; false, taking nothing, if it holds less.
	bool SpendCoins(int amount);
	bool IsAutopilot() const { return _autopilot; }
	// The player's character says one of their recorded lines of a kind
	// ("d_bcrash", a big crash; "w_card", a card found...), now and then.
	void SayLine(const std::string& kind);
	int GetLevelNumber() const { return _levelNumber; }
	std::string GetLevelDirectory() const { return levelDirectory(); }
	// The car the player drives ("current" in the mission scripts): the level's
	// own until another is bought.
	const std::string& GetPlayerCarModel() const { return _currentCar.empty() ? _playerCarModel : _currentCar; }
	// What a car costs (rewards.mfk's BindReward ... "forsale" ..., cost), 0 when
	// it is not for sale or unknown.
	int CarPrice(const std::string& car) const { return rewardPrice(car, "car"); }
	int SkinPrice(const std::string& skin) const { return rewardPrice(skin, "skin"); }
	// Buys outfit `skin` ("m_prison") like a car, and the player wears it.
	bool BuySkin(const std::string& skin);
	// Buys `car` if the purse holds its price, and drives it from then on. True
	// once the player owns it (--autopilot is given it).
	bool BuyCar(const std::string& car);
	// Level 7's saucer ("destroyboss"): whether the player's car is in its
	// beam, and a barrel blown up against it there (`last`, it comes down).
	// False, and nothing, when the level has none.
	bool IsPlayerInBossBeam() const;
	void HitBoss(bool last);
	// The ground under it, where the player is to drive the barrel.
	bool GetBossGround(Vector3& position) const;
	bool HasBoss() const { return _ufo != nullptr; }
	void AddShake(float amount) { _shakeAmount = std::max(_shakeAmount, amount); }

	static Game& GetInstance() { return *instance; }

private:
	void imguiMenuBar();
	void guiModelMenu(Character&);
	void loadGlobal();
	void debugDrawP3D(const P3D::P3DFile&);
	void debugAboutMenu();
	void drawControlsWindow();
	void OnInputTextEntry(const std::string& text);
	void ensureSceneFBO(int width, int height);
	void ensureFullscreenQuad();
	void blitSceneToBackbuffer(int width, int height);

	std::unique_ptr<Window> _window;
	std::unique_ptr<AudioManager> _audioManager;
	std::unique_ptr<ResourceManager> _resourceManager;
	std::unique_ptr<FreeCamera> _camera;
	std::unique_ptr<Character> _character;
	std::unique_ptr<LineRenderer> _lineRenderer;
	std::unique_ptr<Level> _level;
	std::unique_ptr<WorldPhysics> _worldPhysics;
	std::unique_ptr<ScriptEngine> _scriptEngine;
	std::unique_ptr<TrafficManager> _trafficManager;
	std::unique_ptr<PathGraph> _pathGraph;
	std::unique_ptr<PedestrianManager> _pedestrianManager;
	std::unique_ptr<CollectibleManager> _collectibleManager;
	std::unique_ptr<WaspManager> _waspManager;
	std::unique_ptr<UfoBoss> _ufo;
	// The bursts the game sets off: sparks off a crash, dust under a landing.
	std::unique_ptr<ParticleFactory> _sparkFactory, _dustFactory;
	std::unique_ptr<ParticleSystem> _sparks, _dust;
	// What a car's rear wheels throw up off the road, the level's own sprays:
	// grass on grass, earth and stones on dirt, sand, gravel.
	struct WheelSpray
	{
		std::unique_ptr<ParticleSystem> grass, dirt;
	};
	std::map<const Vehicle*, std::array<WheelSpray, 2>> _wheelSprays;
	void updateWheelSprays(double dt);
	// A barrel blown up against the saucer, at `blast`: sparks, dust, a shake,
	// the explosion heard.
	void bossBlast(const Vector3& blast, bool last);
	// The player's car near wrecked smokes and burns from under its hood (the
	// effects file's car damage effect).
	std::unique_ptr<ParticleSystem> _carSmoke;
	const Vehicle* _carSmokeVehicle = nullptr;
	void updateCarSmoke(double dt);
	// The marks skidding tyres leave.
	std::unique_ptr<class SkidMarks> _skidMarks;
	std::unique_ptr<class HitAndRun> _hitAndRun;
	// Runs the Hit & Run meter: people run down, the police, busted.
	void updateHitAndRun(double dt);
	std::unique_ptr<GameInput> _gameInput;
	std::unique_ptr<FrontendProject> _frontend;
	// The game's main menu page (scrooby2's MainMenu.pag) and the entry it
	// shows; left and right as they were last frame.
	std::unique_ptr<FrontendProject> _mainMenuPage;
	// The game's loading screen (backend.p3d's Loading.pag), and one frame of
	// it drawn before a level loads.
	std::unique_ptr<FrontendProject> _loadingPage;
	// The game's pause screen (ingame.p3d's Pause.pag and PauseMission.pag):
	// its entry picked, and up and down and the mouse as they were.
	std::unique_ptr<FrontendProject> _pausePage;
	int _pauseChoice = 0;
	bool _menuUpHeld = false, _menuDownHeld = false;
	int _pauseMouseX = -1, _pauseMouseY = -1;
	void drawPausePage(const struct GameIntent& intent, const Matrix4x4& proj);
	void showLoadingScreen();
	std::size_t _menuChoice = 0;
	bool _menuLeftHeld = false, _menuRightHeld = false;
	bool _menuShown = false;
	void drawMainMenuPage(const struct GameIntent& intent, const Matrix4x4& proj);
	std::unique_ptr<GL::ShaderProgram> _meshShader;
	std::unique_ptr<SimpleMesh> _playerMesh;
	std::unique_ptr<SimpleMesh> _carMesh;
	std::unique_ptr<GameMenu> _mainMenu;
	std::unique_ptr<GameMenu> _pauseMenu;
	std::unique_ptr<Hud> _hud;
	std::unique_ptr<GL::ShaderProgram> _postProcessShader;
	std::unique_ptr<GL::FrameBuffer> _sceneFBO;
	std::shared_ptr<GL::VertexBuffer> _fullscreenQuadVB;
	std::shared_ptr<GL::VertexBinding> _fullscreenQuadBinding;
	int _sceneFboW = 0;
	int _sceneFboH = 0;
	float _vignetteIntensity = 0.0f;
	std::unique_ptr<P3D::P3DFile> _animP3D;
	std::unique_ptr<P3D::P3DFile> _globalP3D;
	std::unique_ptr<P3D::TextureFont> _textureFontP3D;

	std::unique_ptr<Character> _npcCharacter;

	std::unique_ptr<GL::ShaderProgram> _skinShaderProgram;

	bool _showDebug = true;
	bool _showHelp = false;
	float _health = 100.0f;
	float _shakeAmount = 0.0f;
	float _prevVehicleY = 0.0f;
	float _hornCooldown = 0.0f;

	GameState _gameState = GameState::InGame;

	// Unattended-run switches (--autostart / --quit-after), for measuring the AI
	// over a whole mission without a human holding ENTER down for two minutes.
	bool _autoStart = false;
	double _quitAfterSeconds = 0.0; // 0 = run until the window is closed
	// The mission script started and restarted; --mission overrides it.
	std::string _missionScript = "scripts/Missions/level01/M1race.con";
	// --autopilot: the player is moved onto each objective in turn, so a whole
	// mission can be run unattended to check that its stages follow on.
	bool _autopilot = false;
	double _autopilotTimer = 0.0;
	void runAutopilot(double dt);
	// Runs _missionScript, preceded by its load script when it has one.
	void runMissionScript();
	// Level 1's story missions in order, as found under scripts/level01 (copied
	// off the CD, see docs/ASSETS.md); empty when they are not there.
	std::vector<std::string> _story;
	// Which of them is being played, or -1 outside the story.
	int _storyIndex = -1;
	// --story: start on the first story mission rather than the test mission.
	bool _startStory = false;
	// --clean: the game as it is played, without the developer's overlays.
	bool _clean = false;
	int _storyFrom = 0;
	// --level: which level to play, and where its files are.
	int _levelNumber = 1;
	bool _missionScriptGiven = false;
	// "level01" for level 1: the folder of its scripts and mission art.
	std::string levelDirectory() const;
	// The level's world files in art/: terrain, zones, roads and interiors.
	std::vector<std::string> levelWorldFiles() const;
	// The first argument of every `command` call in the level's level.mfk,
	// in order: levelScriptCalls("AddMission") is the story.
	std::vector<std::string> levelScriptCalls(const std::string& command, const std::string& script = "level.mfk") const;
	// The level's player character and car, from leveli.mfk's AddCharacter and
	// InitLevelPlayerVehicle: Homer and the family sedan on level 1, Bart and
	// the Honor Roller on level 2.
	std::string _playerCharacter = "homer";
	float _fallOffY = -50.0f;
	std::string _currentCar;
	std::set<std::string> _ownedSkins;
	// rewards.mfk's BindReward for `name` of `type` ("car", "skin") for sale:
	// its cost, and the model file it names. 0 when there is none.
	int rewardPrice(const std::string& name, const std::string& type, std::string* file = nullptr) const;
	std::set<std::string> _ownedCars;
	std::string _playerCarModel = "famil_v";
	// The player's animations by what they are, without the character's
	// prefix: "loco_run" is hom_loco_run for Homer, brt_loco_run for Bart.
	std::map<std::string, std::string> _playerAnimations;
	// Played once, and what to go back to after; the idle ones are picked after
	// a while standing still.
	bool _playerOneShot = false;
	double _playerIdleFor = 0.0;
	double _playerAirborneFor = 0.0;
	double _playerGestureAt = 0.0;
	double _lastJumpAt = -100.0;
	bool _playerDashing = false;
	// Dashing along the ground this frame, and the dust it kicks up.
	bool _dashKicksUpDust = false;
	std::unique_ptr<ParticleSystem> _dashDust;
	bool _kickedThisJump = false;
	// Plays the player's animation `what` ("loco_run"), if the character has it.
	bool playPlayerAnimation(const std::string& what, bool loop);
	// Picks the player's animation each frame: run, jump, land, idle.
	void updatePlayerAnimation(double dt, bool moving, bool onGround);
	// The player's footsteps, a stride at a time, by the ground underfoot
	// (concrete, metal, wood, dirt), in the character's own shoes; and its
	// landing.
	void updateFootsteps(bool onGround);
	Vector3 _lastFootstepAt;
	bool _footstepWasOnGround = true;
	// The character's sound script ("homer"), found once.
	std::string _footstepScript;
	// Getting into or out of a car, by its driver's door: the door opens, the
	// player climbs in or out, the door shuts -- three of the character's
	// animations. The player has no control meanwhile.
	struct Boarding
	{
		Vehicle* car = nullptr;
		bool in = true;
		int step = -1;
	};
	Boarding _boarding;
	void startBoarding(Vehicle* car, bool in);
	void updateBoarding();
	Vector3 driverDoor(const Vehicle& car) const;
	// The debug Warp menu: the level script's AddTeleportDest places.
	std::vector<std::pair<std::string, Vector3>> _teleports;
	// The level's interactive gags (level.mfk's GagBegin ... GagEnd): Moe's
	// pickle jar, the Simpsons' couch. Each plays its sound when walked into
	// ("touch") or worked by hand ("action"), and pays its coins the first time.
	struct Gag
	{
		std::string model;
		// Its model, as it stands before it plays (art/nis/gags/), where
		// GagSetPosition puts it. Not animated.
		std::shared_ptr<CompositeModel> drawable;
		// The characters it holds, if any (Homer on the couch).
		std::shared_ptr<SceneSkins> skins;
		// A mesh of it that changes shape (the blow-up doll inflating).
		std::shared_ptr<VertexAnimation> morph;
		Vector3 placed;
		// GagSetCycle: "cycle" plays round and round; "single" plays once when
		// triggered and stays as it ends; "reset" plays once and goes back.
		std::string cycle;
		// When it was last triggered, < 0 when it stands at rest.
		double animStartedAt = -1.0;
		// GagSetRandom: one of the gags sharing its trigger is picked each time
		// (the Simpsons' couch has six, from Grandpa walking in to Lisa).
		bool random = false;
		std::string sound;
		bool byHand = false;
		Vector3 position;
		float radius = 1.0f;
		int coins = 0;
		// GagSetCameraShake: the power plant's meltdown shakes the view.
		bool shakes = false;
		bool played = false;
		bool inside = false;
		double lastPlayed = -100.0;
	};
	std::vector<Gag> _gags;
	// False until the first look after loading: a gag the player starts the
	// level standing in is not walked into.
	bool _gagsPrimed = false;
	// Gags found are kept in donut_gags.dat, keyed by level, model and place.
	void loadGags();
	std::string gagId(const Gag& gag) const;
	void saveGags() const;
	void playGag(Gag& gag);
	void updateGags();
	void drawGags(const Matrix4x4& viewProjection);
	// When the running mission was loaded, for its title card.
	double _missionLoadedAt = 0.0;
	// The game's phrase table, in the language --language picks (E by default).
	TextBible _textBible;
	std::map<std::string, std::unique_ptr<Character>> _npcs;
	// A non-player character's rest pose, and its gestures while it talks.
	static std::string npcRestAnimation(const Character& npc);
	// Sits `character` at `car`'s wheel, idling: false when the car has no seat.
	static bool seatDriver(Character& character, const Vehicle& car);
	// The mission cars' drivers, by car; made as they are first seen.
	std::map<const Vehicle*, std::unique_ptr<Character>> _carDrivers;
	std::set<std::string> _driversWithoutModel;
	void drawCarDrivers(const Matrix4x4& viewProjection);
	void animateNpc(Character& npc, bool talking);
	struct NpcGesture
	{
		double nextAt = 0.0;
		bool gesturing = false;
		bool talking = false;
	};
	std::map<const Character*, NpcGesture> _npcGestures;
	// The level's own people, standing about (leveli.mfk's AddAmbientCharacter:
	// Apu behind his counter, Ralph, the Moleman...). One a mission has placed
	// as its own NPC is not shown twice.
	std::map<std::string, std::unique_ptr<Character>> _ambientNpcs;
	// Who offers the level's street races and bonus mission, where, and which
	// (leveli.mfk's AddNPCCharacterBonusMission: Milhouse, sr1, by the school).
	struct BonusGiver
	{
		std::string mission;
		Vector3 position;
		std::unique_ptr<Character> npc;
	};
	std::vector<BonusGiver> _bonusGivers;
	// Playing a giver's mission: the story mission to go back to after it.
	int _bonusReturn = -1;
	// The giver within reach of a player on foot, or null.
	const BonusGiver* bonusGiverNear() const;
	bool startBonusMission();
	// A character to stand in the scene, idling; null without a model or body.
	std::unique_ptr<Character> makeNpc(const std::string& name) const;

	// Interiors. A door's trigger (a type 7 locator, carrying the interior's
	// name) takes the player on foot inside; the interior's exit brings them
	// back out in front of the door.
	std::string _interior;
	Level::Interior _interiorLayout;
	Vector3 _interiorReturn;
	// The exit only works once the player has stepped away from it: they come
	// in standing next to it.
	bool _interiorExitArmed = false;
	void enterInterior(const Level::Trigger& door);
	void updateInterior();
	char _language = 'E';
	// Tears down whatever mission is running and starts story mission `index`.
	void startStoryMission(int index);
	// Where the story continues, kept in donut_progress.dat: the level and the
	// mission after the last won. Level 1's first when there is none.
	struct StoryProgress
	{
		int level = 1;
		int mission = 0;
	};
	StoryProgress loadStoryProgress() const;
	void saveStoryProgress(int level, int next) const;
	// The level's world and everything built on it: its files, road graph,
	// traffic, pedestrians, coins and cards, story and player.
	void loadLevel();
	// Tears the current level down, physics world included, and loads level
	// `level` in its place, with its own player; no mission is started.
	void switchLevel(int level);
	// Whether level `level`'s scripts were copied off the CD.
	bool levelAvailable(int level) const;
	int _coins = 0;
	double _lastLineAt = -10.0;
	// The car whose engine is heard ("famil_v"), empty on foot.
	std::string _engineCar;
	// A found card's title, shown for a while ("CRAB JUICE").
	std::string _cardMessage;
	double _cardMessageUntil = 0.0;
	// "Hit & Run!", "BUSTED!": shown for a while, in red.
	std::string _alertMessage;
	double _alertUntil = 0.0;
	void alert(const std::string& term, const char* fallback);
	// The view rotation for a camera looking along heading `yaw` (radians about
	// +Y, 0 is +Z), tipped `pitchDown` radians below the horizon.
	static Quaternion cameraLooking(float pitchDown, float yaw);
	// Health lost to impacts, judged from the car's velocity a frame apart.
	void updateCarDamage();
	// The player's car shows its dents: its hood, doors and back swap to their
	// damaged textures as its health runs down.
	void updateCarDamageLook(double dt);
	std::string _damageLookModel;
	int _damageLookStage = 0;
	double _damageLookAge = 0.0;
	const Vehicle* _damageVehicle = nullptr;
	Vector3 _damageVelocity;
	Vector3 _damagePosition;
	void saveCoins() const;
	// `wanted`, or the nearest point to `subject` on the way to it that the level
	// does not hide `subject` from.
	Vector3 unoccludedCameraPosition(const Vector3& subject, const Vector3& wanted) const;
	double _elapsedSeconds = 0.0;

	bool _inVehicle = false;
	Vehicle* _activeVehicle = nullptr;
	double _missionCompleteTimer = 0.0;
	Vector3 _smoothCamPos = Vector3::Zero;
	float _camYaw = 0.0f;     // third-person camera orbit yaw (mouse-controlled)
	float _camPitch = -0.2f;  // third-person camera orbit pitch (mouse-controlled; negative looks down)
	Vector3 _lastSafePos = Vector3(220, 4.5f, -172);
	// Where the car the player drives last had a wheel on the ground: a car
	// that falls off the map is put back on the road nearest to it, rather
	// than the player being set down on foot where they last walked -- level 6
	// m2's follow lost its truck that way, 200m off at the mission's start.
	const Vehicle* _safeCar = nullptr;
	Vector3 _safeCarPos;
	float _safeCarYaw = 0.0f;

	bool _debugResourceManagerWindowOpen = false;
	bool _debugLevelWindowOpen = false;
	bool _debugAudioWindowOpen = false;
	bool _debugAboutWindowOpen = false;
	bool _showControls = false;
	
	static Game* instance;
};

} // namespace Donut
