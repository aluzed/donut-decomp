#include "PauseSundayMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Game/GameInput.h"
#include "Render/Texture.h"
#include "ResourceManager.h"
#include "UI/ScrapBookMenu.h"

#include <algorithm>

#include <fmt/format.h>

namespace Donut
{

namespace
{
constexpr const char* kIngame = "./art/frontend/scrooby2/ingame.p3d";
constexpr const char* kCards = "./art/frontend/scrooby2/resource/images/cards/";
constexpr int kMissions = 7, kCardsALevel = 7;
const Vector4 kPicked(1.0f, 0.84f, 0.0f, 1.0f);
const Vector4 kGreyed(0.5f, 0.5f, 0.5f, 1.0f);

bool inside(const FrontendProject& page, const std::string& name, int mouseX, int mouseY)
{
	float x, y, w, h;
	return page.GetRect(name, x, y, w, h) && mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
}

std::unique_ptr<FrontendProject> pages(std::initializer_list<const char*> names)
{
	auto project = std::make_unique<FrontendProject>();
	for (const char* name : names)
		project->LoadP3D(kIngame, name);
	project->SetTextsFitHeight(true);
	return project;
}

// The pause's entries, top to bottom.
constexpr const char* kEntries[] = {"Continue", "MissionSelect", "LevelProgress", "ViewCards", "Options", "SaveGame",
                                    "QuitGame"};
constexpr int kEntryCount = static_cast<int>(std::size(kEntries));
} // namespace

PauseSundayMenu::PauseSundayMenu(Actions actions): _actions(std::move(actions))
{
	if (!FileSystem::exists(kIngame))
		return;
	_pause = pages({"PauseBgd.pag", "Pause.pag", "PauseSunday.pag"});
	_missionSelect = pages({"PauseBgd.pag", "BigBoard.pag", "PauseFgd.pag", "MissionSelect.pag", "Back.pag"});
	_levelStats = pages({"PauseBgd.pag", "PauseFgd.pag", "XSmallBoard.pag", "LevelStats.pag", "Back.pag"});
	_viewCards = pages({"PauseBgd.pag", "BigBoard.pag", "PauseFgd.pag", "PauseViewCards.pag", "Back.pag"});
	_levelEnd = pages({"XSmallBoard.pag", "LevelStats.pag", "Accept.pag"});
	_levelEnd->SetText("UseMissionSelect", " ");
	// The game saves as it goes: Save Game has nothing to do.
	_pause->SetTextColor("SaveGame", kGreyed);
	// Mission select's numbers, one a row.
	for (int i = 0; i < kMissions; ++i)
		_missionSelect->SetText(fmt::format("MissionNum{}", i), std::to_string(i + 1));
	_missionSelect->GetSpriteBox("HighlightBar", _barX, _barY, _barWidth, _barHeight);
	// Its hint (use mission select) stands where the board's own does.
	_levelStats->SetText("UseMissionSelect", " ");
}

PauseSundayMenu::~PauseSundayMenu() = default;

void PauseSundayMenu::Open(int level, std::vector<Mission> missions, Stats stats, std::vector<int> cards)
{
	if (!IsLoaded())
		return;
	_level = std::clamp(level, 1, 7);
	_missions = std::move(missions);
	_missions.resize(kMissions);
	_cards = std::move(cards);
	_cards.resize(kCardsALevel, -1);
	_screen = Screen::Pause;
	_choice = 0;
	_shown = false;
	_mouseX = _mouseY = -1;

	// Mission select: each mission's title of this level, its status a check
	// when won, a dash before.
	auto& resources = Game::GetInstance().GetResourceManager();
	for (int i = 0; i < kMissions; ++i)
	{
		_missionSelect->SetTextIndex(fmt::format("Mission{}", i), static_cast<std::size_t>(_level - 1));
		if (Texture* status = resources.GetTexture(_missions[i].won ? "check.png" : "dash.png"))
			_missionSelect->SetSpriteTexture(fmt::format("MissionStatus{}", i), status);
		if (_missions[i].won && _missions[i].story >= 0)
			_missionSelect->ResetTextColor(fmt::format("Mission{}", i));
		else
			_missionSelect->SetTextColor(fmt::format("Mission{}", i), kGreyed);
	}
	setStats(*_levelStats, stats);
	_levelStats->SetTextIndex("LevelProgress", stats.percent >= 100 ? 1 : 0);
	layOutCards();
}

void PauseSundayMenu::setStats(FrontendProject& page, const Stats& stats)
{
	// The counts, and the whole as a percentage.
	for (const auto& [name, value] : stats.counts)
		page.SetText(name, value);
	page.SetText("LevelComplete_Value", fmt::format("{}%", stats.percent));
}

void PauseSundayMenu::OpenLevelEnd(int level, Stats stats, std::function<void()> done)
{
	if (!IsLoaded())
		return;
	_level = std::clamp(level, 1, 7);
	setStats(*_levelEnd, stats);
	_levelEnd->SetTextIndex("LevelProgress", 1);
	_levelEndDone = std::move(done);
	_screen = Screen::LevelEnd;
	_shown = false;
}

int PauseSundayMenu::pick(FrontendProject& page, int count, const std::function<std::string(int)>& name,
                          const std::function<bool(int)>& allowed, const GameIntent& intent)
{
	const auto step = [&](int from, int by) {
		int at = from;
		for (int i = 0; i < count; ++i)
		{
			at = (at + by + count) % count;
			if (allowed(at))
				return at;
		}
		return from;
	};
	if (!allowed(_choice))
		_choice = step(_choice, 1);
	const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
	if (_shown && up && !_upHeld)
		_choice = step(_choice, -1);
	if (_shown && down && !_downHeld)
		_choice = step(_choice, 1);
	_upHeld = up;
	_downHeld = down;
	int clicked = -1;
	const bool moved = _mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY);
	for (int i = 0; i < count; ++i)
		if (allowed(i) && inside(page, name(i), intent.mouseX, intent.mouseY))
		{
			if (moved)
				_choice = i;
			if (_shown && intent.mouseLeftClick)
				clicked = _choice = i;
		}
	_mouseX = intent.mouseX;
	_mouseY = intent.mouseY;
	return clicked;
}

