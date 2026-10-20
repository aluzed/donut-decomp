#include "GameHud.h"

#include "Core/FileSystem.h"
#include "FrontendProject.h"
#include "Game.h"
#include "Level.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/Mesh.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/SpriteBatch.h"
#include "Render/Texture.h"
#include "ResourceManager.h"
#include "UI/Hud.h"
#include "UI/ScrapBookMenu.h"
#include "UI/TextBible.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#include <fmt/format.h>

namespace Donut
{

namespace
{
constexpr const char* kIngame = "./art/frontend/scrooby2/ingame.p3d";
// How long the objective and the coins stay after they change.
constexpr double kObjectiveSeconds = 5.0, kCoinsSeconds = 4.0;
// The counters' letters, and the gap between their rows, in page pixels.
constexpr float kCounterHeight = 34.0f, kCounterStep = 40.0f;

// The radar's icons: each kind's pool of sprites on the page.
struct Pool
{
	const char* prefix;
	int count;
};
constexpr Pool kAICars {"IconAICar0_", 8}, kHarass {"IconHarass0_", 8}, kWaypoints {"IconWaypoint0_", 4},
    kCheckers {"IconChecker0_", 4}, kPhones {"PhoneBooth0_", 32}, kShops {"PurchaseCentre0_", 4},
    kBonus {"BonusMission0_", 2}, kStreetRaces {"StreetRace0_", 4}, kPlayerCars {"IconPlayerCar0_", 2},
    kCollectibles {"IconCollectible0_", 32}, kMissions {"IconMission0_", 32}, kWagers {"WagerRace0_", 2};
constexpr Pool kPools[] = {kAICars, kHarass, kWaypoints, kCheckers, kPhones, kShops,
                           kBonus, kStreetRaces, kPlayerCars, kCollectibles, kMissions, kWagers};

// What the game shows only now and then, hidden until it is.
constexpr const char* kHiddenSprites[] = {
    "RadarConeBox0", "RadarCone0", "HnRMeterBgd", "HnRMeter", "HnRSector0", "HnRSector1", "HnRSector2",
    "HnRSector3", "HnRSector4", "HnRSector5", "HnRSector6", "HnRSector7", "HitNRun", "MessageBox", "HitNRunTicket",
    "ActionButton", "CardCollected", "ItemsCount", "MissionFailed", "MissionComplete", "CountDown",
    "ProximityBarBgd", "ProximityBar", "ProximityMeter", "DamageBarBgd", "DamageBar", "DamageMeter", "Lap",
    "Position", "Collectibles", "ParTime", "Timer", "ObjectiveIcon", "walkietalkie", "TutorialMessage"};
constexpr const char* kHiddenTexts[] = {"SpeedUnit0", "Speed0_0", "Speed0_1", "Speed0_2", "Message", "HitNRun",
                                        "WaspsComplete", "CardTitle", "CardGet", "ItemsUnlocked", "ItemsComplete",
                                        "StageComplete", "Laps", "PositionOrdinal", "ItemDropped"};

// A letter's picture: letters/<digit>.png, letterA.png, colon.png...
Texture* letter(char c)
{
	static const std::map<char, const char*> kPunctuation = {{':', "colon"},    {'/', "slash"},  {'-', "hyphen"},
	                                                         {'!', "exclamation"}, {'.', "period"}, {',', "comma"},
	                                                         {'?', "question"}, {'$', "money"}};
	std::string name;
	if (std::isdigit(static_cast<unsigned char>(c)))
		name = std::string(1, c);
	else if (std::isalpha(static_cast<unsigned char>(c)))
		name = std::string("letter") + static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	else if (const auto found = kPunctuation.find(c); found != kPunctuation.end())
		name = found->second;
	else
		return nullptr;
	return Game::GetInstance().GetResourceManager().GetTexture(name + ".png");
}

// An objective's icon: msnicons/<kind>/<name>.png, whichever kind has it.
Texture* objectiveIcon(const std::string& name)
{
	if (name.empty())
		return nullptr;
	for (const char* kind : {"char", "location", "object", "vehicle"})
	{
		const std::string path = fmt::format("./art/frontend/dynaload/images/msnicons/{}/{}.png", kind, name);
		if (FileSystem::exists(path))
			return ScrapBookMenu::image(path);
	}
	return nullptr;
}
} // namespace

GameHud::GameHud()
{
	if (!FileSystem::exists(kIngame))
		return;
	_page = std::make_unique<FrontendProject>();
	_page->LoadP3D(kIngame, "Hud.pag");
	_page->SetTextsFitHeight(true);
	_coins = std::make_unique<FrontendProject>();
	_coins->LoadP3D(kIngame, "Coins.pag");
	_coins->SetSpriteVisible("NumCoins", false);
	const uint8_t white[] = {255, 255, 255, 255};
	_white = std::make_unique<Texture>(white, 1, 1, "hud_white");
	hideAll();
}

GameHud::~GameHud()
{
	if (_radarFrameBuffer)
		glDeleteFramebuffers(1, &_radarFrameBuffer);
}

void GameHud::LoadMap(int level)
{
	_map.clear();
	const std::string file = fmt::format("./art/frontend/scrooby2/resource/pure3d/l{}hudmap.p3d", level);
	if (!FileSystem::exists(file))
		return;
	const P3D::P3DFile p3d(file);
	auto& resources = Game::GetInstance().GetResourceManager();
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (chunk->IsType(P3D::ChunkType::Texture))
			resources.LoadTexture(*P3D::Texture::Load(*chunk));
		else if (chunk->IsType(P3D::ChunkType::Shader))
		{
			if (auto shader = P3D::Shader::Load(*chunk); !resources.HasShader(shader->GetName()))
				resources.LoadShader(*shader);
		}
		else if (chunk->IsType(P3D::ChunkType::Geometry))
		{
			auto mesh = std::make_unique<Mesh>(*P3D::Geometry::Load(*chunk));
			mesh->Commit();
			_map.push_back(std::move(mesh));
		}
	}
}

