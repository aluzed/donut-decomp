// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <cstddef>
#include <string_view>

namespace Donut
{

/**
 * High-level, device-independent game actions.
 *
 * Gameplay code queries these (Input::IsActionDown(GameAction::MoveForward), ...)
 * instead of physical keys/buttons. The binding from an action to one or more
 * physical inputs (keyboard key, gamepad button, gamepad axis) lives in the
 * keymap (loaded from keymap.conf). See docs/keymap.md for the file format.
 *
 * The order here is the canonical iteration order used when serialising
 * keymap.conf, so keep new actions grouped sensibly and avoid reordering
 * existing ones gratuitously.
 */
enum class GameAction : std::size_t
{
	// --- Movement (shared by the on-foot character and vehicle driving) ---
	MoveForward = 0, // walk forward / throttle
	MoveBackward,    // walk back / brake-reverse
	MoveLeft,        // strafe-left / steer-left
	MoveRight,       // strafe-right / steer-right

	// --- Character / context-sensitive ---
	Interact, // enter vehicle when near one, exit vehicle, or jump on foot

	// --- Vehicle ---
	Boost,       // temporary speed boost
	Honk,        // horn
	VehicleJump, // hop / jump the vehicle

	// --- Free (debug) camera, active while CameraLook is held ---
	CameraLook,      // hold to capture the mouse and look around
	FreeCamForward,  // fly camera forward
	FreeCamBackward, // fly camera back
	FreeCamLeft,     // fly camera left
	FreeCamRight,    // fly camera right
	FreeCamUp,       // fly camera up
	FreeCamDown,     // fly camera down
	FreeCamFast,     // hold to move the fly camera faster

	// --- UI / global ---
	UIConfirm,  // advance splash / confirm
	UIClick,    // primary click in menus
	PauseToggle, // toggle pause
	QuitGame,   // quit from the pause menu
	ResetBestTime,
	RestartMission,
	ToggleDebugDraw,
	ToggleHelp,

	// --- Debug ---
	DebugTeleportToVehicle,

	Count
};

constexpr std::size_t GameActionCount = static_cast<std::size_t>(GameAction::Count);

/**
 * Canonical, stable identifier for an action as written in keymap.conf.
 * Returns "" for GameAction::Count / out-of-range values.
 */
constexpr std::string_view ToString(GameAction action)
{
	switch (action)
	{
	case GameAction::MoveForward: return "move_forward";
	case GameAction::MoveBackward: return "move_backward";
	case GameAction::MoveLeft: return "move_left";
	case GameAction::MoveRight: return "move_right";
	case GameAction::Interact: return "interact";
	case GameAction::Boost: return "boost";
	case GameAction::Honk: return "honk";
	case GameAction::VehicleJump: return "vehicle_jump";
	case GameAction::CameraLook: return "camera_look";
	case GameAction::FreeCamForward: return "freecam_forward";
	case GameAction::FreeCamBackward: return "freecam_backward";
	case GameAction::FreeCamLeft: return "freecam_left";
	case GameAction::FreeCamRight: return "freecam_right";
	case GameAction::FreeCamUp: return "freecam_up";
	case GameAction::FreeCamDown: return "freecam_down";
	case GameAction::FreeCamFast: return "freecam_fast";
	case GameAction::UIConfirm: return "ui_confirm";
	case GameAction::UIClick: return "ui_click";
	case GameAction::PauseToggle: return "pause_toggle";
	case GameAction::QuitGame: return "quit_game";
	case GameAction::ResetBestTime: return "reset_best_time";
	case GameAction::RestartMission: return "restart_mission";
	case GameAction::ToggleDebugDraw: return "toggle_debug_draw";
	case GameAction::ToggleHelp: return "toggle_help";
	case GameAction::DebugTeleportToVehicle: return "debug_teleport_to_vehicle";
	case GameAction::Count: return "";
	}
	return "";
}

/**
 * Parse an action identifier from keymap.conf. Returns false (and leaves
 * `out` untouched) if the name is unknown.
 */
constexpr bool TryParseAction(std::string_view name, GameAction& out)
{
	for (std::size_t i = 0; i < GameActionCount; ++i)
	{
		auto action = static_cast<GameAction>(i);
		if (ToString(action) == name)
		{
			out = action;
			return true;
		}
	}
	return false;
}

} // namespace Donut
