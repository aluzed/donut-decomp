#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

namespace P3D
{
class P3DChunk;
}

// A billboard animation (an Animation of type "BQG"): one group per quad,
// named after it, with its width (WDT), height (HGT), colour (CLR) and
// visibility (VIS) over frames -- a siren's flashing, a wasp's signal ring
// pulsing, a shockwave growing and fading.
struct BillboardAnimation
{
	struct Quad
	{
		std::string name;
		std::vector<std::pair<float, float>> width, height;
		std::vector<std::pair<float, Vector4>> color;
		// Visible at frame 0 or not, and the frames it toggles at.
		bool startsVisible = true;
		std::vector<float> toggles;

		// At `frame`: the quad's size (`size` as it is when not animated), its
		// colour, whether it shows.
		Vector2 SizeAt(float frame, const Vector2& size) const;
		std::optional<Vector4> ColorAt(float frame) const;
		bool VisibleAt(float frame) const;
	};

	float frames = 0.0f;
	float frameRate = 30.0f;
	std::vector<Quad> quads;

	static std::optional<BillboardAnimation> FromChunk(const P3D::P3DChunk& animation);
};

} // namespace Donut
