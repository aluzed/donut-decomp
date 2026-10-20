#include "OptionsMenu.h"

#include "Audio/AudioManager.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Game/GameInput.h"
#include "Input/Input.h"
#include "Input/Keymap.h"
#include "UI/TextBible.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Donut
{

namespace
{
constexpr const char* kFrontend = "./art/frontend/scrooby2/frontend.p3d";
constexpr const char* kSettings = "donut_settings.dat";
// A step of a slider, left or right.
constexpr float kStep = 0.1f;

// The sound page's sliders, top to bottom, and the sound each sets.
struct Slider
{
	const char* name;
	const char* key; // in donut_settings.dat
	AudioManager::Channel channel;
};
constexpr Slider kSliders[] = {
    {"Music", "music", AudioManager::Channel::Music},
    {"Effects", "effects", AudioManager::Channel::Effects},
    {"Engine", "vehicle", AudioManager::Channel::Engine},
    {"Voice", "voice", AudioManager::Channel::Voice},
};

// The keyboard page's rows, top to bottom, and the action each sets; Count:
// none here (first person, attack, the camera's zoom, resetting the car), and
// jumping is the action key's. Moving and driving are the same actions.
constexpr GameAction kKeyRows[] = {
    GameAction::MoveForward, GameAction::MoveBackward, GameAction::MoveLeft,     GameAction::MoveRight,
    GameAction::Count,       GameAction::Count,        GameAction::Count,        GameAction::Boost,
    GameAction::Interact,    GameAction::Count,        GameAction::MoveForward,  GameAction::MoveBackward,
    GameAction::Honk,        GameAction::Count,
};
constexpr int kKeyRowCount = static_cast<int>(std::size(kKeyRows));

// An action's keys (keyboard and mouse), in the order bound.
std::vector<InputSource> keysOf(GameAction action)
{
	std::vector<InputSource> keys;
	for (const InputSource& source : Input::GetKeymap()[static_cast<std::size_t>(action)])
		if (source.kind == InputSourceKind::Key)
			keys.push_back(source);
	return keys;
}

bool inside(const FrontendProject& page, const std::string& name, int mouseX, int mouseY, float* along = nullptr)
{
	float x, y, w, h;
	if (!page.GetRect(name, x, y, w, h) || mouseX < x || mouseX > x + w || mouseY < y || mouseY > y + h)
		return false;
	if (along)
		*along = w > 0.0f ? (mouseX - x) / w : 0.0f;
	return true;
}
} // namespace

OptionsMenu::OptionsMenu(AudioManager& audio, SDL_Window* window, std::function<void()> viewCredits)
    : _audio(audio), _window(window), _viewCredits(std::move(viewCredits))
{
	load();
	applyDisplay();
	// The screen's sizes, each once (it lists them by refresh rate too), from
	// the game's own 640 x 480 up.
	const int display = _window ? std::max(0, SDL_GetWindowDisplayIndex(_window)) : 0;
	for (int i = SDL_GetNumDisplayModes(display) - 1; i >= 0; --i)
	{
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(display, i, &mode) == 0 && mode.w >= 640 && mode.h >= 480 &&
		    std::find(_sizes.begin(), _sizes.end(), std::make_pair(mode.w, mode.h)) == _sizes.end())
			_sizes.emplace_back(mode.w, mode.h);
	}
	std::sort(_sizes.begin(), _sizes.end());
	if (!FileSystem::exists(kFrontend))
		return;
	_options = std::make_unique<FrontendProject>();
	_options->LoadP3D(kFrontend, "Options.pag");
	// The console's display mode (interlaced or progressive), on the page
	// under the PC's Display entry.
	_options->SetText("DisplayMode", " ");
	_options->SetText("DisplayMode_Value", " ");
	_options->SetSpriteVisible("DisplayMode_ArrowL", false);
	_options->SetSpriteVisible("DisplayMode_ArrowR", false);
	_sound = std::make_unique<FrontendProject>();
	_sound->LoadP3D(kFrontend, "Sound.pag");
	// Each slider at its box's size: its bar across, its icon the knob on it.
	for (const Slider& slider : kSliders)
		for (const char* part : {"_SliderBgd", "_Slider", "_Icon", "_Off"})
		{
			const std::string name = std::string(slider.name) + part;
			float x, y, w, h;
			if (_sound->GetSpriteBox(name, x, y, w, h))
				_sound->SetSpriteBox(name, x, y, w, h);
			if (part[1] == 'I' || part[1] == 'O')
				_sound->BringSpriteToFront(name);
		}
	_display = std::make_unique<FrontendProject>();
	_display->LoadP3D(kFrontend, "Display.pag");
	// Its colours: the screen's, which the game does not choose.
	_display->SetText("ColourDepth_Value", "32-bit");
	_display->SetSpriteVisible("ColourDepth_ArrowL", false);
	_display->SetSpriteVisible("ColourDepth_ArrowR", false);
	// Its arrows at their boxes' size, not their images' twice that.
	for (const char* arrow : {"Resolution_ArrowL", "Resolution_ArrowR", "DisplayMode_ArrowL", "DisplayMode_ArrowR"})
	{
		float x, y, w, h;
		if (_display->GetSpriteBox(arrow, x, y, w, h))
			_display->SetSpriteBox(arrow, x, y, w, h);
	}
	_keys = std::make_unique<FrontendProject>();
	_keys->LoadP3D(kFrontend, "PCKeyboardConfig.pag");
	_keys->SetTextsFitHeight(true);
	const std::string& accept = Game::GetInstance().GetTextBible().Get("ACCEPT");
	_display->SetText("ApplyChanges", accept.empty() ? "Accept" : accept);
	// Stereo, which is what OpenAL is given.
	_sound->SetText("SurroundSound_Value", "STEREO SOUND");
	_sound->SetSpriteVisible("SurroundSound_LArrow", false);
	_sound->SetSpriteVisible("SurroundSound_RArrow", false);
}

