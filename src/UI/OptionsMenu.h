#pragma once

#include "Core/Math/Matrix4x4.h"

#include <SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class AudioManager;
class FrontendProject;
struct GameIntent;

// The game's options (scrooby2's Options.pag) and its sound page (Sound.pag),
// from the main menu or the pause screen. The options list Controller, Sound,
// View Movies, View Credits and Display; the sound page the volume of the
// music, the effects, the cars and the voices, each a slider; the display page
// (Display.pag) the window's size and whether it fills the screen; Controller
// the keys (PCKeyboardConfig.pag), two for each of the game's commands. What is not
// done here is shown greyed and passed over.
//
// The volumes and the display are kept in donut_settings.dat, the keys in
// keymap.conf.
class OptionsMenu
{
public:
	// `viewCredits` rolls the credits; none, from the pause screen.
	OptionsMenu(AudioManager& audio, SDL_Window* window, std::function<void()> viewCredits);
	~OptionsMenu();

	bool IsLoaded() const { return _options != nullptr; }
	void Open(bool fromPause);
	bool IsOpen() const { return _screen != Screen::Closed; }
	// Draws the page open and takes the keys and the mouse: up and down pick,
	// left and right move a slider, the confirm key opens, Escape goes back.
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

private:
	enum class Screen
	{
		Closed,
		Options,
		Sound,
		Display,
		Keys,
	};
	struct Entry
	{
		const char* name;
		bool enabled;
	};
	AudioManager& _audio;
	SDL_Window* _window;
	std::function<void()> _viewCredits;
	std::unique_ptr<FrontendProject> _options;
	std::unique_ptr<FrontendProject> _sound;
	std::unique_ptr<FrontendProject> _display;
	std::unique_ptr<FrontendProject> _keys;
	// The screen's sizes, smallest first; the one picked and whether the game
	// fills the screen, as the page shows them until they are accepted.
	std::vector<std::pair<int, int>> _sizes;
	std::size_t _size = 0;
	bool _fullScreen = false;
	// As kept: the window's size (0: as it was opened) and full screen.
	int _keptWidth = 0, _keptHeight = 0;
	bool _keptFullScreen = false;
	Screen _screen = Screen::Closed;
	bool _fromPause = false;
	int _choice = 0;
	// Keys as they were: a move is taken when one goes down.
	bool _upHeld = false, _downHeld = false, _leftHeld = false, _rightHeld = false;
	// Not the frame a page opens: the key that opened it would act on it.
	bool _shown = false;
	int _mouseX = -1, _mouseY = -1;

	// Picks among `entries` with the keys and the mouse; the entry clicked
	// on, or -1.
	int pick(FrontendProject& page, const std::vector<Entry>& entries, const GameIntent& intent);
	void colour(FrontendProject& page, const std::vector<Entry>& entries);
	// The key cell picked waits for a key (Input's capture), or none.
	bool _capturing = false;
	int _column = 0;
	void drawKeys(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm, int by);
	void drawDisplay(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm, int by);
	// The window at the size and mode kept.
	void applyDisplay();
	void load();
	void save() const;
};

} // namespace Donut
