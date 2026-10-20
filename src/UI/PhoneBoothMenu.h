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

// The phone booth's car select (scrooby2's PhoneBooth.pag): every car the game
// rewards, one at a time between two arrows, its picture, name and four stats
// as stars (speed, acceleration, toughness, stability); a car not had yet under
// a lock, with the level it comes from. Left and right go from car to car, the
// confirm key calls the one shown, Escape hangs up.
class PhoneBoothMenu
{
public:
	struct Car
	{
		std::string name;
		int level = 1;
		bool unlocked = false;
		// rewards.mfk's SetCarAttributes: speed, acceleration, toughness,
		// stability, 0 to 5 by halves.
		float stars[4] = {};
	};

	// `call` is given the car chosen.
	explicit PhoneBoothMenu(std::function<void(const std::string&)> call);
	~PhoneBoothMenu();

	bool IsLoaded() const { return _page != nullptr; }
	// Shows `cars`, from the one named `shown`.
	void Open(std::vector<Car> cars, const std::string& shown);
	bool IsOpen() const { return _open; }
	void Draw(const GameIntent& intent, const Matrix4x4& proj);

private:
	std::unique_ptr<FrontendProject> _page;
	std::function<void(const std::string&)> _call;
	std::vector<Car> _cars;
	std::size_t _shown = 0;
	bool _open = false;
	// Not the frame it opens: the key that opened it would act on it.
	bool _ready = false;
	bool _leftHeld = false, _rightHeld = false;
	// The picture's own box, which each car's is fitted into.
	float _previewX = 0.0f, _previewY = 0.0f, _previewWidth = 0.0f, _previewHeight = 0.0f;
	void layOut();
};

} // namespace Donut
