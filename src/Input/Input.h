// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <Input/Binding.h>
#include <Input/ButtonCodes.h>
#include <Input/GameAction.h>
#include <SDL.h>
#include <map>
#include <memory>
#include <string>

namespace Donut
{
class ITextEntryEventHandler
{

public:
	virtual ~ITextEntryEventHandler() {}

	virtual void Call(const std::string&) = 0;
};

template <class Instance>
class TextEntryEventHandler: public ITextEntryEventHandler
{

private:
	Instance* instance;
	typedef void (Instance::*Callback)(const std::string&);
	Callback callback;

public:
	TextEntryEventHandler(Instance* instance, Callback callback): instance(instance), callback(callback) {}

	void Call(const std::string& text) { (instance->*callback)(text); }
};

class Input
{

public:
	Input() = delete;
	~Input() = delete;

	static void PreEvent();
	static void ResetMouseDelta();
	static void HandleEvent(const SDL_Event& e);

	static bool IsDown(Button button);
	static bool JustPressed(Button button);
	static bool JustReleased(Button button);

	// --- high-level action queries (resolved through the active keymap) ---
	// Install the binding table (e.g. loaded from keymap.conf). Until set, a
	// built-in default keymap is used.
	static void SetKeymap(const Keymap& keymap);
	static const Keymap& GetKeymap();
	// Load (or create) keymap.conf at `path` and install it.
	static void LoadKeymap(const std::string& path);
	// Write the active keymap back to `path`.
	static bool SaveKeymap(const std::string& path);

	// true while any input bound to the action is held
	static bool IsActionDown(GameAction action);
	// rising/falling edge this frame (digital sources: keys, mouse, pad buttons)
	static bool JustPressed(GameAction action);
	static bool JustReleased(GameAction action);
	// 0..1 magnitude of the action this frame (digital = 0/1, analog axes scaled
	// past their dead zone); the strongest bound source wins
	static float GetActionAxis(GameAction action);

	// --- interactive rebinding (used by the in-game controls UI) ---
	// start listening for the next physical input
	static void BeginCapture();
	static void CancelCapture();
	static bool IsCapturing();
	// returns true once when an input was captured, writing it to `out`
	static bool ConsumeCapturedInput(InputSource& out);
	// replace an action's bindings with a single source, and reset to defaults
	static void RebindAction(GameAction action, const InputSource& src);
	static void ResetKeymapToDefault();

	static float GetMouseDeltaX();
	static float GetMouseDeltaY();
	static int GetMouseX();
	static int GetMouseY();

	static bool IsGamepadConnected();

	static void InitGamepad();
	static void CloseGamepad();

	template <class Instance, typename Callback>
	static void CaptureTextEntry(Instance* instance, Callback callback)
	{
		if (TextEntry == nullptr)
		{
			SDL_StartTextInput();
		}

		TextEntry = std::make_unique<TextEntryEventHandler<Instance>>(instance, callback);
	}

	static void ReleaseTextEntry();

private:
	struct ButtonState
	{
		ButtonState(): Pressed(false), Released(false), Down(false) {};

		bool Pressed;
		bool Released;
		bool Down;
	};

	static void UpdateButton(Button button, bool down);
	static void UpdateGamepadButton(Uint8 sdlButton, bool down);

	// resolution helpers for a single bound input
	static bool SourceDown(const InputSource& src);
	static bool SourcePressed(const InputSource& src);
	static bool SourceReleased(const InputSource& src);
	static float SourceMagnitude(const InputSource& src);

	// returns true if `e` was consumed as a captured input (or cancelled capture)
	static bool TryCaptureEvent(const SDL_Event& e);

	static Keymap ActiveKeymap;
	static bool Capturing;
	static bool HasCaptured;
	static InputSource CapturedSource;

	static float MouseDeltaX;
	static float MouseDeltaY;
	static int MouseX;
	static int MouseY;

	static SDL_GameController* Gamepad;
	// raw normalised SDL axis values (down/right positive), indexed by SDL_GameControllerAxis
	static float GamepadAxes[SDL_CONTROLLER_AXIS_MAX];
	// digital gamepad button states, indexed by SDL_GameControllerButton
	static ButtonState GamepadButtonStates[SDL_CONTROLLER_BUTTON_MAX];

	static Button KeyCodeToButtonCode(SDL_Keycode key);
	static std::map<SDL_Keycode, Button> KeyCodeToButtonCodeMap;
	static ButtonState ButtonStates[to_underlying(Button::Count)];

	static std::unique_ptr<ITextEntryEventHandler> TextEntry;
};
} // namespace Donut
