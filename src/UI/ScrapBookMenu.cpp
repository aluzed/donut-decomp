#include "ScrapBookMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Game/GameInput.h"
#include "Render/Texture.h"
#include "ResourceManager.h"
#include "ThirdParty/stb_image.h"
#include "UI/TextBible.h"

#include <fmt/format.h>

namespace Donut
{

namespace
{
constexpr const char* kFrontend = "./art/frontend/scrooby2/frontend.p3d";
constexpr const char* kCardImages = "./art/frontend/scrooby2/resource/images/cards/";
// A level's cards: seven found in it, of eight in the deck (the eighth its
// bonus card, given otherwise).
constexpr int kCardsALevel = 7, kDeckALevel = 8, kLevels = 7;
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
	_gallery = std::make_unique<FrontendProject>();
	_gallery->LoadP3D(kFrontend, "CardGallery.pag");
	_card = std::make_unique<FrontendProject>();
	_card->LoadP3D(kFrontend, "ViewCard.pag");
	_card->SetTextsFitHeight(true);
	// Its small cards and the quotes' speakers are not shown: the large card
	// (HighResCard.pag) takes their place.
	for (int i = 0; i < kCardsALevel; ++i)
		_card->SetSpriteVisible(fmt::format("Card{}", i), false);
	for (int i = 0; i < 3; ++i)
		_card->SetSpriteVisible(fmt::format("Quote{}", i), false);
	_largeCard = std::make_unique<FrontendProject>();
	_largeCard->LoadP3D(kFrontend, "HighResCard.pag");
}

Texture* ScrapBookMenu::image(const std::string& path)
{
	auto& resources = Game::GetInstance().GetResourceManager();
	if (Texture* loaded = resources.GetTexture(path))
		return loaded;
	int width = 0, height = 0, channels = 0;
	uint8_t* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
	if (!pixels)
		return nullptr;
	resources.AddTexture(path, std::make_unique<Texture>(pixels, width, height, path));
	stbi_image_free(pixels);
	return resources.GetTexture(path);
}

Texture* ScrapBookMenu::cardPicture(int card)
{
	return card >= 0 ? image(fmt::format("{}db/card{:02}.png", kCardImages, card)) : image(std::string(kCardImages) + "cardred.png");
}

void ScrapBookMenu::layOutCards()
{
	// The cards as large as the frame's picture (frame.png, 84 x 108): their
	// boxes, 191 x 245, are the full card's, and the slots 120 apart.
	Texture* frame = image(std::string(kCardImages) + "frame.png");
	for (int i = 0; i < kCardsALevel; ++i)
	{
		const int card = _cardLevel * kDeckALevel + i;
		const std::string name = fmt::format("Card{}", i);
		_gallery->SetSpriteTexture(name, cardPicture(_cardsFound.count(card) ? card : -1));
		float x, y, w, h;
		if (frame && _gallery->GetSpriteBox(name, x, y, w, h))
			_gallery->SetSpriteBox(name, x, y, static_cast<float>(frame->GetWidth()), static_cast<float>(frame->GetHeight()));
	}
	// The frame, its stars about the card picked.
	float x, y, w, h;
	if (_gallery->GetSpriteBox(fmt::format("Card{}", _cardPicked), x, y, w, h))
	{
		float fx, fy, fw, fh;
		if (frame && _gallery->GetSpriteBox("CardFrame", fx, fy, fw, fh))
			_gallery->SetSpriteBox("CardFrame", x, y, static_cast<float>(frame->GetWidth()),
			                       static_cast<float>(frame->GetHeight()));
		_gallery->BringSpriteToFront("CardFrame");
	}
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

	_cardsFound = stats.cardsFound;
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

	if (_screen == Screen::Cards)
	{
		drawCards(intent, proj, back, confirm);
		return;
	}
	if (_screen == Screen::Card)
	{
		drawCard(intent, proj, back, confirm);
		return;
	}
	if (_screen == Screen::Contents)
	{
		// Of the contents, the cards are open; the missions, clothes and
		// vehicles' galleries are not done, greyed.
		for (const char* other : {"StoryMissions", "CharacterClothing", "Vehicles"})
			_contents->SetTextColor(other, kGreyed);
		_contents->SetTextColor("CollectorCards", kPicked);
		_contents->Draw(proj);
		const bool clicked = _shown && intent.mouseLeftClick &&
		                     inside(*_contents, "CollectorCards", intent.mouseX, intent.mouseY);
		if (back)
		{
			_screen = Screen::Hub;
			_shown = false;
			return;
		}
		if (confirm || clicked)
		{
			_screen = Screen::Cards;
			_cardLevel = 0;
			_cardPicked = 0;
			layOutCards();
			_shown = false;
			return;
		}
		_shown = true;
		return;
	}

	// The stats are read, not picked from: Escape, the confirm key or a
	// click goes back to the book's first page.
	_stats->Draw(proj);
	if (back || confirm || (_shown && intent.mouseLeftClick))
	{
		_screen = Screen::Hub;
		_shown = false;
		return;
	}
	_shown = true;
}

void ScrapBookMenu::drawCards(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm)
{
	// Two rows, four cards then three: left and right go from card to card,
	// past a level's last to the next level's first; up and down from row to
	// row.
	const bool left = intent.moveLeft > 0.5f, right = intent.moveRight > 0.5f;
	const bool up = intent.moveForward > 0.5f, down = intent.moveBackward > 0.5f;
	int card = _cardLevel * kCardsALevel + _cardPicked;
	if (_shown && left && !_leftHeld)
		card = (card + kLevels * kCardsALevel - 1) % (kLevels * kCardsALevel);
	if (_shown && right && !_rightHeld)
		card = (card + 1) % (kLevels * kCardsALevel);
	if (_shown && ((up && !_upHeld) || (down && !_downHeld)))
	{
		const int slot = card % kCardsALevel;
		card += slot < 4 ? std::min(4, kCardsALevel - 1 - slot) : -4;
	}
	_leftHeld = left;
	_rightHeld = right;
	_upHeld = up;
	_downHeld = down;
	// The mouse over a card picks it.
	bool clicked = false;
	const bool moved = _mouseX >= 0 && (intent.mouseX != _mouseX || intent.mouseY != _mouseY);
	for (int i = 0; i < kCardsALevel; ++i)
		if (inside(*_gallery, fmt::format("Card{}", i), intent.mouseX, intent.mouseY))
		{
			if (moved || intent.mouseLeftClick)
				card = _cardLevel * kCardsALevel + i;
			clicked = clicked || (_shown && intent.mouseLeftClick);
		}
	_mouseX = intent.mouseX;
	_mouseY = intent.mouseY;
	if (card != _cardLevel * kCardsALevel + _cardPicked)
	{
		_cardLevel = card / kCardsALevel;
		_cardPicked = card % kCardsALevel;
		layOutCards();
	}
	_gallery->Draw(proj);
	_shown = true;
	if (back)
	{
		_screen = Screen::Contents;
		_shown = false;
		return;
	}
	// A card found opens large.
	const int deck = _cardLevel * kDeckALevel + _cardPicked;
	if ((confirm || clicked) && _cardsFound.count(deck))
	{
		const TextBible& bible = Game::GetInstance().GetTextBible();
		// The text bible keeps a phrase as a spreadsheet would: within quotes
		// when it holds a comma or a quote, each quote doubled.
		const auto text = [&](const char* key) {
			std::string phrase = bible.Get(fmt::format("{}_{:02}", key, deck));
			if (phrase.size() > 1 && phrase.front() == '"' && phrase.back() == '"')
				phrase = phrase.substr(1, phrase.size() - 2);
			for (std::size_t at = 0; (at = phrase.find("\"\"", at)) != std::string::npos; ++at)
				phrase.erase(at, 1);
			return phrase;
		};
		_card->SetText("CardTitle", text("CARD_TITLE"));
		_card->SetText("CardEpisode", text("CARD_EPISODE"));
		_card->SetTextWrapped("CardDescription", text("CARD_DESC"));
		// The two quotes, one after the other.
		std::string quotes = text("CARD_QUOTE1");
		if (const std::string& second = text("CARD_QUOTE2"); !second.empty() && second != "*")
			quotes += " " + second;
		_card->SetTextWrapped("Quote0", quotes);
		_card->SetText("Quote1", " ");
		_card->SetText("Quote2", " ");
		_largeCard->SetSpriteTexture("Card", cardPicture(deck));
		_screen = Screen::Card;
		_shown = false;
	}
}

void ScrapBookMenu::drawCard(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm)
{
	_card->Draw(proj);
	_largeCard->Draw(proj);
	if (back || confirm || (_shown && intent.mouseLeftClick))
	{
		_screen = Screen::Cards;
		_shown = false;
		return;
	}
	_shown = true;
}

} // namespace Donut
