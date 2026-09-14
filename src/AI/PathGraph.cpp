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
// How close a path-loop node has to be to a road node to be linked to it. The
// road chain has a node every ~8m, so a loop that runs beside a road has one
// within a few metres; 30m only bridges the gap where a loop leaves the road.
constexpr float kRoadSnapDistance = 30.0f;
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
	// connect -- Intersection nodes joined by Road links that name their
	// endpoints -- so splice it in and hang each loop off it.
	const size_t firstRoadNode = _nodes.size();
	_firstRoadNode = static_cast<int>(firstRoadNode);
	const auto& intersections = level.GetIntersections();
	std::map<std::string, int> intersectionNodes;

	for (const auto& [name, position] : intersections)
	{
		Node node;
		node.position = position;
		intersectionNodes[name] = static_cast<int>(_nodes.size());
		_nodes.push_back(node);
	}

	// Each Road is a chain of RoadSegments, not a straight line: walk it junction
	// -> segment -> segment -> ... -> junction so a route follows the road's shape.
	// Linking the junctions directly gave hops of up to 118m, and the race car
	// drove the first one straight into the side of a building.
	int roadEdges = 0, segmentNodes = 0, bareRoads = 0, longLinks = 0, longEndLinks = 0;
	float longestLink = 0.0f;
	const auto link = [&](int a, int b, bool junctionEnd) {
		_nodes[a].neighbors.push_back(b);
		_nodes[b].neighbors.push_back(a);

		const float length = (_nodes[a].position - _nodes[b].position).Length();
		if (length > longestLink)
			longestLink = length;
		if (length > 25.0f)
		{
			++longLinks;
			if (junctionEnd)
				++longEndLinks;
		}
	};

	for (const auto& road : level.GetRoads())
	{
		const auto a = intersectionNodes.find(road.start);
		const auto b = intersectionNodes.find(road.end);
		if (a == intersectionNodes.end() || b == intersectionNodes.end())
			continue;

		if (road.points.empty())
			++bareRoads;

		// The RoadSegment children are not stored in order along the road -- taking
		// them as they come gave 288 links over 25m and one of 239m, and the race
		// route zig-zagged back and forth across the street. Walk them
		// nearest-first from the start junction instead, which is the order a car
		// drives them in.
		std::vector<Vector3> ordered;
		ordered.reserve(road.points.size());
		std::vector<bool> used(road.points.size(), false);
		Vector3 from = _nodes[a->second].position;
		for (std::size_t n = 0; n < road.points.size(); ++n)
		{
			int nearest = -1;
			float nearestDist = std::numeric_limits<float>::infinity();
			for (std::size_t k = 0; k < road.points.size(); ++k)
			{
				if (used[k])
					continue;
				const float d = (road.points[k] - from).LengthSquared();
				if (d < nearestDist)
				{
					nearestDist = d;
					nearest = static_cast<int>(k);
				}
			}

			used[nearest] = true;
			from = road.points[nearest];
			ordered.push_back(from);
		}

		int previous = a->second;
		for (const auto& point : ordered)
		{
			Node node;
			node.position = point;
			const int index = static_cast<int>(_nodes.size());
			_nodes.push_back(node);
			++segmentNodes;

			link(previous, index, previous == a->second);
			previous = index;
		}

		link(previous, b->second, true);
		++roadEdges;
	}

	// Attach every path node to the road node it sits closest to, so the loops
	// become reachable from the road network rather than floating beside it.
	int stitched = 0;
	for (size_t i = 0; i < firstRoadNode; ++i)
	{
		int nearest = -1;
		float nearestDist = kRoadSnapDistance * kRoadSnapDistance;

		for (size_t j = firstRoadNode; j < _nodes.size(); ++j)
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

		link(static_cast<int>(i), nearest, true);
		++stitched;
	}

	Log::Info("PathGraph: built graph with {} nodes ({} junctions, {} roads, {} road segments, {} path nodes stitched in)",
	          _nodes.size(), intersections.size(), roadEdges, segmentNodes, stitched);
	// Most of these are honest: a straight street is tiled with a few long slabs
	// rather than many short ones, so consecutive slab centres are far apart and
	// the line between them still runs down the road.
	Log::Info("PathGraph: {} links longer than 25m ({} of them onto a junction or a stitched path node), longest {:.0f}m",
	          longLinks, longEndLinks, longestLink);
	if (level.GetUnresolvedRoadTiles() > 0)
		Log::Warn("PathGraph: {} road segments name a RoadDataSegment that never loaded", level.GetUnresolvedRoadTiles());
	if (bareRoads > 0)
		Log::Warn("PathGraph: {} of {} roads carry no RoadSegment children, so they are straight junction-to-junction "
		          "edges that cut across whatever lies between",
		          bareRoads, roadEdges);

	// FindRoute only travels road nodes, so the road network has to be connected
	// on its own before anything else: bridge it first, among road nodes only, so
	// no route is ever completed through somebody's front garden.
	bridgeComponents(_firstRoadNode);
	computeComponents(_firstRoadNode);
	Log::Info("PathGraph: road network is {} connected component(s)", _componentCount);

	// Then the graph as a whole. Snapping to the road still leaves islands --
	// loops further than the snap distance from any road -- and an island is
	// unroutable, silently: GetNextNode just circles it forever.
	bridgeComponents(0);

	computeComponents(0);
	std::vector<std::size_t> sizes(_componentCount, 0);
	for (int c : _component)
		++sizes[c];
	const std::size_t largest = sizes.empty() ? 0 : *std::max_element(sizes.begin(), sizes.end());

	// Connectivity decides whether any agent can cross town, and it is invisible
	// otherwise: report it so a regression here is obvious.
	Log::Info("PathGraph: {} connected components, largest holds {} nodes ({}%)", _componentCount, largest,
	          _nodes.empty() ? 0 : (largest * 100 / _nodes.size()));
}

