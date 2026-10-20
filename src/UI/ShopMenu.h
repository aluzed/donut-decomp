#pragma once

#include "Core/Math/Matrix4x4.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class FrontendProject;
struct GameIntent;

// A shop (scrooby2's Rewards.pag): Gil's cars, or another seller's, or the
// clothes of an interior's wardrobe. One at a time between two arrows, its
// picture (where the game turned its 3D model), name and price; a car's stats
// as stars instead of its picture with up or down. Left and right go from one
// to the next, the confirm key buys it (or, had already, takes it), Escape
// leaves.
class ShopMenu
{
public:
	struct Item
	{
		std::string name;
		int price = 0;
		bool owned = false;
		// A car's speed, acceleration, toughness and stability, 0 to 5; a
		// skin has none.
		bool car = false;
		float stars[4] = {};
	};

	// `take` is given the item chosen and answers whether it could be had
	// (enough coins); the shop closes when it could.
	explicit ShopMenu(std::function<bool(const Item&)> take);
	~ShopMenu();

	bool IsLoaded() const { return _page != nullptr; }
	void Open(std::vector<Item> items);
	bool IsOpen() const { return _open; }
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

private:
	std::unique_ptr<FrontendProject> _page, _buy, _cancel;
	// Behind the page, in place of the game's 3D showroom: the game darkened.
	std::unique_ptr<class Texture> _shade;
	std::function<bool(const Item&)> _take;
	std::vector<Item> _items;
	std::size_t _shown = 0;
	bool _open = false, _ready = false, _stats = false;
	bool _leftHeld = false, _rightHeld = false, _upHeld = false, _downHeld = false;
	void layOut();
};

} // namespace Donut