void GameHud::drawMap(const HudState& state)
{
	Level* level = Game::GetInstance().HasLevel() ? &Game::GetInstance().GetLevel() : nullptr;
	if (_map.empty() || !level)
		return;
	// The radar sprite's box, 152 page pixels, its disc the middle 120.
	constexpr int kSize = 256;
	const float discRadius = kSize * 60.0f / 152.0f;
	if (!_radarFrameBuffer)
	{
		_radarMap = std::make_unique<Texture>(nullptr, kSize, kSize, "hud_radar_map");
		glGenFramebuffers(1, &_radarFrameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, _radarFrameBuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _radarMap->GetOpenGLHandle(), 0);
	}
	GLint frameBuffer = 0, viewport[4] = {};
	GLfloat clearColour[4] = {};
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColour);
	glBindFramebuffer(GL_FRAMEBUFFER, _radarFrameBuffer);
	glViewport(0, 0, kSize, kSize);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// The roads: ahead up, the player in the middle, the radar's range to the
	// disc's rim. The page's matrices are row by row, the GL's column by
	// column: a point's x on the radar is the first column's dot with it.
	const float k = state.radarRange > 0.0f ? (discRadius / (kSize * 0.5f)) / state.radarRange : 0.0f;
	const float cs = std::cos(state.playerHeading), sn = std::sin(state.playerHeading);
	const float px = state.playerXZ.X, pz = state.playerXZ.Y;
	const Matrix4x4 viewProj(k * cs, k * sn, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -k * sn, k * cs, 0.0f, 0.0f,
	                         -k * (cs * px - sn * pz), -k * (sn * px + cs * pz), 0.0f, 1.0f);
	GL::ShaderProgram& shader = level->GetWorldShader();
	shader.Bind();
	shader.SetUniformValue("fogDensity", 0.0f);
	shader.SetUniformValue("lightmapIntensity", 0.0f);
	shader.SetUniformValue("viewProj", viewProj);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	for (const auto& mesh : _map)
		mesh->Draw(shader, true);
	for (const auto& mesh : _map)
		mesh->Draw(shader, false);

	// Round: what lies outside the disc cleared, two rows at a time.
	glEnable(GL_SCISSOR_TEST);
	for (int y = 0; y < kSize; y += 2)
	{
		const float dy = y + 1.0f - kSize * 0.5f;
		const float half = std::abs(dy) < discRadius ? std::sqrt(discRadius * discRadius - dy * dy) : 0.0f;
		const int left = static_cast<int>(kSize * 0.5f - half), right = static_cast<int>(std::ceil(kSize * 0.5f + half));
		glScissor(0, y, left, 2);
		glClear(GL_COLOR_BUFFER_BIT);
		glScissor(right, y, kSize - right, 2);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_SCISSOR_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(frameBuffer));
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glClearColor(clearColour[0], clearColour[1], clearColour[2], clearColour[3]);
	// Over the radar's own disc, under its icons: in the place of its cone, a
	// quad the page draws between them.
	float x, y, w, h;
	if (_page->GetSpriteBox("Radar0", x, y, w, h))
	{
		_page->SetSpriteTexture("RadarConeBox0", _radarMap.get());
		_page->SetSpriteColor("RadarConeBox0", Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		_page->SetSpriteBox("RadarConeBox0", x, y, w, h);
		_page->SetSpriteVisible("RadarConeBox0", true);
	}
}

