#pragma once

#include "Core/Math/Vector3.h"
#include <set>
#include <string>
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;
struct Matrix4x4;
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
	// `count` coins spilled about `position` (a crate broken), to pick up.
	void DropCoins(const Vector3& position, int count);
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
	// The level's collector cards. Unlike coins, a card is picked up once for
	// good: those found are kept in donut_cards.dat.
	int GetCardsFound() const;
	// The level's wrenches (its "Wrench" action locators): driven over, one
	// repairs the car and is gone until the coins are laid out again. True
	// when one was just taken.
	bool TakeWrench(const Vector3& carPosition);
	int GetCardsTotal() const { return static_cast<int>(_cards.size()); }
	// The locator name of a card just found ("card13"), once; empty otherwise.
	std::string TakeFoundCard()
	{
		std::string found;
		found.swap(_justFound);
		return found;
	}
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

	struct Card
	{
		std::string id; // the locator's name and where it is: level 1 has two "card13"
		Vector3 position;
		bool found;
	};
	std::vector<Card> _cards;
	std::string _justFound;
	struct Wrench
	{
		Vector3 position;
		bool taken;
	};
	std::vector<Wrench> _wrenches;
	void loadCards();
	void layOutWrenches();
	void saveCards() const;
	// The resource manager's geometry `name`, committed on first use; null when
	// it is not loaded.
	Mesh* committed(const std::string& name);
	std::set<Mesh*> _committed;
};

} // namespace Donut
