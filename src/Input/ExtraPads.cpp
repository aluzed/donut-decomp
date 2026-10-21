#include "Input/ExtraPads.h"

#include "Core/Log.h"
#include "Input/Input.h"

#include <SDL.h>

#include <cmath>

namespace Donut
{

namespace
{
constexpr float kDeadZone = 0.25f;

float axis(SDL_GameController* pad, SDL_GameControllerAxis which)
{
	const float value = static_cast<float>(SDL_GameControllerGetAxis(pad, which)) / 32767.0f;
	return std::fabs(value) < kDeadZone ? 0.0f : value;
}

bool button(SDL_GameController* pad, SDL_GameControllerButton which)
{
	return SDL_GameControllerGetButton(pad, which) != 0;
}
} // namespace

ExtraPads::~ExtraPads()
{
	for (SDL_GameController*& pad : _pads)
		if (pad)
		{
			SDL_GameControllerClose(pad);
			pad = nullptr;
		}
}

const std::array<ExtraPads::Pad, ExtraPads::kCount>& ExtraPads::Poll()
{
	// Those unplugged.
	for (SDL_GameController*& pad : _pads)
		if (pad && !SDL_GameControllerGetAttached(pad))
		{
			SDL_GameControllerClose(pad);
			pad = nullptr;
		}
	// Those plugged in that neither player 1 nor another player has.
	const SDL_JoystickID first =
	    Input::GetGamepad() ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(Input::GetGamepad())) : -1;
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (!SDL_IsGameController(i))
			continue;
		const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
		if (id == first)
			continue;
		bool taken = false;
		for (SDL_GameController* pad : _pads)
			taken = taken || (pad && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id);
		if (taken)
			continue;
		for (SDL_GameController*& pad : _pads)
			if (!pad)
			{
				pad = SDL_GameControllerOpen(i);
				Log::Info("ExtraPads: a pad for another player ({})", SDL_GameControllerName(pad));
				break;
			}
	}

	for (int i = 0; i < kCount; ++i)
	{
		Pad& state = _state[i];
		SDL_GameController* pad = _pads[i];
		state = Pad();
		if (!pad)
			continue;
		state.connected = true;
		const float x = axis(pad, SDL_CONTROLLER_AXIS_LEFTX), y = axis(pad, SDL_CONTROLLER_AXIS_LEFTY);
		state.left = x < -0.5f || button(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		state.right = x > 0.5f || button(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
		state.up = y < -0.5f || button(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
		state.down = y > 0.5f || button(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		state.steer = x != 0.0f ? x : (state.right ? 1.0f : 0.0f) - (state.left ? 1.0f : 0.0f);
		state.gas = std::max(axis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), state.up ? 1.0f : 0.0f);
		state.brake = std::max(axis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT), state.down ? 1.0f : 0.0f);
		state.a = button(pad, SDL_CONTROLLER_BUTTON_A);
		state.b = button(pad, SDL_CONTROLLER_BUTTON_B);
		state.x = button(pad, SDL_CONTROLLER_BUTTON_X);
		state.start = button(pad, SDL_CONTROLLER_BUTTON_START);
		state.nitro = button(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || button(pad, SDL_CONTROLLER_BUTTON_Y);
	}
	return _state;
}

} // namespace Donut
