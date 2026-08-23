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

	// Which connected component a node belongs to. Two nodes can only be routed
	// between when these match, so it is the first thing to look at when
	// FindRoute comes back empty or absurdly short.
	int GetComponent(int node) const;
	int GetComponentCount() const { return _componentCount; }

private:
	// Labels every node with the connected component it belongs to.
	void computeComponents();
	// Links the components together, closest pair first, until one remains.
	void bridgeComponents();

	std::vector<Node> _nodes;
	std::vector<int> _component;
	int _componentCount = 0;
};

} // namespace Donut
