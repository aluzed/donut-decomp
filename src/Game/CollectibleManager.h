#pragma once

#include "Core/Math/Vector3.h"
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;
class Matrix4x4;
class Mesh;
namespace GL
{
class ShaderProgram;
}

class CollectibleManager
{
public:
	CollectibleManager(Level& level, LineRenderer& lineRenderer);

	void SpawnOnPath();
	void Update(const Vector3& playerPos, float collectRadius);
	// The coins as the game's spinning coin model, with the level's world shader;
	// as wire boxes when the model is not loaded.
	void Draw(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj, float time);
	int GetCollected() const { return _collected; }
	// Coins picked up since the last call, for the purse.
	int TakeNewlyCollected()
	{
		const int fresh = _unclaimed;
		_unclaimed = 0;
		return fresh;
	}
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
	int _unclaimed = 0;
	Mesh* _coinModel = nullptr; // the coin model, once committed
};

} // namespace Donut
