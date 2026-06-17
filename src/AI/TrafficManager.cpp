// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/TrafficManager.h"
#include "AI/PathGraph.h"
#include "Core/Log.h"
#include "Core/Math/Math.h"
#include "Level.h"
#include "Render/LineRenderer.h"

namespace Donut
{

TrafficManager::TrafficManager(Level& level, LineRenderer& lineRenderer, const PathGraph& graph)
    : _level(level), _lineRenderer(lineRenderer), _graph(graph)
{
	const auto& nodes = graph.GetNodes();
	if (nodes.empty())
	{
		Log::Warn("TrafficManager: no graph nodes available");
		return;
	}

	std::vector<Vector3> colors = {
		Vector3(1.0f, 0.2f, 0.2f),
		Vector3(0.2f, 1.0f, 0.2f),
		Vector3(0.2f, 0.2f, 1.0f),
		Vector3(1.0f, 1.0f, 0.2f),
		Vector3(1.0f, 0.5f, 0.0f),
		Vector3(0.5f, 0.0f, 0.5f),
		Vector3(0.0f, 0.8f, 0.8f),
		Vector3(0.8f, 0.8f, 0.8f),
	};

	int maxCars = std::min(12, static_cast<int>(nodes.size()));

	for (int i = 0; i < maxCars; ++i)
	{
		TrafficCar car;
		car.currentNode = rand() % nodes.size();
		car.targetNode = rand() % nodes.size();
		while (car.targetNode == car.currentNode && nodes.size() > 1)
			car.targetNode = rand() % nodes.size();

		car.speed = 6.0f + (rand() % 10) * 0.8f;
		car.color = colors[i % colors.size()];
		car.rotation = Quaternion::Identity;
		car.position = nodes[car.currentNode].position;
		_cars.push_back(car);
	}

	Log::Info("TrafficManager: spawned {} cars on graph with {} nodes", _cars.size(), nodes.size());
}

void TrafficManager::Update(double dt)
{
	const auto& nodes = _graph.GetNodes();

	for (auto& car : _cars)
	{
		if (car.currentNode < 0 || car.currentNode >= static_cast<int>(nodes.size()))
			continue;

		int nextNode = _graph.GetNextNode(car.currentNode, car.targetNode);
		if (nextNode < 0 || nextNode >= static_cast<int>(nodes.size()))
			continue;

		Vector3 target = nodes[nextNode].position;
		Vector3 dir = target - car.position;
		float dist = dir.Length();

		if (dist < 1.0f)
		{
			car.currentNode = nextNode;
			if (car.currentNode == car.targetNode)
				car.targetNode = rand() % nodes.size();
			continue;
		}

		dir.Normalize();
		car.position += dir * car.speed * static_cast<float>(dt);

		float yaw = atan2f(dir.X, dir.Z);
		car.rotation = Quaternion::MakeFromEuler(Vector3(0, yaw, 0));
	}
}

void TrafficManager::Draw()
{
	Vector3 boxMins(-0.9f, -0.3f, -2.0f);
	Vector3 boxMaxs(0.9f, 0.9f, 2.0f);

	for (const auto& car : _cars)
	{
		Vector4 col(car.color.X, car.color.Y, car.color.Z, 1.0f);
		_lineRenderer.DrawBox(car.position, car.rotation, boxMins, boxMaxs, col);
	}
}

} // namespace Donut