void GameHud::hideAll()
{
	for (const char* sprite : kHiddenSprites)
		_page->SetSpriteVisible(sprite, false);
	for (const char* text : kHiddenTexts)
		_page->SetText(text, " ");
	for (const Pool& pool : kPools)
		for (int i = 0; i < pool.count; ++i)
			_page->SetSpriteVisible(fmt::format("{}{}", pool.prefix, i), false);
}

void GameHud::layOutRadar(const HudState& state)
{
	float x, y, w, h;
	if (!_page->GetSpriteBox("Radar0", x, y, w, h))
		return;
	// The map's circle, 120 of the frame's 152.
	const Vector2 centre(x + w * 0.5f, y + h * 0.5f);
	const float radius = w * 60.0f / 152.0f;
	const float scale = state.radarRange > 0.0f ? radius / state.radarRange : 0.0f;
	const float cs = std::cos(state.playerHeading), sn = std::sin(state.playerHeading);
	const auto place = [&](const std::string& sprite, const Vector2& world, bool edge) {
		float ix, iy, iw, ih;
		if (!_page->GetSpriteBox(sprite, ix, iy, iw, ih))
			return false;
		// Into the player's frame, ahead up (as the old radar's).
		const Vector2 rel = world - state.playerXZ;
		Vector2 at(rel.X * cs - rel.Y * sn, -(rel.X * sn + rel.Y * cs));
		at = at * scale;
		const float length = std::sqrt(at.X * at.X + at.Y * at.Y);
		if (length > radius)
		{
			if (!edge)
				return false;
			at = at * (radius / length);
		}
		_page->SetSpriteBox(sprite, centre.X + at.X - iw * 0.5f, centre.Y + at.Y - ih * 0.5f, iw, ih);
		_page->SetSpriteVisible(sprite, true);
		return true;
	};
	const auto fill = [&](const Pool& pool, const std::vector<Vector2>& where, bool edge) {
		int used = 0;
		for (const Vector2& world : where)
			if (used < pool.count && place(fmt::format("{}{}", pool.prefix, used), world, edge))
				++used;
	};
	fill(kAICars, state.missionBlips, false);
	fill(kHarass, state.policeBlips, true);
	fill(kPhones, state.phoneBooths, false);
	fill(kShops, state.shops, false);
	fill(kBonus, state.bonusMissions, false);
	fill(kStreetRaces, state.streetRaces, false);
	if (state.showPlayerCar)
		fill(kPlayerCars, {state.playerCarXZ}, true);
	// A race's checkpoints ahead as checkered flags; any other objective's
	// spot as the blue flag, kept on the edge when far.
	if (state.checkpointBlips.size() > 1)
		fill(kCheckers, state.checkpointBlips, false);
	if (state.hasNextCheckpoint)
		fill(kWaypoints, {state.nextCheckpointXZ}, true);
	_page->BringSpriteToFront("IconPlayer0");
}

