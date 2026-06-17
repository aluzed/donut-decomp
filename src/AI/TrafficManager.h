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
class PathGraph;
class SimpleMesh;
namespace GL { class ShaderProgram; }

class TrafficManager
{
public:
	TrafficManager(Level& level, LineRenderer& lineRenderer, const PathGraph& graph);

	void Update(double dt);
	void Draw();
	void DrawSolid(SimpleMesh& carMesh, GL::ShaderProgram& shader, const Matrix4x4& viewProj);

private:
	struct TrafficCar
	{
		Vector3 position;
		Quaternion rotation;
		float speed;
		float maxSpeed;
		Vector3 color;
		int currentNode;
		int targetNode;
	};

	Level& _level;
	LineRenderer& _lineRenderer;
	const PathGraph& _graph;
	std::vector<TrafficCar> _cars;
};

} // namespace Donut
