// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Game/GameInput.h"

#include "Input/ButtonCodes.h"
#include "Input/GameAction.h"
#include "Input/Input.h"

#include <SDL.h>

#include "Render/imgui/imgui.h"
#include "Render/imgui/imgui_impl_sdl.h"

namespace Donut
{

GameInput::GameInput() = default;

GameIntent GameInput::Poll(SDL_Window* window)
{
	(void)window;

	GameIntent intent;

	Input::PreEvent();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			intent.requestQuit = true;

		ImGui_ImplSDL2_ProcessEvent(&event);
		Input::HandleEvent(event);
	}

	// Analog movement axes.
	intent.moveForward = Input::GetActionAxis(GameAction::MoveForward);
	intent.moveBackward = Input::GetActionAxis(GameAction::MoveBackward);
	intent.moveLeft = Input::GetActionAxis(GameAction::MoveLeft);
	intent.moveRight = Input::GetActionAxis(GameAction::MoveRight);

	// Mouse.
	intent.mouseDX = Input::GetMouseDeltaX();
	intent.mouseDY = Input::GetMouseDeltaY();
	intent.mouseX = Input::GetMouseX();
	intent.mouseY = Input::GetMouseY();

	// Held state.
	intent.boostHeld = Input::IsActionDown(GameAction::Boost);

	// Edges.
	intent.interact = Input::JustPressed(GameAction::Interact);
	intent.honk = Input::JustPressed(GameAction::Honk);
	intent.vehicleJump = Input::JustPressed(GameAction::VehicleJump);
	intent.debugTeleportToVehicle = Input::JustPressed(GameAction::DebugTeleportToVehicle);
	intent.uiConfirm = Input::JustPressed(GameAction::UIConfirm);
	intent.uiClick = Input::JustPressed(GameAction::UIClick);
	intent.mouseLeftClick = Input::JustPressed(Button::MouseLeft);
	intent.pauseToggle = Input::JustPressed(GameAction::PauseToggle);
	intent.quit = Input::JustPressed(GameAction::QuitGame);
	intent.resetBest = Input::JustPressed(GameAction::ResetBestTime);
	intent.restart = Input::JustPressed(GameAction::RestartMission);
	intent.toggleDebug = Input::JustPressed(GameAction::ToggleDebugDraw);
	intent.toggleHelp = Input::JustPressed(GameAction::ToggleHelp);

	intent.gamepadConnected = Input::IsGamepadConnected();

	return intent;
}

void GameInput::LockMouse(SDL_Window* window, bool lockMouse)
{
	if (_mouseLocked == lockMouse)
		return;

	_mouseLocked = lockMouse;

	SDL_SetRelativeMouseMode(lockMouse ? SDL_TRUE : SDL_FALSE);

	if (lockMouse)
		SDL_GetMouseState(&_lockedMousePosX, &_lockedMousePosY);

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	SDL_WarpMouseInWindow(window, w / 2, h / 2);

	Input::ResetMouseDelta();
}

} // namespace Donut
