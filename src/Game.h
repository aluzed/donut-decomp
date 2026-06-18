// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

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
class TrafficManager;
class PathGraph;
class PedestrianManager;
class CollectibleManager;
class FrontendProject;
class SimpleMesh;
class GameMenu;

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

	ScriptEngine& GetScriptEngine() { return *_scriptEngine; }

	GameState GetState() const { return _gameState; }
	void SetState(GameState state) { _gameState = state; }

	void LockMouse(bool lockMouse);
	void SetPlayerPosition(const Vector3& pos);
	Vector3 GetPlayerPosition() const;
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
	std::unique_ptr<FrontendProject> _frontend;
	std::unique_ptr<GL::ShaderProgram> _meshShader;
	std::unique_ptr<SimpleMesh> _playerMesh;
	std::unique_ptr<SimpleMesh> _carMesh;
	std::unique_ptr<SimpleMesh> _buildingMesh;
	std::unique_ptr<GameMenu> _mainMenu;
	std::unique_ptr<GameMenu> _pauseMenu;
	std::unique_ptr<GL::ShaderProgram> _postProcessShader;
	std::unique_ptr<GL::FrameBuffer> _sceneFBO;
	std::shared_ptr<GL::VertexBuffer> _fullscreenQuadVB;
	std::shared_ptr<GL::VertexBinding> _fullscreenQuadBinding;
	std::unique_ptr<P3D::P3DFile> _animP3D;
	std::unique_ptr<P3D::P3DFile> _globalP3D;
	std::unique_ptr<P3D::TextureFont> _textureFontP3D;

	std::unique_ptr<Character> _npcCharacter;

	std::unique_ptr<GL::ShaderProgram> _skinShaderProgram;

	bool _mouseLocked;
	int _lockedMousePosX;
	int _lockedMousePosY;
	bool _showDebug = true;
	bool _showHelp = false;
	float _health = 100.0f;
	std::vector<std::pair<Vector3, float>> _buildings;
	float _shakeAmount = 0.0f;
	float _prevVehicleY = 0.0f;

	GameState _gameState = GameState::InGame;
	bool _inVehicle = false;
	Vehicle* _activeVehicle = nullptr;
	double _missionCompleteTimer = 0.0;
	Vector3 _smoothCamPos = Vector3::Zero;
	Vector3 _lastSafePos = Vector3(220, 4.5f, -172);

	bool _debugResourceManagerWindowOpen = false;
	bool _debugLevelWindowOpen = false;
	bool _debugAudioWindowOpen = false;
	bool _debugAboutWindowOpen = false;
	bool _showControls = false;
	
	static Game* instance;
};

} // namespace Donut