OptionsMenu::~OptionsMenu() = default;

void OptionsMenu::Open(bool fromPause)
{
	if (!IsLoaded())
		return;
	_screen = Screen::Options;
	_fromPause = fromPause;
	_choice = 0;
	_shown = false;
	_mouseX = _mouseY = -1;
}

int OptionsMenu::pick(FrontendProject& page, const std::vector<Entry>& entries, const GameIntent& intent)
{
	const int count = static_cast<int>(entries.size());
	const auto step = [&](int from, int by) {
		int at = from;
		for (int i = 0; i < count; ++i)
		{
			at = (at + by + count) % count;
			if (entries[at].enabled)
				return at;
		}
		return from;
	};
	if (!entries[_choice].enabled)
		_choice = step(_choice, 1);
	const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
	if (up && !_upHeld)
		_choice = step(_choice, -1);
	if (down && !_downHeld)
		_choice = step(_choice, 1);
	_upHeld = up;
	_downHeld = down;
	// The mouse moved over an entry picks it; a click takes it.
	int clicked = -1;
	const bool moved = _mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY);
	for (int i = 0; i < count; ++i)
		if (entries[i].enabled && inside(page, entries[i].name, intent.mouseX, intent.mouseY))
		{
			if (moved)
				_choice = i;
			if (intent.mouseLeftClick)
				clicked = _choice = i;
		}
	_mouseX = intent.mouseX;
	_mouseY = intent.mouseY;
	colour(page, entries);
	return clicked;
}

