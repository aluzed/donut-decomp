#include "CollectibleManager.h"

#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Core/Math/Math.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector4.h"
#include "Game.h"
#include "Level.h"
#include "Render/LineRenderer.h"
#include "Render/Mesh.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "ResourceManager.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Log.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <random>

namespace Donut
{

CollectibleManager::CollectibleManager(Level& level, LineRenderer& lineRenderer)
    : _level(level), _lineRenderer(lineRenderer)
{
	loadCards();
}

void CollectibleManager::loadCards()
{
	std::set<std::string> found;
	std::ifstream file("donut_cards.dat");
	for (std::string id; std::getline(file, id);)
		if (!id.empty())
			found.insert(id);

	_cards.clear();
	for (const auto& card : _level.GetCards())
	{
		const std::string id = fmt::format("{} {:.0f} {:.0f}", card.name, card.position.X, card.position.Z);
		_cards.push_back({id, card.position + Vector3(0.0f, 0.5f, 0.0f), found.count(id) > 0});
	}
	Log::Info("CollectibleManager: {} cards, {} found", _cards.size(), GetCardsFound());
}

void CollectibleManager::saveCards() const
{
	// An unattended run teleports past cards; it has not found them.
	if (Game::GetInstance().IsAutopilot())
		return;
	std::ofstream file("donut_cards.dat");
	for (const auto& card : _cards)
		if (card.found)
			file << card.id << "\n";
}

void CollectibleManager::layOutWrenches()
{
	_wrenches.clear();
	for (const auto& trigger : _level.GetTriggers())
		if (trigger.action == "Wrench")
			_wrenches.push_back({trigger.position, false});
	Log::Info("CollectibleManager: {} wrenches", _wrenches.size());
}

bool CollectibleManager::TakeWrench(const Vector3& carPosition)
{
	for (auto& wrench : _wrenches)
		if (!wrench.taken && (wrench.position - carPosition).Length() < 4.0f)
		{
			wrench.taken = true;
			Game::GetInstance().GetAudioManager().PlayRaw(SoundGenerator::Chirp(400, 900, 0.3f, 44100), 44100, 1,
			                                              16);
			return true;
		}
	return false;
}

int CollectibleManager::GetCardsFound() const
{
	return static_cast<int>(std::count_if(_cards.begin(), _cards.end(), [](const Card& c) { return c.found; }));
}

void CollectibleManager::SpawnOnPath()
{
	layOutWrenches();

	// The level's own coins, where it has them; a scatter along the longest
	// path otherwise.
	const auto& placed = _level.GetCoinPositions();
	if (!placed.empty())
	{
		_positions.clear();
		_coins.clear();
		for (Vector3 pos : placed)
		{
			pos.Y += 0.5f;
			_positions.push_back(pos);
			_coins.push_back({pos, false});
		}
		_collected = 0;
		Log::Info("CollectibleManager: {} coins placed by the level", _coins.size());
		return;
	}

	const auto& paths = _level.GetPaths();
	if (paths.empty()) return;

	std::mt19937 rng(std::random_device{}());
	const auto* bestPath = &paths[0];
	for (size_t i = 1; i < paths.size(); ++i)
		if (paths[i].points.size() > bestPath->points.size())
			bestPath = &paths[i];

	const auto& pts = bestPath->points;
	if (pts.size() < 10) return;

	_positions.clear();
	for (size_t i = 0; i < pts.size(); i += 2)
	{
		Vector3 pos = pts[i];
		pos.Y += 1.5f;
		_positions.push_back(pos);
	}

	std::shuffle(_positions.begin(), _positions.end(), rng);
	if (_positions.size() > 30)
		_positions.resize(30);

	_coins.clear();
	for (const auto& p : _positions)
		_coins.push_back({p, false});
	_collected = 0;
}

void CollectibleManager::Update(const Vector3& playerPos, float collectRadius)
{
	for (auto& card : _cards)
	{
		if (card.found || (card.position - playerPos).Length() >= collectRadius)
			continue;
		card.found = true;
		_justFound = card.id.substr(0, card.id.find(' '));
		Log::Info("CollectibleManager: card '{}' found, {}/{}", card.id, GetCardsFound(), GetCardsTotal());
		Game::GetInstance().GetAudioManager().PlayRaw(
			SoundGenerator::Chirp(600, 1800, 0.4f, 44100), 44100, 1, 16);
		saveCards();
	}

	for (auto& coin : _coins)
	{
		if (coin.collected) continue;
		float dist = (coin.position - playerPos).Length();
		if (dist < collectRadius)
		{
			coin.collected = true;
			_collected++;
			_unclaimed++;
			Game::GetInstance().GetAudioManager().PlayRaw(
				SoundGenerator::Chirp(800, 1200, 0.15f, 44100), 44100, 1, 16);
		}
	}
}

void CollectibleManager::Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj, float time)
{
	// The card turns where it stands, as the game's does; a blue frame marks it
	// when its model (cards.p3d) is not there.
	const Quaternion cardSpin = Quaternion::MakeFromEuler(Vector3(0.0f, time * 2.0f, 0.0f));
	if (Mesh* card = committed("card11_geoShapeh"))
	{
		worldShader.Bind();
		for (const bool opaque : {true, false})
			for (const auto& c : _cards)
			{
				if (c.found)
					continue;
				worldShader.SetUniformValue("viewProj", viewProj * Matrix4x4::MakeTranslate(c.position) * Matrix4x4(cardSpin));
				card->Draw(worldShader, opaque);
			}
	}
	else
	{
		for (const auto& c : _cards)
			if (!c.found)
				_lineRenderer.DrawBox(c.position, cardSpin, Vector3(-0.3f, -0.45f, -0.02f), Vector3(0.3f, 0.45f, 0.02f),
				                      Vector4(0.4f, 0.7f, 1.0f, 1.0f));
	}

	// The wrench, turning above the road where it lies.
	const Quaternion wrenchSpin = Quaternion::MakeFromEuler(Vector3(0.0f, time * 2.5f, 0.0f));
	if (Mesh* wrench = committed("wrench7Shape"))
	{
		worldShader.Bind();
		for (const bool opaque : {true, false})
			for (const auto& w : _wrenches)
			{
				if (w.taken)
					continue;
				worldShader.SetUniformValue("viewProj", viewProj * Matrix4x4::MakeTranslate(w.position + Vector3(0.0f, 1.0f, 0.0f)) *
				                                            Matrix4x4(wrenchSpin));
				wrench->Draw(worldShader, opaque);
			}
	}
	else
	{
		for (const auto& w : _wrenches)
			if (!w.taken)
				_lineRenderer.DrawBox(w.position + Vector3(0.0f, 1.0f, 0.0f), wrenchSpin, Vector3(-0.15f, -0.5f, -0.05f),
				                      Vector3(0.15f, 0.5f, 0.05f), Vector4(1.0f, 0.5f, 0.1f, 1.0f));
	}

	// SetCoinDrawable names it in every level script; it lives in the mission
	// art's level.p3d.
	if (Mesh* coin = committed("coinShape_000"))
	{
		const Matrix4x4 spin(Quaternion::MakeFromEuler(Vector3(0.0f, time * 3.0f, 0.0f)));
		worldShader.Bind();
		for (const bool opaque : {true, false})
			for (const auto& c : _coins)
			{
				if (c.collected)
					continue;
				worldShader.SetUniformValue("viewProj", viewProj * Matrix4x4::MakeTranslate(c.position) * spin);
				coin->Draw(worldShader, opaque);
			}
		return;
	}

	for (const auto& coin : _coins)
	{
		if (coin.collected) continue;
		float bob = std::sin(static_cast<float>(coin.position.X + coin.position.Z) * 0.5f) * 0.3f;
		Vector3 pos = coin.position + Vector3(0, bob, 0);

		_lineRenderer.DrawBox(
			pos,
			Quaternion::Identity,
			Vector3(-0.2f, -0.2f, -0.2f),
			Vector3(0.2f, 0.2f, 0.2f),
			Vector4(1.0f, 0.84f, 0.0f, 1.0f));
	}
}

Mesh* CollectibleManager::committed(const std::string& name)
{
	// A geometry is only uploaded when it is first drawn: Mesh::Draw on one never
	// committed binds a null vertex binding.
	Mesh* mesh = Game::GetInstance().GetResourceManager().GetGeometry(name);
	if (mesh && _committed.insert(mesh).second)
		mesh->Commit();
	return mesh;
}

void CollectibleManager::Reset()
{
	_coins.clear();
	_positions.clear();
	_collected = 0;
}

} // namespace Donut