void PathGraph::computeComponents(int firstNode)
{
	_component.assign(_nodes.size(), -1);
	_componentCount = 0;

	for (std::size_t i = firstNode; i < _nodes.size(); ++i)
	{
		if (_component[i] >= 0)
			continue;

		const int id = _componentCount++;
		std::vector<int> stack {static_cast<int>(i)};
		_component[i] = id;
		while (!stack.empty())
		{
			const int n = stack.back();
			stack.pop_back();
			for (int m : _nodes[n].neighbors)
			{
				if (m < firstNode || _component[m] >= 0)
					continue;
				_component[m] = id;
				stack.push_back(m);
			}
		}
	}
}

void PathGraph::bridgeComponents(int firstNode)
{
	// Boruvka-style: every round, each component finds the nearest node in any
	// other component and links to it, which at least halves the component count.
	// Closest pair first means the bridges follow the gaps the level actually
	// leaves between blocks rather than cutting across town.
	for (int round = 0; round < 32; ++round)
	{
		computeComponents(firstNode);
		if (_componentCount <= 1)
			break;

		struct Bridge
		{
			float dist = std::numeric_limits<float>::infinity();
			int from = -1, to = -1;
		};
		std::vector<Bridge> best(_componentCount);

		for (std::size_t i = firstNode; i < _nodes.size(); ++i)
		{
			for (std::size_t j = i + 1; j < _nodes.size(); ++j)
			{
				const int ci = _component[i], cj = _component[j];
				if (ci == cj)
					continue;

				const float d = (_nodes[i].position - _nodes[j].position).LengthSquared();
				if (d < best[ci].dist)
					best[ci] = {d, static_cast<int>(i), static_cast<int>(j)};
				if (d < best[cj].dist)
					best[cj] = {d, static_cast<int>(j), static_cast<int>(i)};
			}
		}

		int added = 0;
		for (const auto& bridge : best)
		{
			if (bridge.from < 0)
				continue;

			auto& neighbors = _nodes[bridge.from].neighbors;
			if (std::find(neighbors.begin(), neighbors.end(), bridge.to) != neighbors.end())
				continue;

			neighbors.push_back(bridge.to);
			_nodes[bridge.to].neighbors.push_back(bridge.from);
			++added;

			// A long bridge is a straight line drawn across whatever is in the way,
			// so it is worth seeing: an agent routed over one will drive through it.
			Log::Debug("PathGraph: bridged components over {:.1f}m, nodes {} -> {}", std::sqrt(bridge.dist),
			           bridge.from, bridge.to);
		}

		if (added == 0)
			break;
	}
}

int PathGraph::GetComponent(int node) const
{
	if (node < 0 || node >= static_cast<int>(_component.size()))
		return -1;
	return _component[node];
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

int PathGraph::FindNearestRoadNode(const Vector3& position) const
{
	int best = -1;
	float bestDist = 1e10f;
	for (size_t i = _firstRoadNode; i < _nodes.size(); ++i)
	{
		const float d = (_nodes[i].position - position).LengthSquared();
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
	const int start = FindNearestRoadNode(from);
	const int goal = FindNearestRoadNode(to);
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
			if (!IsRoadNode(n))
				continue;

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
