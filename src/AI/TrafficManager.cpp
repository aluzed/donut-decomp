// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/TrafficManager.h"
#include "Core/Log.h"
#include "Core/Math/Math.h"
#include "Level.h"
#include "Render/LineRenderer.h"

namespace Donut
{

TrafficManager::TrafficManager(Level& level, LineRenderer& lineRenderer)
    : _level(level), _lineRenderer(lineRenderer)
{
	const auto& paths = level.GetPaths();
	if (paths.empty())
	{
		Log::Warn("TrafficManager: no paths available");
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

	int maxCars = std::min(10, static_cast<int>(paths.size()) * 2);

	for (int i = 0; i < maxCars; ++i)
	{
		int pathIdx = i % paths.size();
		const auto& path = paths[pathIdx];
		if (path.points.size() < 2) continue;

		TrafficCar car;
		car.currentPath = pathIdx;
		car.currentPoint = rand() % path.points.size();
		car.speed = 6.0f + (rand() % 10) * 0.8f;
		car.color = colors[i % colors.size()];
		car.rotation = Quaternion::Identity;

		car.position = path.points[car.currentPoint];
		if (car.currentPoint > 0)
			car.position += Vector3((rand() % 100 - 50) * 0.02f, 0, (rand() % 100 - 50) * 0.02f);

		_cars.push_back(car);
	}

	Log::Info("TrafficManager: spawned {} cars on {} paths", _cars.size(), paths.size());
}

void TrafficManager::Update(double dt)
{
	const auto& paths = _level.GetPaths();

	for (auto& car : _cars)
	{
		if (car.currentPath >= static_cast<int>(paths.size())) continue;
		const auto& path = paths[car.currentPath];
		if (path.points.size() < 2) continue;

		int next = (car.currentPoint + 1) % path.points.size();
		Vector3 target = path.points[next];
		Vector3 dir = target - car.position;
		float dist = dir.Length();

		if (dist < 0.5f)
		{
			car.currentPoint = next;
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
