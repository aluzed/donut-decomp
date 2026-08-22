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

	// Shortest route between the nodes nearest `from` and `to`, as world
	// positions. GetNextNode above is greedy best-first and gets trapped in local
	// minima; this is an A* over the whole graph. Empty when no route exists.
	std::vector<Vector3> FindRoute(const Vector3& from, const Vector3& to) const;
	int FindNearestNode(const Vector3& position) const;

private:
	std::vector<Node> _nodes;
};

} // namespace Donut
