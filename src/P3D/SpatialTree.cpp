#include "P3D/SpatialTree.h"

#include "Core/MemoryStream.h"
#include "P3D/P3DChunk.h"

#include <algorithm>
#include <functional>

namespace Donut::P3D
{

SpatialTree SpatialTree::Load(const P3DChunk& chunk)
{
	SpatialTree tree;
	MemoryStream header(chunk.GetData());
	header.Read<uint32_t>();
	tree.min = header.Read<Vector3>();
	tree.max = header.Read<Vector3>();
	for (const auto& child : chunk.GetChildren())
	{
		if (!child->IsType(ChunkType::TreeNode))
			continue;
		MemoryStream stream(child->GetData());
		Node node;
		node.descendants = stream.Read<uint32_t>();
		const int32_t back = stream.Read<int32_t>();
		node.parent = back == 0 ? -1 : static_cast<int>(tree.nodes.size()) + back;
		for (const auto& part : child->GetChildren())
		{
			if (!part->IsType(ChunkType::TreeNode2))
				continue;
			MemoryStream split(part->GetData());
			const uint8_t axis = split.Read<uint8_t>();
			node.axis = axis == 0xFF ? -1 : axis;
			node.plane = split.Read<float>();
			for (uint32_t& count : node.counts)
				count = split.Read<uint32_t>();
		}
		tree.nodes.push_back(node);
	}
	// Each node's children and box, from the root down: the first child right
	// after it, the second after the first's own.
	if (!tree.nodes.empty())
	{
		tree.nodes[0].min = tree.min;
		tree.nodes[0].max = tree.max;
	}
	for (std::size_t i = 0; i < tree.nodes.size(); ++i)
	{
		Node& node = tree.nodes[i];
		if (node.axis < 0 || i + 1 >= tree.nodes.size())
			continue;
		const std::size_t first = i + 1;
		const std::size_t second = first + tree.nodes[first].descendants + 1;
		node.children[0] = static_cast<int>(first);
		node.children[1] = second < tree.nodes.size() ? static_cast<int>(second) : -1;
		auto along = [&](Vector3& v) -> float& { return node.axis == 0 ? v.X : node.axis == 1 ? v.Y : v.Z; };
		Node& below = tree.nodes[first];
		below.min = node.min;
		below.max = node.max;
		along(below.max) = node.plane;
		if (node.children[1] >= 0)
		{
			Node& above = tree.nodes[second];
			above.min = node.min;
			above.max = node.max;
			along(above.min) = node.plane;
		}
	}
	return tree;
}

int SpatialTree::Depth() const
{
	std::function<int(int)> depth = [&](int index) -> int {
		if (index < 0)
			return 0;
		const Node& node = nodes[index];
		return 1 + std::max(depth(node.children[0]), depth(node.children[1]));
	};
	return nodes.empty() ? 0 : depth(0);
}

} // namespace Donut::P3D
