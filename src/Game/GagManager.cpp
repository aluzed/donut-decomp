#include "Game/GagManager.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Level.h"
#include "Render/CompositeModel.h"
#include "Render/Mesh.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/SceneSkins.h"
#include "Render/VertexAnimation.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

namespace Donut
{

void GagManager::Load(const std::string& levelDirectory, const Level& level, const std::set<std::string>& found)
{
	_levelDirectory = levelDirectory;
	_gags.clear();
	_movieGags.clear();
	_cardGags.clear();
	_primed = false;
	std::ifstream script(fmt::format("scripts/{}/level.mfk", levelDirectory));
	if (!script)
		return;
	std::optional<Gag> gag;
	std::string checkMovie, refusal;
	// GagCheckCollCards(char1, char2, accept, terms, refusal).
	std::vector<std::string> checkCards;
	int unplaced = 0;
	for (std::string line; std::getline(script, line);)
	{
		if (const std::size_t comment = line.find("//"); comment != std::string::npos)
			line.erase(comment);
		const std::size_t open = line.find('(');
		const std::size_t close = line.rfind(')');
		if (open == std::string::npos || close == std::string::npos || close < open)
			continue;
		std::string command = line.substr(0, open);
		command.erase(0, command.find_first_not_of(" \t"));
		command.erase(command.find_last_not_of(" \t") + 1);
		// The arguments, unquoted and trimmed.
		std::vector<std::string> args;
		std::stringstream list(line.substr(open + 1, close - open - 1));
		for (std::string arg; std::getline(list, arg, ',');)
		{
			arg.erase(std::remove(arg.begin(), arg.end(), '"'), arg.end());
			arg.erase(0, arg.find_first_not_of(" \t"));
			arg.erase(arg.find_last_not_of(" \t") + 1);
			args.push_back(arg);
		}

		if (command == "GagBegin")
		{
			gag.emplace();
			gag->model = args.empty() ? "" : args[0];
		}
		else if (!gag)
			continue;
		else if (command == "GagSetPosition" && args.size() == 1)
			gag->placed = level.GetLocatorPosition(args[0]);
		else if (command == "GagSetPosition" && args.size() == 3)
			gag->placed =
			    Vector3(static_cast<float>(std::atof(args[0].c_str())), static_cast<float>(std::atof(args[1].c_str())),
			            static_cast<float>(std::atof(args[2].c_str())));
		else if (command == "GagSetSound" && !args.empty())
			gag->sound = args[0];
		else if (command == "GagSetRandom" && !args.empty())
			gag->random = std::atoi(args[0].c_str()) != 0;
		else if (command == "GagSetCycle" && !args.empty())
			gag->cycle = args[0];
		else if (command == "GagSetCameraShake")
			gag->shakes = true;
		else if (command == "GagPlayFMV" && !args.empty())
			gag->movie = args[0];
		else if (command == "GagCheckMovie" && args.size() >= 4)
		{
			// (char1, char2, fmv, reject).
			checkMovie = args[2];
			refusal = args[3];
		}
		else if (command == "GagCheckCollCards" && args.size() >= 5)
			checkCards = args;
		else if (command == "GagSetSparkle" && !args.empty())
			gag->sparkle = std::atoi(args[0].c_str()) != 0;
		else if (command == "GagSetCoins" && !args.empty())
			gag->coins = std::atoi(args[0].c_str());
		else if (command == "GagSetInterior" && !args.empty())
			gag->interior = args[0];
		else if (command == "GagSetPersist" && !args.empty())
			gag->persists = std::atoi(args[0].c_str()) != 0;
		else if (command == "GagSetTrigger" && args.size() == 3)
		{
			// ("action", PickleTrig, 1.5): a locator of the level's.
			gag->byHand = args[0] == "action";
			gag->position = level.GetLocatorPosition(args[1]);
			gag->radius = static_cast<float>(std::atof(args[2].c_str()));
			if (gag->position == Vector3::Zero)
				gag->sound.clear(), ++unplaced;
		}
		else if (command == "GagSetTrigger" && args.size() == 5)
		{
			// ("touch", x, y, z, radius).
			gag->byHand = args[0] == "action";
			gag->position =
			    Vector3(static_cast<float>(std::atof(args[1].c_str())), static_cast<float>(std::atof(args[2].c_str())),
			            static_cast<float>(std::atof(args[3].c_str())));
			gag->radius = static_cast<float>(std::atof(args[4].c_str()));
		}
		else if (command == "GagEnd")
		{
			if (gag->sound.empty() && !checkMovie.empty() && gag->position != Vector3::Zero)
				_movieGags.push_back({gag->position, gag->radius, checkMovie, refusal});
			if (gag->sound.empty() && !checkCards.empty() && gag->position != Vector3::Zero)
				_cardGags.push_back(
				    {gag->position, gag->radius, gag->interior, checkCards[2], checkCards[3], checkCards[4], gag->sparkle});
			checkMovie.clear();
			refusal.clear();
			checkCards.clear();
			if (!gag->sound.empty())
			{
				// A model of its own: an animated one is posed gag by gag.
				const std::string file = "art/nis/gags/" + gag->model;
				if (gag->placed != Vector3::Zero && FileSystem::exists(file))
				{
					gag->drawable = CompositeModel::LoadP3D(file);
					gag->skins = SceneSkins::Load(file);
					gag->morph = VertexAnimation::FromFile(file);
				}
				gag->played = found.count(Id(*gag)) > 0;
				// GagSetPersist: one played once and left as it ended stands so
				// when the level is loaded again.
				gag->ended = gag->persists && gag->played && gag->cycle == "single";
				_gags.push_back(*gag);
			}
			gag.reset();
		}
	}
	Log::Info("Game: {} gag(s) in the level, {} found before, {} whose trigger locator is not loaded; {} with a movie, "
	          "{} card trade(s)",
	          _gags.size(), GetFound(), unplaced, _movieGags.size(), _cardGags.size());
}

std::string GagManager::Id(const Gag& gag) const
{
	return fmt::format("{} {} {:.0f} {:.0f}", _levelDirectory, gag.model, gag.position.X, gag.position.Z);
}

std::vector<std::string> GagManager::GetFoundIds() const
{
	std::vector<std::string> ids;
	for (const auto& gag : _gags)
		if (gag.played)
			ids.push_back(Id(gag));
	return ids;
}

int GagManager::GetFound() const
{
	return static_cast<int>(std::count_if(_gags.begin(), _gags.end(), [](const Gag& g) { return g.played; }));
}

bool GagManager::IsHere(const Gag& gag, const std::string& interior)
{
	// GagSetInterior: in that building only (the observatory's, Professor
	// Frink's), outdoors none of them.
	if (gag.interior.empty())
		return interior.empty();
	if (gag.interior.size() != interior.size())
		return false;
	for (std::size_t i = 0; i < interior.size(); ++i)
		if (std::tolower(static_cast<unsigned char>(gag.interior[i])) != std::tolower(static_cast<unsigned char>(interior[i])))
			return false;
	return true;
}

void GagManager::Update(const Vector3& player, const std::string& interior, double now, const std::function<void(Gag&)>& play)
{
	for (auto& gag : _gags)
	{
		if (gag.byHand || !IsHere(gag, interior))
			continue;
		const bool inside = (gag.position - player).Length() < gag.radius;
		// Once on the way in, and not again for a few seconds.
		if (_primed && inside && !gag.inside && now - gag.lastPlayed > 5.0)
		{
			// Of the random gags on this trigger, one plays; the others are
			// counted as walked into, so that they wait for the next time.
			std::vector<Gag*> shared;
			if (gag.random)
				for (auto& other : _gags)
					if (other.random && !other.byHand && (other.position - gag.position).Length() < 0.1f &&
					    std::abs(other.radius - gag.radius) < 0.01f)
					{
						shared.push_back(&other);
						other.inside = true;
					}
			Gag& picked = shared.empty() ? gag : *shared[static_cast<std::size_t>(rand()) % shared.size()];
			play(picked);
			for (Gag* other : shared) other->lastPlayed = now;
		}
		gag.inside = inside;
	}
	_primed = true;
}

GagManager::Gag* GagManager::FindByHand(const Vector3& player, const std::string& interior)
{
	for (auto& gag : _gags)
		if (gag.byHand && IsHere(gag, interior) && (gag.position - player).Length() < gag.radius + 1.0f)
			return &gag;
	return nullptr;
}

const GagManager::MovieGag* GagManager::FindMovieGag(const Vector3& player) const
{
	for (const auto& gag : _movieGags)
		if ((gag.position - player).Length() < gag.radius + 1.0f)
			return &gag;
	return nullptr;
}

const GagManager::CardGag* GagManager::FindCardGag(const Vector3& player, const std::string& interior) const
{
	for (const auto& gag : _cardGags)
	{
		Gag place;
		place.interior = gag.interior;
		if (IsHere(place, interior) && (gag.position - player).Length() < gag.radius + 1.0f)
			return &gag;
	}
	return nullptr;
}

std::vector<Vector3> GagManager::GetSparkles(const Vector3& player, const std::string& interior, float range,
                                             bool cardsTraded) const
{
	std::vector<Vector3> sparkles;
	for (const auto& gag : _gags)
		if (gag.sparkle && !gag.played && IsHere(gag, interior) && (gag.position - player).Length() < range)
			sparkles.push_back(gag.position);
	if (!cardsTraded)
		for (const auto& gag : _cardGags)
		{
			Gag place;
			place.interior = gag.interior;
			if (gag.sparkle && IsHere(place, interior) && (gag.position - player).Length() < range)
				sparkles.push_back(gag.position);
		}
	return sparkles;
}

void GagManager::Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& skinShader, const Matrix4x4& viewProjection,
                      const Vector3& camera, const std::string& interior, double now)
{
	for (auto& gag : _gags)
	{
		if (!gag.drawable || (gag.placed - camera).Length() > 150.0f || !IsHere(gag, interior))
			continue;
		// A scene with characters in it is seen only while it plays: Grandpa
		// walks in, says his piece and is gone.
		if (gag.skins && gag.drawable->IsAnimated())
		{
			const double played = (now - gag.animStartedAt) * gag.drawable->GetAnimationFrameRate();
			if (gag.animStartedAt < 0.0 || (gag.cycle != "cycle" && played >= gag.drawable->GetAnimationFrames()))
				continue;
		}
		if (gag.drawable->IsAnimated())
		{
			const float rate = gag.drawable->GetAnimationFrameRate();
			const float frames = gag.drawable->GetAnimationFrames();
			float frame = 0.0f;
			if (gag.cycle == "cycle")
				frame = static_cast<float>(now) * rate;
			else if (gag.ended && gag.animStartedAt < 0.0)
				frame = frames;
			else if (gag.animStartedAt >= 0.0)
			{
				frame = static_cast<float>(now - gag.animStartedAt) * rate;
				// Played through: "reset" goes back to its rest, the others
				// stay as they end.
				if (frame >= frames && gag.cycle == "reset")
				{
					gag.animStartedAt = -1.0;
					frame = 0.0f;
				}
			}
			gag.drawable->Pose(frame, gag.cycle == "cycle");
			if (gag.morph)
				if (Mesh* mesh = gag.drawable->FindMesh(gag.morph->mesh))
					for (std::size_t group = 0; group < gag.morph->groups.size(); ++group)
						if (const auto* offsets = gag.morph->KeyAt(group, frame))
							mesh->OffsetGroup(group, *offsets);
			if (gag.skins)
				gag.skins->Pose(frame, gag.cycle == "cycle");
		}
		const Matrix4x4 model = Matrix4x4::MakeTranslate(gag.placed);
		for (const bool opaque : {true, false}) gag.drawable->Draw(worldShader, viewProjection, model, opaque);
		if (gag.skins)
		{
			glDisable(GL_CULL_FACE); // skinned characters, as for the player
			gag.skins->Draw(viewProjection, model, skinShader);
		}
	}
}

} // namespace Donut
