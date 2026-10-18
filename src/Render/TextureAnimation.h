#pragma once

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

// A texture animation (an Animation chunk of type "TEX"): a shader drawn with
// one texture after another -- a character's eyes blinking (TEX_eyeball_m),
// a river's water (TEX_animated_water_m), an arcade game's screen.
struct TextureAnimation
{
	// The shader it retextures: the animation's name less "TEX_".
	std::string shader;
	float frameRate = 30.0f;
	float frameCount = 0.0f;
	// The texture from each key frame on.
	std::vector<std::pair<float, std::string>> keys;

	// Read from an Animation chunk; nothing when it is not a texture animation.
	static std::optional<TextureAnimation> FromChunk(const P3D::P3DChunk& animation);
	// The texture at `frame`.
	const std::string& TextureAt(float frame) const;
};

} // namespace Donut
