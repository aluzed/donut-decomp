// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Input/Input.h>

#include <Input/Keymap.h>

#include "Core/Log.h"

#include <algorithm>
#include <cmath>

namespace Donut
{
std::map<SDL_Keycode, Button> Input::KeyCodeToButtonCodeMap = {
    {SDLK_UNKNOWN, Button::None},
    {SDLK_0, Button::Key0},
    {SDLK_1, Button::Key1},
    {SDLK_2, Button::Key2},
    {SDLK_3, Button::Key3},
    {SDLK_4, Button::Key4},
    {SDLK_5, Button::Key5},
    {SDLK_6, Button::Key6},
    {SDLK_7, Button::Key7},
    {SDLK_8, Button::Key8},
    {SDLK_9, Button::Key9},

    {SDLK_a, Button::KeyA},
    {SDLK_b, Button::KeyB},
    {SDLK_c, Button::KeyC},
    {SDLK_d, Button::KeyD},
    {SDLK_e, Button::KeyE},
    {SDLK_f, Button::KeyF},
    {SDLK_g, Button::KeyG},
    {SDLK_h, Button::KeyH},
    {SDLK_i, Button::KeyI},
    {SDLK_j, Button::KeyJ},
    {SDLK_k, Button::KeyK},
    {SDLK_l, Button::KeyL},
    {SDLK_m, Button::KeyM},
    {SDLK_n, Button::KeyN},
    {SDLK_o, Button::KeyO},
    {SDLK_p, Button::KeyP},
    {SDLK_q, Button::KeyQ},
    {SDLK_r, Button::KeyR},
    {SDLK_s, Button::KeyS},
    {SDLK_t, Button::KeyT},
    {SDLK_u, Button::KeyU},
    {SDLK_v, Button::KeyV},
    {SDLK_w, Button::KeyW},
    {SDLK_x, Button::KeyX},
    {SDLK_y, Button::KeyY},
    {SDLK_z, Button::KeyZ},

    {SDLK_KP_0, Button::KeyPad0},
    {SDLK_KP_1, Button::KeyPad1},
    {SDLK_KP_2, Button::KeyPad2},
    {SDLK_KP_3, Button::KeyPad3},
    {SDLK_KP_4, Button::KeyPad4},
    {SDLK_KP_5, Button::KeyPad5},
    {SDLK_KP_6, Button::KeyPad6},
    {SDLK_KP_7, Button::KeyPad7},
    {SDLK_KP_8, Button::KeyPad8},
    {SDLK_KP_9, Button::KeyPad9},

    {SDLK_KP_DIVIDE, Button::KeyPadDIVIDE},
    {SDLK_KP_MULTIPLY, Button::KeyPadMULTIPLY},
    {SDLK_KP_MINUS, Button::KeyPadMINUS},
    {SDLK_KP_PLUS, Button::KeyPadPLUS},
    {SDLK_KP_ENTER, Button::KeyPadENTER},
    {SDLK_KP_DECIMAL, Button::KeyPadDECIMAL},

    {SDLK_LEFTBRACKET, Button::KeyLBRACKET},
    {SDLK_RIGHTBRACKET, Button::KeyRBRACKET},
    {SDLK_SEMICOLON, Button::KeySEMICOLON},
    {SDLK_QUOTE, Button::KeyAPOSTROPHE},
    {SDLK_BACKQUOTE, Button::KeyBACKQUOTE},
    {SDLK_COMMA, Button::KeyCOMMA},
    {SDLK_PERIOD, Button::KeyPERIOD},
    {SDLK_SLASH, Button::KeySLASH},
    {SDLK_BACKSLASH, Button::KeyBACKSLASH},
    {SDLK_MINUS, Button::KeyMINUS},
    {SDLK_EQUALS, Button::KeyEQUAL},
    {SDLK_RETURN, Button::KeyENTER},
    {SDLK_SPACE, Button::KeySPACE},
    {SDLK_BACKSPACE, Button::KeyBACKSPACE},
    {SDLK_TAB, Button::KeyTAB},
    {SDLK_CAPSLOCK, Button::KeyCAPSLOCK},
    {SDLK_NUMLOCKCLEAR, Button::KeyNUMLOCK},
    {SDLK_ESCAPE, Button::KeyESCAPE},
    {SDLK_SCROLLLOCK, Button::KeySCROLLLOCK},
    {SDLK_INSERT, Button::KeyINSERT},
    {SDLK_DELETE, Button::KeyDELETE},
    {SDLK_HOME, Button::KeyHOME},
    {SDLK_END, Button::KeyEND},
    {SDLK_PAGEUP, Button::KeyPAGEUP},
    {SDLK_PAGEDOWN, Button::KeyPAGEDOWN},
    {SDLK_PAUSE, Button::KeyPAUSE},
    {SDLK_LSHIFT, Button::KeyLSHIFT},
    {SDLK_RSHIFT, Button::KeyRSHIFT},
    {SDLK_LALT, Button::KeyLALT},
    {SDLK_RALT, Button::KeyRALT},
    {SDLK_LCTRL, Button::KeyLCONTROL},
    {SDLK_RCTRL, Button::KeyRCONTROL},
    {SDLK_LGUI, Button::KeyLGUI},
    {SDLK_RGUI, Button::KeyRGUI},
    {SDLK_APPLICATION, Button::KeyAPP},

    {SDLK_UP, Button::KeyUp},
    {SDLK_LEFT, Button::KeyLeft},
    {SDLK_DOWN, Button::KeyDown},
    {SDLK_RIGHT, Button::KeyRight},

    {SDLK_F1, Button::F1},
    {SDLK_F2, Button::F2},
    {SDLK_F3, Button::F3},
    {SDLK_F4, Button::F4},
    {SDLK_F5, Button::F5},
    {SDLK_F6, Button::F6},
    {SDLK_F7, Button::F7},
    {SDLK_F8, Button::F8},
    {SDLK_F9, Button::F9},
    {SDLK_F10, Button::F10},
    {SDLK_F11, Button::F11},
    {SDLK_F12, Button::F12},

    {SDLK_PRINTSCREEN, Button::PrintScreen},
};

Input::ButtonState Input::ButtonStates[to_underlying(Button::Count)] = {ButtonState()};
float Input::MouseDeltaX = 0.0f;
float Input::MouseDeltaY = 0.0f;
int Input::MouseX = 0;
int Input::MouseY = 0;
SDL_GameController* Input::Gamepad = nullptr;
float Input::GamepadAxes[SDL_CONTROLLER_AXIS_MAX] = {};
Input::ButtonState Input::GamepadButtonStates[SDL_CONTROLLER_BUTTON_MAX] = {ButtonState()};
Keymap Input::ActiveKeymap = KeymapConfig::MakeDefault();
bool Input::Capturing = false;
bool Input::HasCaptured = false;
InputSource Input::CapturedSource = InputSource();
std::unique_ptr<ITextEntryEventHandler> Input::TextEntry = nullptr;

Button Input::KeyCodeToButtonCode(SDL_Keycode key)
{
	auto it = KeyCodeToButtonCodeMap.find(key);
	return (it != KeyCodeToButtonCodeMap.end()) ? it->second : Button::None;
}

void Input::UpdateButton(Button button, bool down)
{
	auto& buttonState = ButtonStates[to_underlying(button)];
	buttonState.Pressed = !buttonState.Down && down;
	buttonState.Released = buttonState.Down && !down;
	buttonState.Down = down;
}

void Input::UpdateGamepadButton(Uint8 sdlButton, bool down)
{
	if (sdlButton >= SDL_CONTROLLER_BUTTON_MAX)
		return;
	auto& buttonState = GamepadButtonStates[sdlButton];
	buttonState.Pressed = !buttonState.Down && down;
	buttonState.Released = buttonState.Down && !down;
	buttonState.Down = down;
}

void Input::ResetMouseDelta()
{
	MouseDeltaX = 0.0f;
	MouseDeltaY = 0.0f;
}

void Input::PreEvent()
{
	ResetMouseDelta();

	for (size_t i = 0; i < to_underlying(Button::Count); ++i)
	{
		auto& buttonState = ButtonStates[i];
		buttonState.Pressed = false;
		buttonState.Released = false;
	}

	for (auto& buttonState : GamepadButtonStates)
	{
		buttonState.Pressed = false;
		buttonState.Released = false;
	}
}

void Input::HandleEvent(const SDL_Event& e)
{
	// when rebinding, the first physical input is captured instead of being
	// fed to the game (so it doesn't also fire whatever it's currently bound to)
	if (Capturing && TryCaptureEvent(e))
		return;

	if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
	{
		UpdateButton(KeyCodeToButtonCode(e.key.keysym.sym), e.key.state == SDL_PRESSED);
	}
	else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
	{
		UpdateButton((Button)(e.button.button), (e.button.state == SDL_PRESSED));
		MouseX = e.button.x;
		MouseY = e.button.y;
	}
	else if (e.type == SDL_MOUSEMOTION)
	{
		MouseDeltaX += (float)e.motion.xrel;
		MouseDeltaY += (float)e.motion.yrel;
		MouseX = e.motion.x;
		MouseY = e.motion.y;
	}
	else if (e.type == SDL_CONTROLLERAXISMOTION)
	{
		float val = static_cast<float>(e.caxis.value) / 32767.0f;
		if (fabs(val) < 0.1f) val = 0.0f;
		// raw SDL value (down/right positive) for keymap axis resolution
		if (e.caxis.axis < SDL_CONTROLLER_AXIS_MAX)
			GamepadAxes[e.caxis.axis] = val;
	}
	else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
	{
		// gamepad buttons resolve through the keymap (pad_a, pad_b, ...)
		UpdateGamepadButton(e.cbutton.button, e.cbutton.state == SDL_PRESSED);
	}
	else if (e.type == SDL_CONTROLLERDEVICEADDED)
	{
		if (!Gamepad)
		{
			Gamepad = SDL_GameControllerOpen(e.cdevice.which);
			Log::Info("Input: gamepad connected");
		}
	}
	else if (e.type == SDL_CONTROLLERDEVICEREMOVED)
	{
		if (Gamepad && e.cdevice.which == SDL_JoystickInstanceID(
			SDL_GameControllerGetJoystick(Gamepad)))
		{
			SDL_GameControllerClose(Gamepad);
			Gamepad = nullptr;
			// drop any held analog/button state so nothing stays stuck on
			for (auto& v : GamepadAxes) v = 0.0f;
			for (auto& b : GamepadButtonStates) b = ButtonState();
			Log::Info("Input: gamepad disconnected");
		}
	}
	else if (e.type == SDL_TEXTINPUT)
	{
		if (TextEntry != nullptr)
		{
			TextEntry->Call(e.text.text);
		}
	}
}

float Input::GetMouseDeltaX()
{
	return MouseDeltaX;
}

float Input::GetMouseDeltaY()
{
	return MouseDeltaY;
}

int Input::GetMouseX()
{
	return MouseX;
}

int Input::GetMouseY()
{
	return MouseY;
}

bool Input::IsDown(Button button)
{
	return ButtonStates[to_underlying(button)].Down;
}

bool Input::JustPressed(Button button)
{
	return ButtonStates[to_underlying(button)].Pressed;
}

bool Input::JustReleased(Button button)
{
	return ButtonStates[to_underlying(button)].Released;
}

void Input::ReleaseTextEntry()
{
	if (TextEntry != nullptr)
	{
		TextEntry = nullptr;
		SDL_StopTextInput();
	}
}

// --- keymap / action layer ----------------------------------------------

void Input::SetKeymap(const Keymap& keymap) { ActiveKeymap = keymap; }
const Keymap& Input::GetKeymap() { return ActiveKeymap; }

void Input::LoadKeymap(const std::string& path)
{
	Keymap km;
	KeymapConfig::LoadOrCreate(path, km);
	ActiveKeymap = km;
}

bool Input::SaveKeymap(const std::string& path)
{
	return KeymapConfig::Save(path, ActiveKeymap);
}

void Input::BeginCapture()
{
	Capturing = true;
	HasCaptured = false;
}

void Input::CancelCapture()
{
	Capturing = false;
	HasCaptured = false;
}

bool Input::IsCapturing() { return Capturing; }

bool Input::ConsumeCapturedInput(InputSource& out)
{
	if (!HasCaptured)
		return false;
	out = CapturedSource;
	HasCaptured = false;
	return true;
}

bool Input::TryCaptureEvent(const SDL_Event& e)
{
	switch (e.type)
	{
	case SDL_KEYDOWN:
	{
		// Escape cancels the capture rather than binding itself
		if (e.key.keysym.sym == SDLK_ESCAPE)
		{
			CancelCapture();
			return true;
		}
		Button b = KeyCodeToButtonCode(e.key.keysym.sym);
		if (b == Button::None)
			return true; // swallow unmapped keys while capturing
		CapturedSource = InputSource::Key(b);
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
		CapturedSource = InputSource::Key(static_cast<Button>(e.button.button));
		break;
	case SDL_CONTROLLERBUTTONDOWN:
		if (e.cbutton.button >= SDL_CONTROLLER_BUTTON_MAX)
			return true;
		CapturedSource = InputSource::Pad(static_cast<SDL_GameControllerButton>(e.cbutton.button));
		break;
	case SDL_CONTROLLERAXISMOTION:
	{
		float val = static_cast<float>(e.caxis.value) / 32767.0f;
		if (std::fabs(val) < 0.6f) // require a firm push so idle drift isn't captured
			return false;          // not enough: let the event pass, keep capturing
		CapturedSource = InputSource::Axis(static_cast<SDL_GameControllerAxis>(e.caxis.axis),
		                                   val < 0.0f ? -1.0f : 1.0f);
		break;
	}
	default:
		return false; // not an input we capture; let it through
	}

	HasCaptured = true;
	Capturing = false;
	return true;
}

void Input::RebindAction(GameAction action, const InputSource& src)
{
	auto& bindings = BindingsFor(ActiveKeymap, action);
	bindings.clear();
	bindings.push_back(src);
}

void Input::ResetKeymapToDefault()
{
	ActiveKeymap = KeymapConfig::MakeDefault();
}

float Input::SourceMagnitude(const InputSource& src)
{
	switch (src.kind)
	{
	case InputSourceKind::Key:
		return ButtonStates[to_underlying(src.button)].Down ? 1.0f : 0.0f;
	case InputSourceKind::GamepadButton:
		if (src.gpButton < 0 || src.gpButton >= SDL_CONTROLLER_BUTTON_MAX)
			return 0.0f;
		return GamepadButtonStates[src.gpButton].Down ? 1.0f : 0.0f;
	case InputSourceKind::GamepadAxis:
	{
		if (src.gpAxis < 0 || src.gpAxis >= SDL_CONTROLLER_AXIS_MAX)
			return 0.0f;
		// take the requested half of the axis
		float v = GamepadAxes[src.gpAxis] * src.axisDir;
		if (v <= src.deadzone)
			return 0.0f;
		const float denom = std::max(1e-4f, 1.0f - src.deadzone);
		return std::min(1.0f, (v - src.deadzone) / denom);
	}
	default: return 0.0f;
	}
}

bool Input::SourceDown(const InputSource& src)
{
	return SourceMagnitude(src) > 0.0f;
}

bool Input::SourcePressed(const InputSource& src)
{
	switch (src.kind)
	{
	case InputSourceKind::Key:
		return ButtonStates[to_underlying(src.button)].Pressed;
	case InputSourceKind::GamepadButton:
		if (src.gpButton < 0 || src.gpButton >= SDL_CONTROLLER_BUTTON_MAX)
			return false;
		return GamepadButtonStates[src.gpButton].Pressed;
	default:
		// analog axes have no meaningful press edge
		return false;
	}
}

bool Input::SourceReleased(const InputSource& src)
{
	switch (src.kind)
	{
	case InputSourceKind::Key:
		return ButtonStates[to_underlying(src.button)].Released;
	case InputSourceKind::GamepadButton:
		if (src.gpButton < 0 || src.gpButton >= SDL_CONTROLLER_BUTTON_MAX)
			return false;
		return GamepadButtonStates[src.gpButton].Released;
	default:
		return false;
	}
}

bool Input::IsActionDown(GameAction action)
{
	for (const auto& src : BindingsFor(ActiveKeymap, action))
		if (SourceDown(src))
			return true;
	return false;
}

bool Input::JustPressed(GameAction action)
{
	for (const auto& src : BindingsFor(ActiveKeymap, action))
		if (SourcePressed(src))
			return true;
	return false;
}

bool Input::JustReleased(GameAction action)
{
	for (const auto& src : BindingsFor(ActiveKeymap, action))
		if (SourceReleased(src))
			return true;
	return false;
}

float Input::GetActionAxis(GameAction action)
{
	float magnitude = 0.0f;
	for (const auto& src : BindingsFor(ActiveKeymap, action))
		magnitude = std::max(magnitude, SourceMagnitude(src));
	return magnitude;
}
bool Input::IsGamepadConnected() { return Gamepad != nullptr; }

void Input::InitGamepad()
{
	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
		{
			Gamepad = SDL_GameControllerOpen(i);
			Log::Info("Input: gamepad detected");
			break;
		}
	}
}

void Input::CloseGamepad()
{
	if (Gamepad)
	{
		SDL_GameControllerClose(Gamepad);
		Gamepad = nullptr;
	}
}
} // namespace Donut