void OptionsMenu::colour(FrontendProject& page, const std::vector<Entry>& entries)
{
	for (int i = 0; i < static_cast<int>(entries.size()); ++i)
	{
		if (!entries[i].enabled)
			page.SetTextColor(entries[i].name, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		else if (i == _choice)
			page.SetTextColor(entries[i].name, Vector4(1.0f, 0.84f, 0.0f, 1.0f));
		else
			page.ResetTextColor(entries[i].name);
	}
}

void OptionsMenu::Draw(const GameIntent& intent, const Matrix4x4& proj)
{
	if (_screen == Screen::Closed)
		return;
	const bool back = _shown && intent.pauseToggle;
	const bool confirm = _shown && intent.uiConfirm;
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	const bool leftPressed = left && !_leftHeld, rightPressed = right && !_rightHeld;
	_leftHeld = left;
	_rightHeld = right;

	if (_screen == Screen::Options)
	{
		const std::vector<Entry> entries = {{"Controller", true},
		                                    {"Sound", true},
		                                    {"ViewMovies", false},
		                                    {"ViewCredits", !_fromPause && _viewCredits != nullptr},
		                                    {"Display", _window != nullptr && !_sizes.empty()}};
		const int clicked = _shown ? pick(*_options, entries, intent) : -1;
		if (!_shown)
			colour(*_options, entries);
		_options->Draw(proj);
		_shown = true;
		if (back)
		{
			_screen = Screen::Closed;
			return;
		}
		if (!confirm && clicked < 0)
			return;
		const std::string chosen = entries[_choice].name;
		if (chosen == "Sound")
		{
			_screen = Screen::Sound;
			_choice = 0;
			_shown = false;
		}
		else if (chosen == "Controller")
		{
			_screen = Screen::Keys;
			_choice = 0;
			_column = 0;
			_shown = false;
		}
		else if (chosen == "Display")
		{
			// The page shows the window as it is.
			int width = 0, height = 0;
			SDL_GetWindowSize(_window, &width, &height);
			_fullScreen = (SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN) != 0;
			_size = 0;
			for (std::size_t i = 0; i < _sizes.size(); ++i)
				if (_sizes[i].first <= width && _sizes[i].second <= height)
					_size = i;
			_screen = Screen::Display;
			_choice = 0;
			_shown = false;
		}
		else if (chosen == "ViewCredits")
		{
			_screen = Screen::Closed;
			_viewCredits();
		}
		return;
	}

	if (_screen == Screen::Keys)
	{
		drawKeys(intent, proj, back, confirm, rightPressed ? 1 : leftPressed ? -1 : 0);
		return;
	}
	if (_screen == Screen::Display)
	{
		drawDisplay(intent, proj, back, confirm, rightPressed ? 1 : leftPressed ? -1 : 0);
		return;
	}

	// The sound page: a slider picked moves with left and right, or to where
	// its bar is clicked.
	std::vector<Entry> entries;
	for (const Slider& slider : kSliders)
		entries.push_back({slider.name, true});
	entries.push_back({"SurroundSound", false});
	if (_shown)
		pick(*_sound, entries, intent);
	else
		colour(*_sound, entries);
	for (int i = 0; i < static_cast<int>(std::size(kSliders)); ++i)
	{
		const Slider& slider = kSliders[i];
		float value = _audio.GetChannelVolume(slider.channel);
		float along = 0.0f;
		if (_shown && intent.mouseLeftClick &&
		    inside(*_sound, std::string(slider.name) + "_SliderBgd", intent.mouseX, intent.mouseY, &along))
		{
			_choice = i;
			value = along;
		}
		else if (i == _choice && (leftPressed || rightPressed))
			value += rightPressed ? kStep : -kStep;
		// To a tenth: ten steps from silent to full, none left over.
		value = std::clamp(std::round(value * 10.0f) / 10.0f, 0.0f, 1.0f);
		if (value != _audio.GetChannelVolume(slider.channel))
			_audio.SetChannelVolume(slider.channel, value);
		const std::string name = slider.name;
		_sound->SetSpriteFill(name + "_Slider", value);
		// The knob where the value is along the bar: the speaker, or crossed
		// out when silent.
		float barX, barY, barW, barH;
		if (_sound->GetSpriteBox(name + "_SliderBgd", barX, barY, barW, barH))
			for (const char* part : {"_Icon", "_Off"})
			{
				float x, y, w, h;
				if (_sound->GetSpriteBox(name + part, x, y, w, h))
					_sound->SetSpriteBox(name + part, barX + value * barW - w * 0.5f, y, w, h);
			}
		_sound->SetSpriteVisible(name + "_Icon", value > 0.0f);
		_sound->SetSpriteVisible(name + "_Off", value <= 0.0f);
	}
	colour(*_sound, entries);
	_sound->Draw(proj);
	_shown = true;
	if (back)
	{
		save();
		_screen = Screen::Options;
		_choice = 1;
		_shown = false;
	}
}

void OptionsMenu::drawDisplay(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm, int by)
{
	// Top to bottom; a value changes with left and right, or a click on its
	// arrows, and is only taken by Accept.
	const std::vector<Entry> entries = {
	    {"Resolution", true}, {"ColourDepth", false}, {"DisplayMode", true}, {"ApplyChanges", true}};
	const int clicked = _shown ? pick(*_display, entries, intent) : -1;
	if (!_shown)
		colour(*_display, entries);
	for (const char* entry : {"Resolution", "DisplayMode"})
	{
		if (!_shown || !intent.mouseLeftClick)
			break;
		if (inside(*_display, std::string(entry) + "_ArrowL", intent.mouseX, intent.mouseY))
			by = -1, _choice = entry[0] == 'R' ? 0 : 2;
		else if (inside(*_display, std::string(entry) + "_ArrowR", intent.mouseX, intent.mouseY))
			by = 1, _choice = entry[0] == 'R' ? 0 : 2;
	}
	if (by != 0 && _choice == 0)
		_size = static_cast<std::size_t>(std::clamp(static_cast<int>(_size) + by, 0, static_cast<int>(_sizes.size()) - 1));
	if (by != 0 && _choice == 2)
		_fullScreen = !_fullScreen;
	colour(*_display, entries);
	_display->SetText("Resolution_Value", fmt::format("{} x {}", _sizes[_size].first, _sizes[_size].second));
	const TextBible& bible = Game::GetInstance().GetTextBible();
	const std::string& mode = bible.Get(_fullScreen ? "FULL_SCREEN" : "WINDOW");
	_display->SetText("DisplayMode_Value", mode.empty() ? (_fullScreen ? "Full Screen" : "Window") : mode);
	_display->Draw(proj);
	_shown = true;
	if ((confirm || clicked == 3) && _choice == 3)
	{
		_keptWidth = _sizes[_size].first;
		_keptHeight = _sizes[_size].second;
		_keptFullScreen = _fullScreen;
		applyDisplay();
		save();
	}
	if (back)
	{
		_screen = Screen::Options;
		_choice = 4;
		_shown = false;
	}
}

void OptionsMenu::drawKeys(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm, int by)
{
	const auto cell = [](int column, int row) { return fmt::format("Map{}Label{:02}", column + 1, row); };
	// A key pressed for the cell picked: it takes the cell's place among its
	// action's keys, the action's other inputs (the pad's) kept.
	if (_capturing)
	{
		InputSource captured;
		if (Input::ConsumeCapturedInput(captured))
		{
			_capturing = false;
			if (captured.kind == InputSourceKind::Key)
			{
				const GameAction action = kKeyRows[_choice];
				Keymap keymap = Input::GetKeymap();
				std::vector<InputSource> keys = keysOf(action);
				if (_column < static_cast<int>(keys.size()))
					keys[_column] = captured;
				else
					keys.push_back(captured);
				auto& bindings = keymap[static_cast<std::size_t>(action)];
				bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
				                              [](const InputSource& s) { return s.kind == InputSourceKind::Key; }),
				               bindings.end());
				bindings.insert(bindings.begin(), keys.begin(), keys.end());
				Input::SetKeymap(keymap);
				Input::SaveKeymap("keymap.conf");
				Log::Info("Options: row {}, key {} bound to {}", _choice, _column + 1, KeymapConfig::HumanLabel(captured));
			}
			// The key that was bound does not act on the page as well.
			_shown = false;
		}
		else if (!Input::IsCapturing())
		{
			// Escape: nothing bound.
			_capturing = false;
			_shown = false;
		}
	}
	else
	{
		// Up and down pick a row, left and right a column; the mouse a cell.
		const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
		const auto step = [&](int from, int delta) {
			int at = from;
			for (int i = 0; i < kKeyRowCount; ++i)
			{
				at = (at + delta + kKeyRowCount) % kKeyRowCount;
				if (kKeyRows[at] != GameAction::Count)
					return at;
			}
			return from;
		};
		if (kKeyRows[_choice] == GameAction::Count)
			_choice = step(_choice, 1);
		if (_shown && up && !_upHeld)
			_choice = step(_choice, -1);
		if (_shown && down && !_downHeld)
			_choice = step(_choice, 1);
		_upHeld = up;
		_downHeld = down;
		if (_shown && by != 0)
			_column = 1 - _column;
		bool clicked = false;
		const bool moved = _mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY);
		for (int row = 0; row < kKeyRowCount; ++row)
			for (int column = 0; column < 2; ++column)
				if (kKeyRows[row] != GameAction::Count && inside(*_keys, cell(column, row), intent.mouseX, intent.mouseY))
				{
					if (moved || intent.mouseLeftClick)
						_choice = row, _column = column;
					clicked = clicked || (_shown && intent.mouseLeftClick);
				}
		_mouseX = intent.mouseX;
		_mouseY = intent.mouseY;
		if (back)
		{
			_screen = Screen::Options;
			_choice = 0;
			_shown = false;
			return;
		}
		if (_shown && (confirm || clicked))
		{
			Input::BeginCapture();
			_capturing = true;
		}
	}

	for (int row = 0; row < kKeyRowCount; ++row)
	{
		const std::string label = fmt::format("Label{:02}", row);
		const bool used = kKeyRows[row] != GameAction::Count;
		const std::vector<InputSource> keys = used ? keysOf(kKeyRows[row]) : std::vector<InputSource>();
		if (used)
			_keys->ResetTextColor(label);
		else
			_keys->SetTextColor(label, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		for (int column = 0; column < 2; ++column)
		{
			const std::string name = cell(column, row);
			const bool picked = used && row == _choice && column == _column;
			if (picked && _capturing)
				_keys->SetText(name, "...");
			else if (used)
				_keys->SetText(name, column < static_cast<int>(keys.size()) ? KeymapConfig::HumanLabel(keys[column]) : "-");
			if (!used)
				_keys->SetTextColor(name, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
			else if (picked)
				_keys->SetTextColor(name, Vector4(1.0f, 0.84f, 0.0f, 1.0f));
			else
				_keys->ResetTextColor(name);
		}
	}
	_keys->Draw(proj);
	_shown = true;
}

void OptionsMenu::applyDisplay()
{
	if (!_window || _keptWidth <= 0 || _keptHeight <= 0)
		return;
	// Out of full screen first: a window's size is not the screen's mode.
	SDL_SetWindowFullscreen(_window, 0);
	SDL_SetWindowSize(_window, _keptWidth, _keptHeight);
	if (_keptFullScreen)
	{
		SDL_DisplayMode mode {SDL_PIXELFORMAT_UNKNOWN, _keptWidth, _keptHeight, 0, nullptr};
		SDL_SetWindowDisplayMode(_window, &mode);
		SDL_SetWindowFullscreen(_window, SDL_WINDOW_FULLSCREEN);
	}
	else
		SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	Log::Info("Options: display {} x {}, {}", _keptWidth, _keptHeight, _keptFullScreen ? "full screen" : "window");
}

void OptionsMenu::load()
{
	std::ifstream file(kSettings);
	std::string key;
	float value;
	while (file >> key >> value)
	{
		for (const Slider& slider : kSliders)
			if (key == slider.key)
				_audio.SetChannelVolume(slider.channel, value);
		if (key == "width")
			_keptWidth = static_cast<int>(value);
		else if (key == "height")
			_keptHeight = static_cast<int>(value);
		else if (key == "fullscreen")
			_keptFullScreen = value != 0.0f;
	}
}

void OptionsMenu::save() const
{
	std::ofstream file(kSettings);
	for (const Slider& slider : kSliders)
		file << slider.key << ' ' << _audio.GetChannelVolume(slider.channel) << '\n';
	if (_keptWidth > 0 && _keptHeight > 0)
		file << "width " << _keptWidth << "\nheight " << _keptHeight << "\nfullscreen " << (_keptFullScreen ? 1 : 0)
		     << '\n';
	Log::Info("Options: volumes saved, music {:.1f}, effects {:.1f}, vehicle {:.1f}, voice {:.1f}",
	          _audio.GetChannelVolume(AudioManager::Channel::Music),
	          _audio.GetChannelVolume(AudioManager::Channel::Effects),
	          _audio.GetChannelVolume(AudioManager::Channel::Engine),
	          _audio.GetChannelVolume(AudioManager::Channel::Voice));
}

} // namespace Donut