void GameHud::drawLetters(SpriteBatch& batch, const FrontendProject& page, const std::string& text, Vector2 at,
                          float height, bool centred)
{
	// Each letter its own width at that height, the next a fifth of it in
	// (the pictures have room about them); a space half the height.
	constexpr float kAdvance = 0.8f;
	float width = 0.0f;
	for (const char c : text)
		if (const Texture* picture = letter(c))
			width += height * picture->GetWidth() / picture->GetHeight() * kAdvance;
		else
			width += height * 0.5f;
	if (centred)
		at = at - Vector2(width * 0.5f, height * 0.5f);
	const float scale = page.GetScale();
	for (const char c : text)
	{
		Texture* picture = letter(c);
		const float w = picture ? height * picture->GetWidth() / picture->GetHeight() : height * 0.5f;
		if (picture)
			batch.Draw(picture, page.ToScreen(at), Vector2(w, height) * scale, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		at.X += picture ? w * kAdvance : w;
	}
}

void GameHud::Draw(const HudState& state, const Matrix4x4& proj, double dt)
{
	if (!IsLoaded())
		return;
	hideAll();
	layOutRadar(state);
	drawMap(state);

	// Under the radar, the speed in the car: three digits, then the unit.
	if (state.inVehicle)
	{
		const int speed = std::clamp(static_cast<int>(std::lround(std::abs(state.speedKmh))), 0, 999);
		_page->SetTextIndex("Speed0_0", static_cast<std::size_t>(speed % 10));
		_page->SetTextIndex("Speed0_1", static_cast<std::size_t>(speed / 10 % 10));
		_page->SetTextIndex("Speed0_2", static_cast<std::size_t>(speed / 100));
		_page->SetTextIndex("SpeedUnit0", 0);
	}

	// Round the radar, the Hit & Run meter, when it has anything in it.
	if (state.chaseActive)
	{
		_page->SetSpriteVisible("HnRMeterBgd", true);
		_page->SetSpriteVisible("HitNRun", true);
	}

	// The objective, for a while after it changes: its box, its text, its icon.
	if (state.objective != _objective)
	{
		_objective = state.objective;
		_objectiveFor = _objective.empty() ? 0.0 : kObjectiveSeconds;
	}
	_objectiveFor = std::max(0.0, _objectiveFor - dt);
	if (_objectiveFor > 0.0 && state.missionActive)
	{
		_page->SetSpriteVisible("MessageBox", true);
		_page->SetTextWrapped("Message", _objective);
		if (Texture* icon = objectiveIcon(state.objectiveIcon))
		{
			_page->SetSpriteTexture("ObjectiveIcon", icon);
			_page->SetSpriteVisible("ObjectiveIcon", true);
		}
	}

	// The call-outs. Hit & Run, or busted: the ticket and its line.
	if (state.alertTerm == "HIT_AND_RUN" || state.alertTerm == "BUSTED")
	{
		_page->SetSpriteVisible("HitNRunTicket", true);
		_page->SetTextIndex("HitNRun", state.alertTerm == "BUSTED" ? 1 : 0);
	}
	// A card found: its picture, title and "you got a card"; the level's
	// cards all found.
	if (state.cardFound >= 0)
	{
		// The page names stubs (cards/db/_stubs/, blank); the cards are beside
		// them.
		if (Texture* card = ScrapBookMenu::image(
		        fmt::format("./art/frontend/scrooby2/resource/images/cards/db/card{:02}_t.png", state.cardFound)))
		{
			_page->SetSpriteTexture("CardCollected", card);
			_page->SetSpriteVisible("CardCollected", true);
		}
		_page->SetTextIndex("CardTitle", static_cast<std::size_t>(state.cardFound));
		_page->SetText("CardGet", "");
	}
	if (state.itemsDone == "CARD_LEVEL_COMPLETE" || state.itemsDone == "CARD_DECK_COMPLETE")
		_page->SetTextIndex("ItemsComplete", state.itemsDone == "CARD_DECK_COMPLETE" ? 1 : 0);
	else if (state.itemsDone == "WASPS_COMPLETE" || state.itemsDone == "GAGS_COMPLETE")
		_page->SetTextIndex("WaspsComplete", state.itemsDone == "GAGS_COMPLETE" ? 1 : 0);
	if (state.stageComplete)
		_page->SetText("StageComplete", "");

	// The counters, top left, one under the other: the stage's clock, the
	// place in a race, its laps, the car's damage.
	float x, y, w, h;
	_page->GetSpriteBox("Timer", x, y, w, h);
	Vector2 counter(x, y);
	std::vector<std::string> counters;
	if (state.missionActive && state.timeRemaining >= 0.0f)
	{
		const int seconds = static_cast<int>(std::ceil(state.timeRemaining));
		counters.push_back(fmt::format("{}:{:02}", seconds / 60, seconds % 60));
	}
	if (state.missionActive && state.raceField > 1)
	{
		static const char* const kOrdinals[] = {"TH", "ST", "ND", "RD"};
		const int p = state.racePosition;
		counters.push_back(fmt::format("{}{}", p, (p % 100 >= 11 && p % 100 <= 13) || p % 10 > 3 ? kOrdinals[0] : kOrdinals[p % 10]));
	}
	if (state.missionActive && state.totalLaps > 1)
		counters.push_back(fmt::format("LAP {}/{}", std::min(state.lap + 1, state.totalLaps), state.totalLaps));
	Vector2 damageAt = counter + Vector2(0.0f, kCounterStep * counters.size());
	// The car's damage, in a mission car, as the meter's grey bar.
	const bool damage = state.inVehicle && state.missionActive;
	if (damage)
	{
		float mx, my, mw, mh, bx, by, bw, bh;
		if (_page->GetSpriteBox("DamageMeter", mx, my, mw, mh) && _page->GetSpriteBox("DamageBar", bx, by, bw, bh))
		{
			const Vector2 shift = damageAt - Vector2(mx, my);
			_page->SetSpriteBox("DamageMeter", mx + shift.X, my + shift.Y, mw, mh);
			_page->SetSpriteBox("DamageBar", bx + shift.X, by + shift.Y, bw, bh);
			_page->SetSpriteFill("DamageBar", std::clamp(state.health / 100.0f, 0.0f, 1.0f));
			_page->SetSpriteVisible("DamageMeter", true);
			_page->SetSpriteVisible("DamageBar", true);
		}
	}

	_page->Draw(proj);

	SpriteBatch batch;
	// Without the level's map, the roads under the radar's icons, as dots.
	if (_map.empty() && _page->GetSpriteBox("Radar0", x, y, w, h))
	{
		const Vector2 centre(x + w * 0.5f, y + h * 0.5f);
		const float radius = w * 60.0f / 152.0f;
		const float scale = state.radarRange > 0.0f ? radius / state.radarRange : 0.0f;
		const float cs = std::cos(state.playerHeading), sn = std::sin(state.playerHeading);
		const float dot = 2.0f * _page->GetScale();
		for (const Vector2& road : state.roadPoints)
		{
			const Vector2 rel = road - state.playerXZ;
			const Vector2 at = Vector2(rel.X * cs - rel.Y * sn, -(rel.X * sn + rel.Y * cs)) * scale;
			if (at.X * at.X + at.Y * at.Y > radius * radius * 0.92f)
				continue;
			batch.Draw(_white.get(), _page->ToScreen(centre + at) - Vector2(dot * 0.5f, dot * 0.5f), Vector2(dot, dot),
			           Vector4(0.85f, 0.85f, 0.9f, 0.6f));
		}
	}
	// The Hit & Run meter's sectors, an eighth of the ring each, lit as it
	// fills.
	if (state.chaseActive && _page->GetSpriteBox("HnRSector0", x, y, w, h))
		if (Texture* sector = Game::GetInstance().GetResourceManager().GetTexture("hrsector.png"))
		{
			const int lit = std::clamp(static_cast<int>(std::ceil(state.heat / 10.0f * 8.0f)), 0, 8);
			for (int i = 0; i < lit; ++i)
				batch.Draw(sector, _page->ToScreen(Vector2(x, y)), Vector2(w, h) * _page->GetScale(), i * 45.0f,
				           Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}
	for (const std::string& text : counters)
	{
		drawLetters(batch, *_page, text, counter, kCounterHeight, false);
		counter.Y += kCounterStep;
	}
	// A count of wasp cameras or gags, where the card's count would be.
	if (!state.itemsCount.empty() && _page->GetSpriteBox("ItemsCount", x, y, w, h))
		drawLetters(batch, *_page, state.itemsCount, Vector2(x, y), kCounterHeight, false);
	// The countdown, large, in the middle.
	if (!state.countdown.empty() && _page->GetSpriteBox("CountDown", x, y, w, h))
	{
		std::string text = state.countdown;
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::toupper(c); });
		drawLetters(batch, *_page, text, Vector2(x + w * 0.5f, y + h * 0.5f), 64.0f, true);
	}
	// The coins, top right, for a while after they change.
	if (state.coins != _coinCount)
	{
		_coinsFor = _coinCount < 0 ? 0.0 : kCoinsSeconds;
		_coinCount = state.coins;
	}
	_coinsFor = std::max(0.0, _coinsFor - dt);
	if (_coinsFor > 0.0 && _coins->GetSpriteBox("NumCoins", x, y, w, h))
	{
		_coins->Draw(proj);
		drawLetters(batch, *_coins, fmt::format("${}", state.coins), Vector2(x, y), kCounterHeight, false);
	}
	batch.Flush(proj);
}

} // namespace Donut
