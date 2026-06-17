// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Fwd.h"
#include "Core/Math/Fwd.h"
#include "Core/Math/Vector3.h"

#include <memory>
#include <vector>

namespace Donut
{

class Level;
namespace GL { class ShaderProgram; }

class TrafficManager
{
public:
	TrafficManager(Level& level);

	void Update(double dt);
	void Draw(const Matrix4x4& viewProj, GL::ShaderProgram& shader);

private:
	struct TrafficCar
	{
		Vector3 position;
		int currentPath;
		int currentPoint;
		float speed;
		Vector3 color;
	};

	Level& _level;
	std::vector<TrafficCar> _cars;
};

} // namespace Donut
