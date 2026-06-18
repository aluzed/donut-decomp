// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <Input/ButtonCodes.h>
#include <Input/GameAction.h>

#include <SDL.h>

#include <array>
#include <vector>

namespace Donut
{

/** Which physical device/channel an InputSource refers to. */
enum class InputSourceKind
{
	None = 0,
	Key,         // keyboard key or mouse button (Button enum)
	GamepadButton, // SDL_GameController digital button
	GamepadAxis,   // half of an SDL_GameController analog axis (incl. triggers)
};

/**
 * A single physical input that can drive an action. An action may be bound to
 * several of these (e.g. an arrow key AND a gamepad axis); any of them can
 * trigger the action, and GetActionAxis() combines their magnitudes.
 *
 * This is a flat, copyable value type so the whole keymap is trivial to clone,
 * diff and serialise. The string<->enum conversions for `button`, `gpButton`
 * and `gpAxis` live with the parser/serialiser (ticket #2/#6), not here.
 */
struct InputSource
{
	InputSourceKind kind = InputSourceKind::None;

	// kind == Key
	Button button = Button::None;

	// kind == GamepadButton
	SDL_GameControllerButton gpButton = SDL_CONTROLLER_BUTTON_INVALID;

	// kind == GamepadAxis
	SDL_GameControllerAxis gpAxis = SDL_CONTROLLER_AXIS_INVALID;
	// Which half of the axis activates the action: +1 for the positive
	// direction, -1 for the negative. Triggers only use +1.
	float axisDir = 1.0f;
	// Normalised 0..1 magnitude below which the axis reads as zero. Lets each
	// stick/trigger binding carry its own dead zone.
	float deadzone = 0.25f;

	static InputSource Key(Button b)
	{
		InputSource s;
		s.kind = InputSourceKind::Key;
		s.button = b;
		return s;
	}

	static InputSource Pad(SDL_GameControllerButton b)
	{
		InputSource s;
		s.kind = InputSourceKind::GamepadButton;
		s.gpButton = b;
		return s;
	}

	static InputSource Axis(SDL_GameControllerAxis axis, float dir, float deadzone = 0.25f)
	{
		InputSource s;
		s.kind = InputSourceKind::GamepadAxis;
		s.gpAxis = axis;
		s.axisDir = dir < 0.0f ? -1.0f : 1.0f;
		s.deadzone = deadzone;
		return s;
	}
};

/**
 * The full action->inputs table. Index with the GameAction value:
 *   keymap[static_cast<size_t>(GameAction::MoveForward)]
 * yields the list of physical inputs bound to MoveForward.
 *
 * Empty vector == action currently unbound.
 */
using Keymap = std::array<std::vector<InputSource>, GameActionCount>;

inline std::vector<InputSource>& BindingsFor(Keymap& map, GameAction action)
{
	return map[static_cast<std::size_t>(action)];
}

inline const std::vector<InputSource>& BindingsFor(const Keymap& map, GameAction action)
{
	return map[static_cast<std::size_t>(action)];
}

} // namespace Donut