void PauseSundayMenu::Draw(const GameIntent& intent, const Matrix4x4& proj)
{
	const bool back = _shown && intent.pauseToggle;
	const bool confirm = _shown && intent.uiConfirm;
	switch (_screen)
	{
	case Screen::Closed: return;
	case Screen::Pause: drawPause(intent, proj, back, confirm); return;
	case Screen::MissionSelect: drawMissionSelect(intent, proj, back, confirm); return;
	case Screen::ViewCards: drawViewCards(intent, proj, back); return;
	case Screen::LevelEnd:
		_levelEnd->Draw(proj);
		if (_shown && (intent.uiConfirm || intent.pauseToggle || intent.mouseLeftClick))
		{
			_screen = Screen::Closed;
			if (_levelEndDone)
				_levelEndDone();
			return;
		}
		_shown = true;
		return;
	case Screen::LevelStats:
		// Read, not picked from: any of the keys, or a click, goes back.
		_levelStats->Draw(proj);
		if (back || confirm || (_shown && intent.mouseLeftClick))
		{
			_screen = Screen::Pause;
			_choice = 2;
			_shown = false;
			return;
		}
		_shown = true;
		return;
	}
}

void PauseSundayMenu::drawPause(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm)
{
	const auto allowed = [](int i) { return std::string(kEntries[i]) != "SaveGame"; };
	const int clicked = pick(*_pause, kEntryCount, [](int i) { return std::string(kEntries[i]); }, allowed, intent);
	for (int i = 0; i < kEntryCount; ++i)
		if (!allowed(i))
			continue;
		else if (i == _choice)
			_pause->SetTextColor(kEntries[i], kPicked);
		else
			_pause->ResetTextColor(kEntries[i]);
	_pause->Draw(proj);
	_shown = true;
	if (back)
	{
		_screen = Screen::Closed;
		_actions.resume();
		return;
	}
	if (!confirm && clicked < 0)
		return;
	const std::string chosen = kEntries[_choice];
	_shown = false;
	if (chosen == "Continue")
	{
		_screen = Screen::Closed;
		_actions.resume();
	}
	else if (chosen == "MissionSelect")
	{
		_screen = Screen::MissionSelect;
		_choice = 0;
	}
	else if (chosen == "LevelProgress")
		_screen = Screen::LevelStats;
	else if (chosen == "ViewCards")
	{
		_screen = Screen::ViewCards;
		_choice = 0;
		layOutCards();
	}
	else if (chosen == "Options")
		// Over this screen, which is back when they close.
		_actions.options();
	else if (chosen == "QuitGame")
		_actions.quit();
}

