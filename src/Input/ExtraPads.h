#pragma once

#include <array>
#include <cstdint>

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

namespace Donut
{

// The pads beyond the first, for the bonus game's players 2 to 4: the first
// is player 1's, with the keyboard, through the keymap (Input). These are
// read as they are, with a fixed layout: the left stick or the pad's cross
// steers and moves, the right trigger accelerates, the left one brakes, A
// confirms, B goes back, X is the aux button, Start joins and pauses, the
// right shoulder (or Y) fires a nitro.
class ExtraPads
{
public:
	static constexpr int kCount = 3;
	struct Pad
	{
		bool connected = false;
		float steer = 0.0f, gas = 0.0f, brake = 0.0f;
		// Held.
		bool left = false, right = false, up = false, down = false;
		bool a = false, b = false, x = false, start = false, nitro = false;
	};

	ExtraPads() = default;
	~ExtraPads();
	ExtraPads(const ExtraPads&) = delete;
	ExtraPads& operator=(const ExtraPads&) = delete;

	// Opens the pads plugged in since, closes those gone; the state of each.
	const std::array<Pad, kCount>& Poll();

private:
	std::array<SDL_GameController*, kCount> _pads {};
	std::array<Pad, kCount> _state {};
};

} // namespace Donut
