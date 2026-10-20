#include "ScrapBookMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game/GameInput.h"

#include <fmt/format.h>

namespace Donut
{

namespace
{
constexpr const char* kFrontend = "./art/frontend/scrooby2/frontend.p3d";
const Vector4 kPicked(1.0f, 0.84f, 0.0f, 1.0f);
const Vector4 kGreyed(0.5f, 0.5f, 0.5f, 1.0f);

bool inside(const FrontendProject& page, const std::string& name, int mouseX, int mouseY)
{
	float x, y, w, h;
	return page.GetRect(name, x, y, w, h) && mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
}
} // namespace

ScrapBookMenu::ScrapBookMenu()
{
	if (!FileSystem::exists(kFrontend))
		return;
	_hub = std::make_unique<FrontendProject>();
	_hub->LoadP3D(kFrontend, "ScrapBook.pag");
	_contents = std::make_unique<FrontendProject>();
	_contents->LoadP3D(kFrontend, "ScrapBookContents.pag");
	_stats = std::make_unique<FrontendProject>();
	_stats->LoadP3D(kFrontend, "ScrapBookStats.pag");
	_stats->SetTextsFitHeight(true);
}

ScrapBookMenu::~ScrapBookMenu() = default;

void ScrapBookMenu::Open(const Stats& stats)
{
	if (!IsLoaded())
		return;
	_screen = Screen::Hub;
	_choice = 0;
	_shown = false;
	_mouseX = _mouseY = -1;

	// The counts, as so many of so many where the total is known; greyed, a
	// dash, where the save files do not keep it (GAME-011).
	const auto set = [](FrontendProject& page, const std::string& name, std::optional<int> value, int total = 0) {
		const std::string label = name.substr(0, name.rfind('_'));
		if (!value)
		{
			page.SetText(name, "-");
			page.SetTextColor(name, kGreyed);
			page.SetTextColor(label, kGreyed);
			return;
		}
		page.SetText(name, total > 0 ? fmt::format("{}/{}", *value, total) : std::to_string(*value));
	};
	set(*_stats, "StoryMissions_Unlocked", stats.storyMissions, stats.storyMissionsTotal);
	set(*_stats, "BonusMissions_Unlocked", stats.bonusMissions);
	set(*_stats, "StreetRaces_Unlocked", stats.streetRaces);
	set(*_stats, "Cards_Unlocked", stats.cards);
	set(*_stats, "Clothing_Unlocked", stats.clothing);
	set(*_stats, "Vehicles_Unlocked", stats.vehicles);
	set(*_stats, "Wasps_Unlocked", stats.wasps);
	set(*_stats, "Gags_Unlocked", stats.gags);
	// The movies are not played (UI-FMV), nor is the whole reckoned.
	set(*_stats, "Movies_Unlocked", std::nullopt);
	set(*_stats, "GameComplete_Value", std::nullopt);

	set(*_contents, "StoryMissions_Unlocked", stats.storyMissions, stats.storyMissionsTotal);
	set(*_contents, "CollectorCards_Unlocked", stats.cards);
	set(*_contents, "CharacterClothing_Unlocked", stats.clothing);
	set(*_contents, "Vehicles_Unlocked", stats.vehicles);
}

void ScrapBookMenu::Draw(const GameIntent& intent, const Matrix4x4& proj)
{
	if (_screen == Screen::Closed)
		return;
	const bool back = _shown && intent.pauseToggle;
	const bool confirm = _shown && intent.uiConfirm;

	if (_screen == Screen::Hub)
	{
		// Open Book above Game Stats.
		static const char* const kEntries[] = {"OpenBook", "GameStats"};
		const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
		if (_shown && ((up && !_upHeld) || (down && !_downHeld)))
			_choice = 1 - _choice;
		_upHeld = up;
		_downHeld = down;
		bool clicked = false;
		const bool moved = _mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY);
		for (int i = 0; i < 2; ++i)
			if (inside(*_hub, kEntries[i], intent.mouseX, intent.mouseY))
			{
				if (moved || intent.mouseLeftClick)
					_choice = i;
				clicked = clicked || (_shown && intent.mouseLeftClick);
			}
		_mouseX = intent.mouseX;
		_mouseY = intent.mouseY;
		for (int i = 0; i < 2; ++i)
			if (i == _choice)
				_hub->SetTextColor(kEntries[i], kPicked);
			else
				_hub->ResetTextColor(kEntries[i]);
		_hub->Draw(proj);
		_shown = true;
		if (back)
			_screen = Screen::Closed;
		else if (confirm || clicked)
		{
			_screen = _choice == 0 ? Screen::Contents : Screen::Stats;
			_shown = false;
		}
		return;
	}

	// The contents and the stats are read, not picked from: Escape, the
	// confirm key or a click goes back to the book's first page.
	(_screen == Screen::Contents ? *_contents : *_stats).Draw(proj);
	if (back || confirm || (_shown && intent.mouseLeftClick))
	{
		_screen = Screen::Hub;
		_shown = false;
		return;
	}
	_shown = true;
}

} // namespace Donut
