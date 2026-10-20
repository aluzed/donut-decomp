#pragma once

#include "Core/Math/Matrix4x4.h"

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

class FrontendProject;
class Texture;
struct GameIntent;

// The game's scrapbook, from the main menu (scrooby2's ScrapBook.pag): Open
// Book, the contents (ScrapBookContents.pag: the story's missions, clothes,
// vehicles and cards, so many of each unlocked), and the game's stats
// (ScrapBookStats.pag). Of the galleries the contents lead to, the cards'
// (CardGallery.pag: a level's seven, those found and those not; ViewCard.pag
// and HighResCard.pag: one, large, with its title, episode, description and
// quotes) and the missions' (MissionGallery.pag: a level's seven and its bonus
// mission, the picture of each won, the title of the one picked), the
// vehicles' (VehicleGallery.pag) and the clothes' (SkinGallery.pag: a level's
// three for sale, those bought, the others the level's character in shadow).
class ScrapBookMenu
{
public:
	// What the save files say was done; none where they do not keep it.
	struct Stats
	{
		int storyMissions = 0, storyMissionsTotal = 0;
		int cards = 0, gags = 0, wasps = 0;
		// The cards found, by their place in the game's deck: eight a level,
		// the level's seven then its bonus card, level 1's first 0
		// (CARD_TITLE_00, card00.png).
		std::set<int> cardsFound;
		// The story's missions won, by their place in the gallery: eight a
		// level, m1 to m7 then the bonus mission, level 1's m1 0.
		std::set<int> missionsWon;
		// Every level's six vehicles (rewards.mfk: its own car, the bonus
		// mission's, the street races', three for sale), level by level, and
		// whether each is the player's.
		std::vector<std::pair<std::string, bool>> vehicleList;
		// Every level's three clothes for sale (rewards.mfk's "skin"
		// "forsale"), level by level, and whether each was bought.
		std::vector<std::pair<std::string, bool>> skinList;
		std::optional<int> bonusMissions, streetRaces, clothing, vehicles;
	};

	// `picture` gives a mission's briefing picture by its file
	// ("art/frontend/dynaload/images/mis01_01.p3d"), or none.
	explicit ScrapBookMenu(std::function<Texture*(const std::string&)> picture);
	~ScrapBookMenu();

	bool IsLoaded() const { return _hub != nullptr; }
	void Open(const Stats& stats);
	bool IsOpen() const { return _screen != Screen::Closed; }
	// Draws the page open and takes the keys and the mouse: up and down pick,
	// the confirm key opens, Escape goes back.
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

	enum class Screen
	{
		Closed,
		Hub,
		Contents,
		Stats,
		Cards,
		Card,
		Missions,
		Vehicles,
		Clothing,
	};
	Screen GetScreen() const { return _screen; }

	// A PNG picture (a car's, cars2D/<car>.png), loaded once into the
	// resource manager by its path; null if it cannot be read.
	static Texture* image(const std::string& path);

private:
	std::unique_ptr<FrontendProject> _hub, _contents, _stats, _gallery, _card, _largeCard, _missions;
	std::function<Texture*(const std::string&)> _picture;
	std::set<int> _missionsWon;
	// The missions' level shown (0-6) and the one picked (0-7); the contents'
	// entry picked (0 the missions, then round the page clockwise).
	int _missionLevel = 0, _missionPicked = 0, _contentsPicked = 0;
	void drawMissions(const GameIntent& intent, const Matrix4x4& proj, bool back);
	std::unique_ptr<FrontendProject> _vehicleGallery;
	std::vector<std::pair<std::string, bool>> _vehicles;
	int _vehicleLevel = 0, _vehiclePicked = 0;
	void drawVehicles(const GameIntent& intent, const Matrix4x4& proj, bool back);
	void layOutVehicles();
	void layOutMissions();
	std::unique_ptr<FrontendProject> _skinGallery;
	std::vector<std::pair<std::string, bool>> _skins;
	int _skinLevel = 0, _skinPicked = 0;
	void drawClothing(const GameIntent& intent, const Matrix4x4& proj, bool back);
	void layOutClothing();
	std::set<int> _cardsFound;
	// The cards' level shown (0-6), and the one picked on it (0-6).
	int _cardLevel = 0, _cardPicked = 0;
	bool _leftHeld = false, _rightHeld = false;
	void drawCards(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm);
	void drawCard(const GameIntent& intent, const Matrix4x4& proj, bool back, bool confirm);
	// The gallery's cards for the level shown, the frame about the one picked.
	void layOutCards();
	// A card's picture (resource/images/cards/db/cardNN.png), or the card not
	// found yet (cardred.png).
	static Texture* cardPicture(int card);
	Screen _screen = Screen::Closed;
	int _choice = 0;
	bool _upHeld = false, _downHeld = false;
	// Not the frame a page opens: the key that opened it would act on it.
	bool _shown = false;
	int _mouseX = -1, _mouseY = -1;
};

} // namespace Donut
