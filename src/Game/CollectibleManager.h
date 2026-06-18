#pragma once

#include "Core/Math/Vector3.h"
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;

class CollectibleManager
{
public:
	CollectibleManager(Level& level, LineRenderer& lineRenderer);

	void SpawnOnPath();
	void Update(const Vector3& playerPos, float collectRadius);
	void Draw();
	int GetCollected() const { return _collected; }
	int GetTotalAvailable() const { return static_cast<int>(_positions.size()); }
	void Reset();
	bool AllCollected() const { return _collected >= static_cast<int>(_positions.size()); }

private:
	struct Coin
	{
		Vector3 position;
		bool collected;
	};

	Level& _level;
	LineRenderer& _lineRenderer;
	std::vector<Coin> _coins;
	std::vector<Vector3> _positions;
	int _collected = 0;
};

} // namespace Donut
