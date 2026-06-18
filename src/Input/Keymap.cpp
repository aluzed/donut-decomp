// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Input/Keymap.h>

#include <Core/Log.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Donut
{
namespace KeymapConfig
{

namespace
{

std::string Trim(const std::string& s)
{
	const auto first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";
	const auto last = s.find_last_not_of(" \t\r\n");
	return s.substr(first, last - first + 1);
}

std::string ToLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

// token -> keyboard/mouse Button (named keys only; single chars handled separately)
const std::unordered_map<std::string, Button>& KeyTokens()
{
	static const std::unordered_map<std::string, Button> table = {
	    {"up", Button::KeyUp},         {"down", Button::KeyDown},
	    {"left", Button::KeyLeft},     {"right", Button::KeyRight},
	    {"space", Button::KeySPACE},   {"enter", Button::KeyENTER},
	    {"escape", Button::KeyESCAPE}, {"tab", Button::KeyTAB},
	    {"backspace", Button::KeyBACKSPACE},
	    {"lshift", Button::KeyLSHIFT}, {"rshift", Button::KeyRSHIFT},
	    {"lctrl", Button::KeyLCONTROL}, {"rctrl", Button::KeyRCONTROL},
	    {"lalt", Button::KeyLALT},     {"ralt", Button::KeyRALT},
	    {"mouse_left", Button::MouseLeft}, {"mouse_right", Button::MouseRight},
	    {"mouse_middle", Button::MouseMiddle},
	    {"f1", Button::F1}, {"f2", Button::F2}, {"f3", Button::F3}, {"f4", Button::F4},
	    {"f5", Button::F5}, {"f6", Button::F6}, {"f7", Button::F7}, {"f8", Button::F8},
	    {"f9", Button::F9}, {"f10", Button::F10}, {"f11", Button::F11}, {"f12", Button::F12},
	};
	return table;
}

const std::unordered_map<std::string, SDL_GameControllerButton>& PadButtonTokens()
{
	static const std::unordered_map<std::string, SDL_GameControllerButton> table = {
	    {"pad_a", SDL_CONTROLLER_BUTTON_A},
	    {"pad_b", SDL_CONTROLLER_BUTTON_B},
	    {"pad_x", SDL_CONTROLLER_BUTTON_X},
	    {"pad_y", SDL_CONTROLLER_BUTTON_Y},
	    {"pad_lb", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
	    {"pad_rb", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
	    {"pad_ls", SDL_CONTROLLER_BUTTON_LEFTSTICK},
	    {"pad_rs", SDL_CONTROLLER_BUTTON_RIGHTSTICK},
	    {"pad_back", SDL_CONTROLLER_BUTTON_BACK},
	    {"pad_start", SDL_CONTROLLER_BUTTON_START},
	    {"pad_guide", SDL_CONTROLLER_BUTTON_GUIDE},
	    {"pad_dpup", SDL_CONTROLLER_BUTTON_DPAD_UP},
	    {"pad_dpdown", SDL_CONTROLLER_BUTTON_DPAD_DOWN},
	    {"pad_dpleft", SDL_CONTROLLER_BUTTON_DPAD_LEFT},
	    {"pad_dpright", SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
	};
	return table;
}

// stick-axis token base (without the +/- sign) -> SDL axis
const std::unordered_map<std::string, SDL_GameControllerAxis>& PadAxisTokens()
{
	static const std::unordered_map<std::string, SDL_GameControllerAxis> table = {
	    {"pad_leftx", SDL_CONTROLLER_AXIS_LEFTX},
	    {"pad_lefty", SDL_CONTROLLER_AXIS_LEFTY},
	    {"pad_rightx", SDL_CONTROLLER_AXIS_RIGHTX},
	    {"pad_righty", SDL_CONTROLLER_AXIS_RIGHTY},
	};
	return table;
}

constexpr float kDefaultDeadzone = 0.25f;

// Fill `km` from config `text`. `seen[i]` is set for every action that appears
// (even with an empty binding list). Does not touch unseen actions.
void ParseText(const std::string& text, Keymap& km, std::array<bool, GameActionCount>& seen)
{
	std::istringstream stream(text);
	std::string line;
	int lineNo = 0;
	while (std::getline(stream, line))
	{
		++lineNo;
		// strip comments
		const auto hash = line.find('#');
		if (hash != std::string::npos)
			line = line.substr(0, hash);
		line = Trim(line);
		if (line.empty())
			continue;

		const auto eq = line.find('=');
		if (eq == std::string::npos)
		{
			Log::Warn("keymap: line {}: missing '=', ignored: '{}'", lineNo, line);
			continue;
		}

		const std::string actionName = ToLower(Trim(line.substr(0, eq)));
		GameAction action;
		if (!TryParseAction(actionName, action))
		{
			Log::Warn("keymap: line {}: unknown action '{}'", lineNo, actionName);
			continue;
		}

		seen[static_cast<std::size_t>(action)] = true;
		auto& bindings = BindingsFor(km, action);

		std::stringstream rhs(line.substr(eq + 1));
		std::string token;
		while (std::getline(rhs, token, ','))
		{
			token = Trim(token);
			if (token.empty())
				continue;
			InputSource src;
			if (ParseInputToken(token, src))
				bindings.push_back(src);
			else
				Log::Warn("keymap: line {}: unknown input '{}' for action '{}'", lineNo, token, actionName);
		}
	}
}

} // namespace

bool ParseInputToken(const std::string& rawToken, InputSource& out)
{
	std::string token = ToLower(Trim(rawToken));
	if (token.empty())
		return false;

	// optional ":deadzone" suffix (only meaningful for axes)
	float deadzone = kDefaultDeadzone;
	const auto colon = token.find(':');
	if (colon != std::string::npos)
	{
		try
		{
			deadzone = std::stof(token.substr(colon + 1));
		}
		catch (...)
		{
			return false;
		}
		deadzone = std::clamp(deadzone, 0.0f, 1.0f);
		token = token.substr(0, colon);
	}

	// gamepad?
	if (token.rfind("pad_", 0) == 0)
	{
		// triggers
		if (token == "pad_lt")
		{
			out = InputSource::Axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1.0f, deadzone);
			return true;
		}
		if (token == "pad_rt")
		{
			out = InputSource::Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1.0f, deadzone);
			return true;
		}

		// stick axis with trailing +/-
		const char sign = token.back();
		if (sign == '+' || sign == '-')
		{
			const std::string base = token.substr(0, token.size() - 1);
			const auto it = PadAxisTokens().find(base);
			if (it != PadAxisTokens().end())
			{
				out = InputSource::Axis(it->second, sign == '-' ? -1.0f : 1.0f, deadzone);
				return true;
			}
			return false;
		}

		// gamepad button
		const auto it = PadButtonTokens().find(token);
		if (it != PadButtonTokens().end())
		{
			out = InputSource::Pad(it->second);
			return true;
		}
		return false;
	}

	// single character: letter or digit
	if (token.size() == 1)
	{
		const char c = token[0];
		if (c >= 'a' && c <= 'z')
		{
			out = InputSource::Key(static_cast<Button>(to_underlying(Button::KeyA) + (c - 'a')));
			return true;
		}
		if (c >= '0' && c <= '9')
		{
			out = InputSource::Key(static_cast<Button>(to_underlying(Button::Key0) + (c - '0')));
			return true;
		}
	}

	// named key / mouse
	const auto it = KeyTokens().find(token);
	if (it != KeyTokens().end())
	{
		out = InputSource::Key(it->second);
		return true;
	}

	return false;
}

std::string ToToken(const InputSource& src)
{
	switch (src.kind)
	{
	case InputSourceKind::Key:
	{
		const auto u = to_underlying(src.button);
		if (src.button >= Button::KeyA && src.button <= Button::KeyZ)
			return std::string(1, static_cast<char>('a' + (u - to_underlying(Button::KeyA))));
		if (src.button >= Button::Key0 && src.button <= Button::Key9)
			return std::string(1, static_cast<char>('0' + (u - to_underlying(Button::Key0))));
		switch (src.button)
		{
		case Button::KeyUp: return "up";
		case Button::KeyDown: return "down";
		case Button::KeyLeft: return "left";
		case Button::KeyRight: return "right";
		case Button::KeySPACE: return "space";
		case Button::KeyENTER: return "enter";
		case Button::KeyESCAPE: return "escape";
		case Button::KeyTAB: return "tab";
		case Button::KeyBACKSPACE: return "backspace";
		case Button::KeyLSHIFT: return "lshift";
		case Button::KeyRSHIFT: return "rshift";
		case Button::KeyLCONTROL: return "lctrl";
		case Button::KeyRCONTROL: return "rctrl";
		case Button::KeyLALT: return "lalt";
		case Button::KeyRALT: return "ralt";
		case Button::MouseLeft: return "mouse_left";
		case Button::MouseRight: return "mouse_right";
		case Button::MouseMiddle: return "mouse_middle";
		case Button::F1: return "f1"; case Button::F2: return "f2"; case Button::F3: return "f3";
		case Button::F4: return "f4"; case Button::F5: return "f5"; case Button::F6: return "f6";
		case Button::F7: return "f7"; case Button::F8: return "f8"; case Button::F9: return "f9";
		case Button::F10: return "f10"; case Button::F11: return "f11"; case Button::F12: return "f12";
		default: return "";
		}
	}
	case InputSourceKind::GamepadButton:
		for (const auto& kv : PadButtonTokens())
			if (kv.second == src.gpButton)
				return kv.first;
		return "";
	case InputSourceKind::GamepadAxis:
	{
		std::string base;
		if (src.gpAxis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
			base = "pad_lt";
		else if (src.gpAxis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
			base = "pad_rt";
		else
		{
			for (const auto& kv : PadAxisTokens())
				if (kv.second == src.gpAxis)
				{
					base = kv.first + (src.axisDir < 0.0f ? "-" : "+");
					break;
				}
		}
		if (base.empty())
			return "";
		if (std::abs(src.deadzone - kDefaultDeadzone) > 1e-3f)
			base += ":" + fmt::format("{:g}", src.deadzone);
		return base;
	}
	default: return "";
	}
}

std::string HumanLabel(const InputSource& src)
{
	switch (src.kind)
	{
	case InputSourceKind::Key:
	{
		switch (src.button)
		{
		case Button::KeyUp: return "Up"; case Button::KeyDown: return "Down";
		case Button::KeyLeft: return "Left"; case Button::KeyRight: return "Right";
		case Button::KeySPACE: return "Space"; case Button::KeyENTER: return "Enter";
		case Button::KeyESCAPE: return "Esc"; case Button::KeyLSHIFT: return "LShift";
		case Button::KeyLCONTROL: return "LCtrl"; case Button::MouseLeft: return "LMB";
		case Button::MouseRight: return "RMB"; case Button::MouseMiddle: return "MMB";
		default: break;
		}
		std::string t = ToToken(src);
		if (!t.empty())
			t[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(t[0])));
		return t;
	}
	case InputSourceKind::GamepadButton:
	{
		switch (src.gpButton)
		{
		case SDL_CONTROLLER_BUTTON_A: return "(A)";
		case SDL_CONTROLLER_BUTTON_B: return "(B)";
		case SDL_CONTROLLER_BUTTON_X: return "(X)";
		case SDL_CONTROLLER_BUTTON_Y: return "(Y)";
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
		case SDL_CONTROLLER_BUTTON_BACK: return "Back";
		case SDL_CONTROLLER_BUTTON_START: return "Start";
		default: return "Pad";
		}
	}
	case InputSourceKind::GamepadAxis:
	{
		switch (src.gpAxis)
		{
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "LT";
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "RT";
		case SDL_CONTROLLER_AXIS_LEFTX: return src.axisDir < 0 ? "LStick Left" : "LStick Right";
		case SDL_CONTROLLER_AXIS_LEFTY: return src.axisDir < 0 ? "LStick Up" : "LStick Down";
		case SDL_CONTROLLER_AXIS_RIGHTX: return src.axisDir < 0 ? "RStick Left" : "RStick Right";
		case SDL_CONTROLLER_AXIS_RIGHTY: return src.axisDir < 0 ? "RStick Up" : "RStick Down";
		default: return "Axis";
		}
	}
	default: return "—";
	}
}

std::string Serialize(const Keymap& keymap)
{
	std::string out = "# donut control bindings — see docs/keymap.md\n"
	                  "# action = input [, input ...]\n\n";
	for (std::size_t i = 0; i < GameActionCount; ++i)
	{
		const auto action = static_cast<GameAction>(i);
		out += std::string(ToString(action)) + " =";
		int emitted = 0;
		for (const auto& binding : keymap[i])
		{
			const std::string token = ToToken(binding);
			if (token.empty())
				continue;
			out += (emitted++ == 0 ? " " : ", ") + token;
		}
		out += "\n";
	}
	return out;
}

bool Save(const std::string& path, const Keymap& keymap)
{
	const std::string tmp = path + ".tmp";
	{
		std::ofstream file(tmp, std::ios::trunc);
		if (!file.is_open())
		{
			Log::Warn("keymap: cannot open '{}' for writing", tmp);
			return false;
		}
		file << Serialize(keymap);
		if (!file.good())
		{
			Log::Warn("keymap: write to '{}' failed", tmp);
			return false;
		}
	}
	std::remove(path.c_str());
	if (std::rename(tmp.c_str(), path.c_str()) != 0)
	{
		Log::Warn("keymap: could not replace '{}'", path);
		return false;
	}
	Log::Info("keymap: saved bindings to '{}'", path);
	return true;
}

const char* DefaultFileContents()
{
	return R"(# donut control bindings — see docs/keymap.md
# action = input [, input ...]

# --- movement (on foot + driving) ---
move_forward  = up,    pad_lefty-, pad_rt
move_backward = down,  pad_lefty+, pad_lt
move_left     = left,  pad_leftx-
move_right    = right, pad_leftx+

# --- character / vehicle ---
interact      = e,      pad_a
boost         = lshift, pad_rb
honk          = h,      pad_x
vehicle_jump  = space,  pad_b

# --- free (debug) camera ---
camera_look      = mouse_right, pad_rs
freecam_forward  = w
freecam_backward = s
freecam_left     = a
freecam_right    = d
freecam_up       = space
freecam_down     = lctrl
freecam_fast     = lshift

# --- ui / global ---
ui_confirm        = space, enter, pad_a
ui_click          = mouse_left
pause_toggle      = escape, pad_start
quit_game         = q, pad_back
reset_best_time   = r
restart_mission   = m
toggle_debug_draw = 1
toggle_help       = f

# --- debug ---
debug_teleport_to_vehicle = t
)";
}

Keymap MakeDefault()
{
	Keymap km;
	std::array<bool, GameActionCount> seen{};
	ParseText(DefaultFileContents(), km, seen);
	return km;
}

bool Load(const std::string& path, Keymap& out)
{
	std::ifstream file(path);
	if (!file.is_open())
		return false;

	std::stringstream buffer;
	buffer << file.rdbuf();

	out = Keymap{};
	std::array<bool, GameActionCount> seen{};
	ParseText(buffer.str(), out, seen);

	// any action the file never mentioned keeps its built-in default
	const Keymap defaults = MakeDefault();
	int filled = 0;
	for (std::size_t i = 0; i < GameActionCount; ++i)
	{
		if (!seen[i])
		{
			out[i] = defaults[i];
			++filled;
		}
	}
	if (filled > 0)
		Log::Info("keymap: {} action(s) absent from '{}', using defaults", filled, path);

	return true;
}

bool LoadOrCreate(const std::string& path, Keymap& out, bool* created)
{
	if (created)
		*created = false;

	std::ifstream probe(path);
	if (probe.is_open())
	{
		probe.close();
		return Load(path, out);
	}

	// missing: write the default file so it can be edited, and use defaults
	out = MakeDefault();
	std::ofstream file(path, std::ios::trunc);
	if (file.is_open())
	{
		file << DefaultFileContents();
		if (created)
			*created = true;
		Log::Info("keymap: '{}' not found, wrote default bindings", path);
	}
	else
	{
		Log::Warn("keymap: '{}' not found and could not be created; using defaults in memory", path);
	}
	return true;
}

} // namespace KeymapConfig
} // namespace Donut
