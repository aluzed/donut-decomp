// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"

struct SDL_Window;

namespace Donut
{

// Per-frame snapshot of every input the gameplay loop needs. `Game::Run`
// consumes only this struct (plus a couple of GameInput helpers) so that no
// SDL_* / Input::* calls leak into the game loop itself (GAME-001a).
struct GameIntent
{
	// Analog movement (keyboard = 0/1, sticks scaled). Already in the canonical
	// Forward/Backward/Left/Right basis; gameplay combines them as needed.
	float moveForward = 0.0f;
	float moveBackward = 0.0f;
	float moveLeft = 0.0f;
	float moveRight = 0.0f;

	// Mouse motion accumulated since the last frame (relative mode).
	float mouseDX = 0.0f;
	float mouseDY = 0.0f;
	// Absolute mouse position (for menu hover/click hit-testing).
	int mouseX = 0;
	int mouseY = 0;

	// Held state.
	bool boostHeld = false;

	// Rising-edge (just-pressed) actions.
	bool interact = false;
	bool honk = false;
	bool vehicleJump = false;
	bool debugTeleportToVehicle = false;
	bool uiConfirm = false;
	bool uiClick = false;       // high-level UIClick action
	bool mouseLeftClick = false; // raw left mouse button edge (menus)
	bool pauseToggle = false;
	bool quit = false;
	bool resetBest = false;
	bool restart = false;
	bool toggleDebug = false;
	bool toggleHelp = false;

	// Set when an SDL_QUIT event (or window-close) was received.
	bool requestQuit = false;

	// Misc device state.
	bool gamepadConnected = false;
};

class GameInput
{
public:
	GameInput();

	// Pumps the SDL event queue (dispatching to ImGui + the Input layer) and
	// returns a snapshot of the resulting per-frame intent. `requestQuit` is
	// also reflected in `intent.requestQuit`.
	GameIntent Poll(SDL_Window* window);

	// Captures/releases the mouse for orbit-camera gameplay (relative mode).
	// Idempotent: calling with the same state is a no-op.
	void LockMouse(SDL_Window* window, bool lockMouse);

	bool IsMouseLocked() const { return _mouseLocked; }

private:
	bool _mouseLocked = false;
	int _lockedMousePosX = 0;
	int _lockedMousePosY = 0;
};

} // namespace Donut
