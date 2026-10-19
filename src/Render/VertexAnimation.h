#pragma once

#include "Core/Math/Vector3.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

// A vertex animation (an Animation of type "VRTX", over a mesh): per primitive
// group of the mesh, which key frame (VertexAnimKeyFrame: a list of offsets,
// one per vertex) it takes from each frame on. One gag has one: the blow-up
// doll inflating.
struct VertexAnimation
{
	// The mesh it moves: the animation's name less "VRTX_".
	std::string mesh;
	float frameRate = 30.0f;
	// Per primitive group, the key frame named from each frame on.
	std::vector<std::vector<std::pair<float, std::string>>> groups;
	std::map<std::string, std::vector<Vector3>> keys;

	// Read from a file's chunks; null when it has none.
	static std::unique_ptr<VertexAnimation> FromFile(const std::string& path);
	// The key frame group `group` takes at `frame`; null for none.
	const std::vector<Vector3>* KeyAt(std::size_t group, float frame) const;
};

} // namespace Donut
