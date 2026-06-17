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
	};

	for (int i = 0; i < 5 && i < static_cast<int>(paths.size()); ++i)
	{
		if (paths[i].points.size() < 2) continue;

		TrafficCar car;
		car.position = paths[i].points[0];
		car.currentPath = i;
		car.currentPoint = 0;
		car.speed = 8.0f + (i * 2.0f);
		car.color = colors[i % colors.size()];
		car.rotation = Quaternion::Identity;
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
