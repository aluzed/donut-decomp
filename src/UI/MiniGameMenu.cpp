#include "UI/MiniGameMenu.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "ResourceManager.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

namespace Donut
{

namespace
{
constexpr const char* kMinigame = "./art/frontend/scrooby2/minigame.p3d";
// The page's five, left to right (Character0 to Character4), and the letter
// their faces go by (l_happy.png...).
constexpr const char* kCharacters[] = {"lisa", "bart", "homer", "marge", "apu"};
constexpr char kFaceLetters[] = {'l', 'b', 'h', 'm', 'a'};
constexpr int kCharacterCount = 5;
// The standings' rows, top to bottom, and the face each shows.
constexpr const char* kRowMoods[] = {"happy", "mad", "normal", "sad"};
// The page's rows are this far apart (the summary's), and its tracks.
constexpr float kSummaryRowStep = 70.0f;
constexpr float kTrackStep = 80.0f;
const Vector4 kGreyed(0.35f, 0.35f, 0.35f, 1.0f);
const Vector4 kPicked(1.0f, 0.84f, 0.0f, 1.0f);
const Vector4 kWhite(1.0f, 1.0f, 1.0f, 1.0f);

Texture* image(const std::string& name)
{
	return Game::GetInstance().GetResourceManager().GetTexture(name + ".png");
}

std::unique_ptr<FrontendProject> pages(std::initializer_list<const char*> names)
{
	auto project = std::make_unique<FrontendProject>();
	for (const char* name : names)
		project->LoadP3D(kMinigame, name);
	project->SetTextsFitHeight(true);
	return project;
}

std::string clock(double seconds)
{
	if (seconds < 0.0)
		return "-";
	const int hundredths = static_cast<int>(std::lround(seconds * 100.0));
	return fmt::format("{}:{:02}.{:02}", hundredths / 6000, hundredths / 100 % 60, hundredths % 100);
}
} // namespace

MiniGameMenu::MiniGameMenu()
{
	if (!FileSystem::exists(kMinigame))
		return;
	_menu = pages({"MiniMenu.pag", "Back2.pag", "Accept2.pag"});
	_pause = pages({"PauseBgd.pag", "XSmallBoard.pag", "PauseFgd.pag", "Pause.pag", "MiniPause.pag"});
	_summary = pages({"BigBoard.pag", "MiniSummary.pag", "Continue.pag"});
	layOut();
}

MiniGameMenu::~MiniGameMenu() = default;

void MiniGameMenu::layOut()
{
	for (int p = 0; p < SuperSprint::kRacers; ++p)
	{
		Box& tag = _tagBoxes[p];
		_menu->GetSpriteBox(fmt::format("Player{}_Selected", p), tag.x, tag.y, tag.width, tag.height);
		Box& text = _tagTexts[p];
		_menu->GetTextBox(fmt::format("Player{}", p), text.x, text.y, text.width, text.height);
		_menu->SetText(fmt::format("Player{}", p), fmt::format("P{}", p + 1));
		_menu->SetText(fmt::format("Vehicle{}", p), fmt::format("P{}", p + 1));
	}
	for (int c = 0; c < kCharacterCount; ++c)
	{
		Box& box = _characterBoxes[c];
		_menu->GetSpriteBox(fmt::format("Character{}", c), box.x, box.y, box.width, box.height);
	}
	for (const char* name : {"NumLaps_RArrow", "NumLaps_LArrow", "Direction", "TrackBgd"})
	{
		Box box;
		if (_menu->GetSpriteBox(name, box.x, box.y, box.width, box.height))
			_cursorBoxes.emplace_back(name, box);
	}
	_menu->GetTextBox("NumLaps", _lapsBox.x, _lapsBox.y, _lapsBox.width, _lapsBox.height);
	// The crowns, on the first row.
	const char* crowns[] = {"TotalTimeKingIcon", "BestLapKingIcon"};
	for (int i = 0; i < 2; ++i)
		_summary->GetSpriteBox(crowns[i], _crownBoxes[i].x, _crownBoxes[i].y, _crownBoxes[i].width,
		                       _crownBoxes[i].height);
	_menu->SetText("Timer_0", " ");
}

void MiniGameMenu::OpenSetup(std::vector<Car> cars, int tracks)
{
	if (cars.empty())
		cars.push_back({"famil_v", 5});
	_cars = std::move(cars);
	_tracks = std::clamp(tracks, 1, SuperSprint::kTracks);
	_stages.fill(Stage::Out);
	_stages[0] = Stage::Character;
	_character = {2, 1, 0, 3};
	_vehicle.fill(0);
	_choosingTrack = false;
	_track = std::min(_track, _tracks - 1);
	_setup.laps = std::clamp(_setup.laps, 1, 9);
	for (auto& held : _held)
		held.fill(true);
}

void MiniGameMenu::ReturnToTracks()
{
	_choosingTrack = true;
	for (auto& held : _held)
		held.fill(true);
}

bool MiniGameMenu::characterTaken(int character, int by) const
{
	for (int p = 0; p < SuperSprint::kRacers; ++p)
		if (p != by && _stages[p] != Stage::Out && _character[p] == character)
			return true;
	return false;
}

void MiniGameMenu::stepCharacter(int player, int step)
{
	for (int tries = 0; tries < kCharacterCount; ++tries)
	{
		_character[player] = (_character[player] + step + kCharacterCount) % kCharacterCount;
		if (!characterTaken(_character[player], player))
			return;
	}
}

void MiniGameMenu::fillComputer()
{
	// Those no one joined for: the computer, as whoever is left, in a car of
	// those to pick at random.
	for (int p = 0; p < SuperSprint::kRacers; ++p)
	{
		SuperSprint::Entry& entry = _setup.entries[p];
		if (_stages[p] == Stage::Ready)
		{
			entry.player = p;
			entry.character = kCharacters[_character[p]];
			entry.car = _cars[_vehicle[p] % _cars.size()].name;
			continue;
		}
		entry.player = -1;
		for (int c = 0; c < kCharacterCount; ++c)
		{
			bool taken = false;
			for (int q = 0; q < SuperSprint::kRacers; ++q)
				taken = taken || (q != p && (_stages[q] == Stage::Ready || q < p) && _setup.entries[q].character ==
				                                                                         kCharacters[c]);
			if (!taken)
			{
				entry.character = kCharacters[c];
				break;
			}
		}
		entry.car = _cars[std::uniform_int_distribution<std::size_t>(0, _cars.size() - 1)(_random)].name;
	}
}

MiniGameMenu::Result MiniGameMenu::DrawSetup(const std::array<Pad, SuperSprint::kRacers>& pads, const Matrix4x4& proj)
{
	if (!IsLoaded())
		return Result::Quit;
	// Each player's directions, pressed this frame.
	std::array<std::array<bool, 4>, SuperSprint::kRacers> pressed {};
	for (int p = 0; p < SuperSprint::kRacers; ++p)
	{
		const bool now[4] = {pads[p].left, pads[p].right, pads[p].up, pads[p].down};
		for (int d = 0; d < 4; ++d)
		{
			pressed[p][d] = now[d] && !_held[p][d];
			_held[p][d] = now[d];
		}
	}

	Result result = Result::None;
	if (_choosingTrack)
	{
		const Pad& pad = pads[0];
		if (pressed[0][0])
			_track = (_track + _tracks - 1) % _tracks;
		if (pressed[0][1])
			_track = (_track + 1) % _tracks;
		if (pressed[0][2])
			_setup.laps = std::min(9, _setup.laps + 1);
		if (pressed[0][3])
			_setup.laps = std::max(1, _setup.laps - 1);
		if (pad.aux)
			_setup.reverse = !_setup.reverse;
		if (pad.back)
		{
			_choosingTrack = false;
			for (auto& stage : _stages)
				if (stage == Stage::Ready)
					stage = Stage::Vehicle;
		}
		else if (pad.confirm || pad.start)
		{
			_setup.track = _track + 1;
			fillComputer();
			result = Result::Start;
		}
	}
	else
	{
		for (int p = 0; p < SuperSprint::kRacers; ++p)
		{
			const Pad& pad = pads[p];
			Stage& stage = _stages[p];
			switch (stage)
			{
			case Stage::Out:
				if (pad.connected && (pad.start || pad.confirm))
				{
					stage = Stage::Character;
					if (characterTaken(_character[p], p))
						stepCharacter(p, 1);
				}
				break;
			case Stage::Character:
				if (pressed[p][0])
					stepCharacter(p, -1);
				if (pressed[p][1])
					stepCharacter(p, 1);
				if (pad.confirm)
					stage = Stage::Vehicle;
				else if (pad.back)
				{
					if (p == 0)
						result = Result::Quit;
					else
						stage = Stage::Out;
				}
				break;
			case Stage::Vehicle:
			{
				const int cars = static_cast<int>(_cars.size());
				if (pressed[p][0])
					_vehicle[p] = (_vehicle[p] + cars - 1) % cars;
				if (pressed[p][1])
					_vehicle[p] = (_vehicle[p] + 1) % cars;
				if (pad.confirm || pad.start)
					stage = Stage::Ready;
				else if (pad.back)
					stage = Stage::Character;
				break;
			}
			case Stage::Ready:
				if (pad.back)
					stage = Stage::Vehicle;
				break;
			}
		}
		// Everyone who joined is ready: on to the track.
		bool ready = true;
		for (const Stage stage : _stages)
			ready = ready && (stage == Stage::Out || stage == Stage::Ready);
		if (ready && _stages[0] == Stage::Ready && result == Result::None)
			_choosingTrack = true;
	}

	// The characters: those picked lit, a player's tag under each.
	for (int c = 0; c < kCharacterCount; ++c)
	{
		bool picked = false;
		for (int p = 0; p < SuperSprint::kRacers; ++p)
			picked = picked || (_stages[p] != Stage::Out && _character[p] == c);
		_menu->SetSpriteTexture(fmt::format("Character{}", c),
		                        image(picked ? std::string(kCharacters[c]) + "_s" : kCharacters[c]));
	}
	for (int p = 0; p < SuperSprint::kRacers; ++p)
	{
		const bool in = _stages[p] != Stage::Out;
		const Box& character = _characterBoxes[_character[p]];
		const Box& tag = _tagBoxes[p];
		const float x = character.x + (character.width - tag.width) * 0.5f;
		_menu->SetSpriteBox(fmt::format("Player{}_Selected", p), x, tag.y, tag.width, tag.height);
		_menu->SetSpriteVisible(fmt::format("Player{}_Selected", p), in);
		_menu->SetTextPosition(fmt::format("Player{}", p), x + _tagTexts[p].x - tag.x, _tagTexts[p].y);
		_menu->SetText(fmt::format("Player{}", p), in ? fmt::format("P{}", p + 1) : " ");

		// The player's car: its picture and stars between the arrows, once
		// they are at it; "Press Start" for one not in.
		const bool atCar = _stages[p] == Stage::Vehicle || _stages[p] == Stage::Ready;
		const Car& car = _cars[_vehicle[p] % _cars.size()];
		_menu->SetSpriteVisible(fmt::format("Vehicle{}_Value", p), atCar);
		if (atCar)
			_menu->SetSpriteTexture(fmt::format("Vehicle{}_Value", p), image(car.name));
		_menu->SetSpriteVisible(fmt::format("Vehicle{}_Rating", p), atCar);
		if (atCar)
			_menu->SetSpriteTexture(fmt::format("Vehicle{}_Rating", p), image(fmt::format("S{}", std::clamp(car.rating, 0, 9))));
		for (const char* arrow : {"LArrow", "RArrow"})
			_menu->SetSpriteVisible(fmt::format("Vehicle{}_{}", p, arrow), _stages[p] == Stage::Vehicle);
		_menu->SetSpriteVisible(fmt::format("Vehicle{}_Selected", p), in);
		_menu->SetText(fmt::format("Vehicle{}", p), in ? fmt::format("P{}", p + 1) : " ");
		_menu->SetText(fmt::format("PressStart{}", p), in || !pads[p].connected ? " " : std::string());
		_menu->SetTextColor(fmt::format("Vehicle{}", p), _stages[p] == Stage::Ready ? kPicked : kWhite);
	}
	const bool choosingCharacter = _stages[0] == Stage::Character && !_choosingTrack;
	_menu->SetText("ChangeCharacter", choosingCharacter ? std::string() : " ");
	_menu->SetSpriteVisible("CharacterLArrow", choosingCharacter);
	_menu->SetSpriteVisible("CharacterRArrow", choosingCharacter);
	bool waiting = !_choosingTrack && _stages[0] == Stage::Ready;
	_menu->SetText("WaitingForOtherPlayers", waiting ? std::string() : " ");

	// The tracks, and the cursor over the one picked: its way round, its laps.
	for (const char* name : {"OptionsOverlayBottom", "OptionsOverlay", "OptionsOverlayTop", "Options", "TrackBgd"})
		_menu->SetSpriteVisible(name, _choosingTrack);
	_menu->SetText("SelectTrack", _choosingTrack ? std::string() : " ");
	_menu->SetText("Options", _choosingTrack ? std::string() : " ");
	for (int t = 0; t < SuperSprint::kTracks; ++t)
	{
		_menu->SetSpriteVisible(fmt::format("Track{}", t), _choosingTrack);
		_menu->SetSpriteColor(fmt::format("Track{}", t), t < _tracks ? kWhite : kGreyed);
	}
	const float shift = kTrackStep * static_cast<float>(_track);
	for (const auto& [name, box] : _cursorBoxes)
	{
		_menu->SetSpriteBox(name, box.x + shift, box.y, box.width, box.height);
		_menu->SetSpriteVisible(name, _choosingTrack);
	}
	if (_choosingTrack)
		_menu->SetSpriteTexture("Direction", image(_setup.reverse ? "trackCCW" : "trackCW"));
	_menu->SetTextPosition("NumLaps", _lapsBox.x + shift, _lapsBox.y);
	_menu->SetTextIndex("NumLaps", static_cast<std::size_t>(_setup.laps));
	if (!_choosingTrack)
		_menu->SetText("NumLaps", " ");
	_menu->Draw(proj);
	return result;
}

void MiniGameMenu::OpenPause()
{
	_pauseChoice = 0;
	_pauseHeld[0] = _pauseHeld[1] = true;
	_pauseShown = false;
}

MiniGameMenu::PauseResult MiniGameMenu::DrawPause(const Pad& pad, const Matrix4x4& proj)
{
	if (!_pause)
		return PauseResult::Continue;
	if (pad.up && !_pauseHeld[0])
		_pauseChoice = 0;
	if (pad.down && !_pauseHeld[1])
		_pauseChoice = 1;
	_pauseHeld[0] = pad.up;
	_pauseHeld[1] = pad.down;
	_pause->SetTextColor("Continue", _pauseChoice == 0 ? kPicked : kWhite);
	_pause->SetTextColor("Quit", _pauseChoice == 1 ? kPicked : kWhite);
	_pause->Draw(proj);
	// The key that opened it is not taken again the frame it opens.
	if (!_pauseShown)
	{
		_pauseShown = true;
		return PauseResult::None;
	}
	if (pad.back || pad.start)
		return PauseResult::Continue;
	if (pad.confirm)
		return _pauseChoice == 0 ? PauseResult::Continue : PauseResult::Quit;
	return PauseResult::None;
}

void MiniGameMenu::OpenSummary(std::vector<Standing> standings)
{
	if (!_summary)
		return;
	standings.resize(SuperSprint::kRacers);
	int bestTime = -1, bestLap = -1;
	for (int row = 0; row < SuperSprint::kRacers; ++row)
	{
		const Standing& s = standings[row];
		if (s.player >= 0)
			_summary->SetTextIndex(fmt::format("Player{}", row), static_cast<std::size_t>(s.player));
		else
			_summary->SetText(fmt::format("Player{}", row), " ");
		_summary->SetText(fmt::format("NumWins{}", row), std::to_string(s.wins));
		_summary->SetText(fmt::format("NumPoints{}", row), std::to_string(s.points));
		_summary->SetText(fmt::format("TotalTime{}", row), s.place > 0 ? clock(s.totalTime) : Game::GetInstance().GetTextBible().Get("DNF"));
		_summary->SetText(fmt::format("BestLap{}", row), clock(s.bestLap));
		int character = 2;
		for (int c = 0; c < kCharacterCount; ++c)
			if (s.character == kCharacters[c])
				character = c;
		_summary->SetSpriteTexture(fmt::format("Face{}", row),
		                           image(fmt::format("{}_{}", kFaceLetters[character], kRowMoods[row])));
		if (s.place > 0 && (bestTime < 0 || s.totalTime < standings[bestTime].totalTime))
			bestTime = row;
		if (s.bestLap >= 0.0 && (bestLap < 0 || s.bestLap < standings[bestLap].bestLap))
			bestLap = row;
	}
	// The crowns, by the best of each column; the page has them on the
	// first row.
	const std::pair<const char*, int> crowns[] = {{"TotalTimeKingIcon", bestTime}, {"BestLapKingIcon", bestLap}};
	for (int i = 0; i < 2; ++i)
	{
		const auto& [icon, row] = crowns[i];
		const Box& box = _crownBoxes[i];
		_summary->SetSpriteVisible(icon, row >= 0);
		if (row >= 0)
			_summary->SetSpriteBox(icon, box.x, box.y + kSummaryRowStep * static_cast<float>(row), box.width, box.height);
	}
}

bool MiniGameMenu::DrawSummary(const Pad& pad, const Matrix4x4& proj)
{
	if (!_summary)
		return true;
	_summary->Draw(proj);
	return pad.confirm || pad.start || pad.back;
}

} // namespace Donut
