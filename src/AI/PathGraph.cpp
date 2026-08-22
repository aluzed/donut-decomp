// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/PathGraph.h"
#include "Core/Log.h"
#include "Level.h"

#include <algorithm>
#include <limits>
#include <queue>

namespace Donut
{

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

	Log::Info("PathGraph: built graph with {} nodes", _nodes.size());
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
