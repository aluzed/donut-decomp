// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/PathGraph.h"
#include "Core/Log.h"
#include "Level.h"

#include <algorithm>
#include <limits>
#include <map>
#include <queue>

namespace Donut
{
namespace
{
// How close a path-loop node has to be to a junction to be linked to it.
constexpr float kJunctionSnapDistance = 70.0f;
} // namespace


PathGraph::PathGraph(const Level& level)
{
	const auto& paths = level.GetPaths();

	for (const auto& path : paths)
	{
		for (const auto& point : path.points)
		{
			Node node;
			node.position = point;
			_nodes.push_back(node);
		}
	}

	for (size_t i = 0; i < _nodes.size(); ++i)
	{
		int bestNeighbor = -1;
		float bestDist = 30.0f * 30.0f;

		for (size_t j = 0; j < _nodes.size(); ++j)
		{
			if (i == j) continue;
			float d = (_nodes[i].position - _nodes[j].position).LengthSquared();
			if (d < bestDist)
			{
				bestDist = d;
				bestNeighbor = static_cast<int>(j);
			}
		}

		if (bestNeighbor >= 0)
			_nodes[i].neighbors.push_back(bestNeighbor);

		if (i + 1 < _nodes.size())
		{
			float seqDist = (_nodes[i].position - _nodes[i + 1].position).LengthSquared();
			if (seqDist < 50.0f * 50.0f)
				_nodes[i].neighbors.push_back(static_cast<int>(i + 1));
		}
		if (i > 0)
		{
			float seqDist = (_nodes[i].position - _nodes[i - 1].position).LengthSquared();
			if (seqDist < 50.0f * 50.0f)
				_nodes[i].neighbors.push_back(static_cast<int>(i - 1));
		}
	}

	// The Path loops above are one closed loop per city block, and nothing joins
	// two of them: on their own they form 110 disconnected components, so no
	// route between neighbourhoods exists. The level's real road network does
	// connect -- 60 Intersection nodes joined by 99 Road links that name their
	// endpoints -- so splice it in and hang each loop off its nearest junction.
	const auto& intersections = level.GetIntersections();
	std::map<std::string, int> intersectionNodes;

	for (const auto& [name, position] : intersections)
	{
		Node node;
		node.position = position;
		intersectionNodes[name] = static_cast<int>(_nodes.size());
		_nodes.push_back(node);
	}

	int roadEdges = 0;
	for (const auto& road : level.GetRoads())
	{
		const auto a = intersectionNodes.find(road.start);
		const auto b = intersectionNodes.find(road.end);
		if (a == intersectionNodes.end() || b == intersectionNodes.end())
			continue;

		_nodes[a->second].neighbors.push_back(b->second);
		_nodes[b->second].neighbors.push_back(a->second);
		++roadEdges;
	}

	// Attach every path node to the junction it sits closest to, so the loops
	// become reachable from the road network rather than floating beside it.
	int stitched = 0;
	const size_t firstIntersection = _nodes.size() - intersections.size();
	for (size_t i = 0; i < firstIntersection; ++i)
	{
		int nearest = -1;
		float nearestDist = kJunctionSnapDistance * kJunctionSnapDistance;

		for (size_t j = firstIntersection; j < _nodes.size(); ++j)
		{
			const float d = (_nodes[i].position - _nodes[j].position).LengthSquared();
			if (d < nearestDist)
			{
				nearestDist = d;
				nearest = static_cast<int>(j);
			}
		}

		if (nearest < 0)
			continue;

		_nodes[i].neighbors.push_back(nearest);
		_nodes[nearest].neighbors.push_back(static_cast<int>(i));
		++stitched;
	}

	Log::Info("PathGraph: built graph with {} nodes ({} junctions, {} road links, {} path nodes stitched in)",
	          _nodes.size(), intersections.size(), roadEdges, stitched);

	// Connectivity is the thing that decides whether any agent can cross town, and
	// it is invisible otherwise: report it so a regression here is obvious.
	std::vector<bool> seen(_nodes.size(), false);
	std::size_t components = 0, largest = 0;
	for (std::size_t i = 0; i < _nodes.size(); ++i)
	{
		if (seen[i])
			continue;

		++components;
		std::size_t size = 0;
		std::vector<int> stack {static_cast<int>(i)};
		seen[i] = true;
		while (!stack.empty())
		{
			const int n = stack.back();
			stack.pop_back();
			++size;
			for (int m : _nodes[n].neighbors)
			{
				if (seen[m])
					continue;
				seen[m] = true;
				stack.push_back(m);
			}
		}
		largest = std::max(largest, size);
	}

	Log::Info("PathGraph: {} connected components, largest holds {} nodes ({}%)", components, largest,
	          _nodes.empty() ? 0 : (largest * 100 / _nodes.size()));
}

Vector3 PathGraph::GetRandomNode() const
{
	if (_nodes.empty()) return Vector3::Zero;
	return _nodes[rand() % _nodes.size()].position;
}

int PathGraph::FindNearestNode(const Vector3& position) const
{
	int best = -1;
	float bestDist = 1e10f;
	for (size_t i = 0; i < _nodes.size(); ++i)
	{
		float d = (_nodes[i].position - position).LengthSquared();
		if (d < bestDist)
		{
			bestDist = d;
			best = static_cast<int>(i);
		}
	}
	return best;
}

int PathGraph::GetNextNode(int current, int target) const
{
	if (current < 0 || current >= static_cast<int>(_nodes.size()))
		return -1;
	if (target < 0 || target >= static_cast<int>(_nodes.size()))
		return -1;

	const auto& node = _nodes[current];
	if (node.neighbors.empty())
		return target;

	int best = node.neighbors[0];
	float bestDist = (_nodes[best].position - _nodes[target].position).LengthSquared();

	for (size_t i = 1; i < node.neighbors.size(); ++i)
	{
		int n = node.neighbors[i];
		float d = (_nodes[n].position - _nodes[target].position).LengthSquared();
		if (d < bestDist)
		{
			bestDist = d;
			best = n;
		}
	}

	return best;
}

std::vector<Vector3> PathGraph::FindRoute(const Vector3& from, const Vector3& to) const
{
	const int start = FindNearestNode(from);
	const int goal = FindNearestNode(to);
	if (start < 0 || goal < 0)
		return {};
	if (start == goal)
		return {_nodes[goal].position};

	const auto heuristic = [&](int n) { return (_nodes[n].position - _nodes[goal].position).Length(); };

	std::vector<float> best(_nodes.size(), std::numeric_limits<float>::infinity());
	std::vector<int> cameFrom(_nodes.size(), -1);
	using Entry = std::pair<float, int>; // f-score, node
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

	best[start] = 0.0f;
	open.emplace(heuristic(start), start);

	while (!open.empty())
	{
		const int current = open.top().second;
		open.pop();

		if (current == goal)
			break;

		for (int n : _nodes[current].neighbors)
		{
			const float step = (_nodes[current].position - _nodes[n].position).Length();
			const float candidate = best[current] + step;
			if (candidate >= best[n])
				continue;

			best[n] = candidate;
			cameFrom[n] = current;
			open.emplace(candidate + heuristic(n), n);
		}
	}

	if (cameFrom[goal] < 0)
		return {};

	std::vector<Vector3> route;
	for (int n = goal; n >= 0; n = cameFrom[n])
		route.push_back(_nodes[n].position);
	std::reverse(route.begin(), route.end());
	return route;
}

} // namespace Donut
