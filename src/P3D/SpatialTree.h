#pragma once

#include "Core/Math/Vector3.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Donut::P3D
{

class P3DChunk;

// A level's spatial tree (Tree, 0x3F00004, in its terrain file; the bonus game's
// tracks have one too): a kd-tree over the level, its node count and the box it
// spans, then its nodes
// in pre-order (TreeNode, 0x3F00005: how many nodes under it, its parent as an
// offset back), each with its split (TreeNode2, 0x3F00006: the axis, 0 X, 2 Z,
// 255 for a leaf, and where along it) and how many of each kind of the world's
// objects lie in it -- 8 counts: static entities, static physics, intersects,
// dynamic physics, fences, road segments, paths, animated objects. Level 1's:
// 3103 nodes, 1552 leaves; its fences counted 1373, as many as its files hold.
struct SpatialTree
{
	struct Node
	{
		int parent = -1;
		uint32_t descendants = 0;
		// -1 for a leaf.
		int axis = -1;
		float plane = 0.0f;
		std::array<uint32_t, 8> counts {};
		// Its children (below the plane, then above), -1 for a leaf.
		int children[2] = {-1, -1};
		// The box it spans: the tree's, split down to it.
		Vector3 min, max;
	};

	Vector3 min, max;
	std::vector<Node> nodes;

	static SpatialTree Load(const P3DChunk& chunk);
	int Depth() const;
};

} // namespace Donut::P3D
