#include "OptionsMenu.h"

#include "Audio/AudioManager.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "FrontendProject.h"
#include "Game/GameInput.h"

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

OptionsMenu::OptionsMenu(AudioManager& audio, std::function<void()> viewCredits)
    : _audio(audio), _viewCredits(std::move(viewCredits))
{
	load();
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
		const std::vector<Entry> entries = {{"Controller", false},
		                                    {"Sound", true},
		                                    {"ViewMovies", false},
		                                    {"ViewCredits", !_fromPause && _viewCredits != nullptr},
		                                    {"Display", false}};
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
		else if (chosen == "ViewCredits")
		{
			_screen = Screen::Closed;
			_viewCredits();
		}
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

void OptionsMenu::load()
{
	std::ifstream file(kSettings);
	std::string key;
	float value;
	while (file >> key >> value)
		for (const Slider& slider : kSliders)
			if (key == slider.key)
				_audio.SetChannelVolume(slider.channel, value);
}

void OptionsMenu::save() const
{
	std::ofstream file(kSettings);
	for (const Slider& slider : kSliders)
		file << slider.key << ' ' << _audio.GetChannelVolume(slider.channel) << '\n';
	Log::Info("Options: volumes saved, music {:.1f}, effects {:.1f}, vehicle {:.1f}, voice {:.1f}",
	          _audio.GetChannelVolume(AudioManager::Channel::Music),
	          _audio.GetChannelVolume(AudioManager::Channel::Effects),
	          _audio.GetChannelVolume(AudioManager::Channel::Engine),
	          _audio.GetChannelVolume(AudioManager::Channel::Voice));
}

} // namespace Donut
