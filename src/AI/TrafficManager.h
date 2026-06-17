// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

#include <memory>
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;

class TrafficManager
{
public:
	TrafficManager(Level& level, LineRenderer& lineRenderer);

	void Update(double dt);
	void Draw();

private:
	struct TrafficCar
	{
		Vector3 position;
		Quaternion rotation;
		int currentPath;
		int currentPoint;
		float speed;
		Vector3 color;
	};

	Level& _level;
	LineRenderer& _lineRenderer;
	std::vector<TrafficCar> _cars;
};

} // namespace Donut
