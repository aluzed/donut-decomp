// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/TrafficManager.h"
#include "Core/Log.h"
#include "Core/Math/Matrix4x4.h"
#include "Level.h"
#include "Render/OpenGL/ShaderProgram.h"

namespace Donut
{

TrafficManager::TrafficManager(Level& level): _level(level)
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
	}
}

void TrafficManager::Draw(const Matrix4x4& viewProj, GL::ShaderProgram& shader)
{
	for (const auto& car : _cars)
	{
		Matrix4x4 model = Matrix4x4::MakeTranslate(car.position);
		shader.SetUniformValue("viewProj", viewProj * model);
		shader.SetUniformValue("alphaMask", 0.0f);
	}
}

} // namespace Donut