void PauseSundayMenu::drawMissionSelect(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm)
{
	const auto allowed = [this](int i) { return _missions[i].won && _missions[i].story >= 0; };
	const bool any = std::any_of(_missions.begin(), _missions.end(), [](const Mission& m) { return m.won && m.story >= 0; });
	const int clicked = any ? pick(*_missionSelect, kMissions, [](int i) { return fmt::format("Mission{}", i); }, allowed, intent) : -1;
	// The bar across the row picked: the first row's, a row (25 page pixels)
	// down for each.
	_missionSelect->SetSpriteVisible("HighlightBar", any);
	_missionSelect->SetSpriteBox("HighlightBar", _barX, _barY + 25.0f * _choice, _barWidth, _barHeight);
	for (int i = 0; i < kMissions; ++i)
		if (allowed(i))
		{
			if (any && i == _choice)
				_missionSelect->SetTextColor(fmt::format("Mission{}", i), kPicked);
			else
				_missionSelect->ResetTextColor(fmt::format("Mission{}", i));
		}
	_missionSelect->Draw(proj);
	_shown = true;
	if (back)
	{
		_screen = Screen::Pause;
		_choice = 1;
		_shown = false;
		return;
	}
	if (any && (confirm || clicked >= 0) && allowed(_choice))
	{
		_screen = Screen::Closed;
		_actions.replay(_missions[_choice].story);
	}
}

void PauseSundayMenu::layOutCards()
{
	// As the scrapbook's cards: as large as the frame's picture (84 x 108),
	// a card found its own, one not yet the yellow back.
	Texture* frame = ScrapBookMenu::image(std::string(kCards) + "frame.png");
	for (int i = 0; i < kCardsALevel; ++i)
	{
		const std::string name = fmt::format("Card{}", i);
		Texture* picture = _cards[i] >= 0 ? ScrapBookMenu::image(fmt::format("{}db/card{:02}.png", kCards, _cards[i]))
		                                  : ScrapBookMenu::image(std::string(kCards) + "cardyell.png");
		if (picture)
			_viewCards->SetSpriteTexture(name, picture);
		float x, y, w, h;
		if (frame && _viewCards->GetSpriteBox(name, x, y, w, h))
			_viewCards->SetSpriteBox(name, x, y, static_cast<float>(frame->GetWidth()), static_cast<float>(frame->GetHeight()));
	}
	float x, y, w, h;
	if (frame && _viewCards->GetSpriteBox(fmt::format("Card{}", _choice), x, y, w, h))
		_viewCards->SetSpriteBox("CardFrame", x, y, static_cast<float>(frame->GetWidth()), static_cast<float>(frame->GetHeight()));
	_viewCards->BringSpriteToFront("CardFrame");
}

void PauseSundayMenu::drawViewCards(const GameIntent& intent, const Matrix4x4& proj, bool back)
{
	// Left and right from card to card; the mouse picks.
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	int choice = _choice;
	if (_shown && left && !_leftHeld)
		choice = (choice + kCardsALevel - 1) % kCardsALevel;
	if (_shown && right && !_rightHeld)
		choice = (choice + 1) % kCardsALevel;
	_leftHeld = left;
	_rightHeld = right;
	for (int i = 0; i < kCardsALevel; ++i)
		if (_mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY) &&
		    inside(*_viewCards, fmt::format("Card{}", i), intent.mouseX, intent.mouseY))
			choice = i;
	_mouseX = intent.mouseX;
	_mouseY = intent.mouseY;
	if (choice != _choice)
	{
		_choice = choice;
		layOutCards();
	}
	_viewCards->Draw(proj);
	_shown = true;
	if (back)
	{
		_screen = Screen::Pause;
		_choice = 3;
		_shown = false;
	}
}

} // namespace Donut
