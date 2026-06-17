// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"

#include <vector>

namespace Donut
{

class Level;

class PathGraph
{
public:
	struct Node
	{
		Vector3 position;
		std::vector<int> neighbors;
	};

	PathGraph(const Level& level);

	const std::vector<Node>& GetNodes() const { return _nodes; }

	Vector3 GetRandomNode() const;
	int GetNextNode(int current, int target) const;
	int FindNearestNode(const Vector3& position) const;

private:
	std::vector<Node> _nodes;
};

} // namespace Donut
