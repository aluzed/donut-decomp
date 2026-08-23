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

	// Shortest route between the road nodes nearest `from` and `to`, as world
	// positions. GetNextNode above is greedy best-first and gets trapped in local
	// minima; this is an A*. Empty when no route exists.
	//
	// Only road nodes are routed through. The Path loops stitched into the graph
	// are traffic lanes around individual blocks -- they cut across forecourts and
	// verges, and A* took them as shortcuts because they are closer together than
	// road tiles, which put the race line into a wall. Traffic still steers by the
	// whole graph through GetNextNode.
	std::vector<Vector3> FindRoute(const Vector3& from, const Vector3& to) const;
	int FindNearestNode(const Vector3& position) const;
	int FindNearestRoadNode(const Vector3& position) const;
	bool IsRoadNode(int node) const { return node >= _firstRoadNode; }

	// Which connected component a node belongs to. Two nodes can only be routed
	// between when these match, so it is the first thing to look at when
	// FindRoute comes back empty or absurdly short.
	int GetComponent(int node) const;
	int GetComponentCount() const { return _componentCount; }

private:
	// Labels every node from `firstNode` up with the component it belongs to,
	// ignoring anything below it. Pass 0 for the whole graph, _firstRoadNode for
	// the road network alone.
	void computeComponents(int firstNode);
	// Links those components together, closest pair first, until one remains.
	void bridgeComponents(int firstNode);

	std::vector<Node> _nodes;
	// Index of the first junction/road-segment node. Everything below it comes
	// from a Path loop.
	int _firstRoadNode = 0;
	std::vector<int> _component;
	int _componentCount = 0;
};

} // namespace Donut
