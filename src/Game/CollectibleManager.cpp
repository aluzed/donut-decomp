#include "CollectibleManager.h"

#include "Audio/AudioManager.h"
#include "Audio/SoundGenerator.h"
#include "Core/Math/Math.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector4.h"
#include "Game.h"
#include "Level.h"
#include "Render/LineRenderer.h"

#include <algorithm>
#include <random>

namespace Donut
{

CollectibleManager::CollectibleManager(Level& level, LineRenderer& lineRenderer)
    : _level(level), _lineRenderer(lineRenderer)
{
}

void CollectibleManager::SpawnOnPath()
{
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
	for (auto& coin : _coins)
	{
		if (coin.collected) continue;
		float dist = (coin.position - playerPos).Length();
		if (dist < collectRadius)
		{
			coin.collected = true;
			_collected++;
			Game::GetInstance().GetAudioManager().PlayRaw(
				SoundGenerator::Chirp(800, 1200, 0.15f, 44100), 44100, 1, 16);
		}
	}
}

void CollectibleManager::Draw()
{
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

void CollectibleManager::Reset()
{
	_coins.clear();
	_positions.clear();
	_collected = 0;
}

} // namespace Donut
