#pragma once

#include "Core/Math/Matrix4x4.h"

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
// music, the effects, the cars and the voices, each a slider. What is not
// done here is shown greyed and passed over.
//
// The volumes are kept in donut_settings.dat.
class OptionsMenu
{
public:
	// `viewCredits` rolls the credits; none, from the pause screen.
	OptionsMenu(AudioManager& audio, std::function<void()> viewCredits);
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
	};
	struct Entry
	{
		const char* name;
		bool enabled;
	};
	AudioManager& _audio;
	std::function<void()> _viewCredits;
	std::unique_ptr<FrontendProject> _options;
	std::unique_ptr<FrontendProject> _sound;
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
	void load();
	void save() const;
};

} // namespace Donut
