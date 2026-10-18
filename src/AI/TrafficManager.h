// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

#include <memory>
#include <string>
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;
class WorldPhysics;
class PathGraph;
class SimpleMesh;
class CompositeModel;
namespace GL { class ShaderProgram; }

class TrafficManager
{
public:
	struct TrafficCar
	{
		Vector3 position;
		Quaternion rotation;
		float speed;
		float maxSpeed;
		Vector3 color;
		// The road node last passed, and the one being driven to.
		int currentNode;
		int targetNode;
		// Index into the traffic models, or -1 to draw the placeholder box.
		int model = -1;
		// Where it is drawn and collides: on the ground under `position`.
		Vector3 grounded;
	};

	TrafficManager(Level& level, LineRenderer& lineRenderer, const PathGraph& graph);
	~TrafficManager();

	void Update(double dt);
	void Draw();
	void DrawSolid(SimpleMesh& carMesh, GL::ShaderProgram& shader, const Matrix4x4& viewProj);
	// The cars that have a model, drawn with it (`worldShader`, as for mission
	// cars); DrawSolid draws the rest as boxes.
	void DrawModels(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj);

	// Gives the cars the models a level script names for its traffic
	// (AddTrafficModel("minivanA", 2) ...), shared out by those weights.
	void LoadModels(const std::string& levelScript);

	const std::vector<TrafficCar>& GetCars() const { return _cars; }
	// Where the cars that are not traffic stand this frame (the player's, a
	// mission's). A traffic car stops short of one in its way: it is a
	// kinematic body, of unlimited mass, and drove through them, throwing them
	// tens of metres into the air.
	void SetObstacles(std::vector<Vector3> obstacles) { _obstacles = std::move(obstacles); }

private:
	// The way on from `node`, coming from `from`: a neighbour other than the
	// one just left, the straighter the likelier; back the way it came only at
	// a dead end. -1 when the node leads nowhere.
	int chooseNextNode(int from, int node) const;

	Level& _level;
	LineRenderer& _lineRenderer;
	const PathGraph& _graph;
	std::vector<TrafficCar> _cars;
	std::vector<Vector3> _obstacles;
	std::vector<std::unique_ptr<CompositeModel>> _models;

	// Each car is a kinematic box in the physics world, moved with it every
	// frame, so that the player's car hits it rather than driving through.
	struct Body;
	std::vector<std::unique_ptr<Body>> _bodies;
	WorldPhysics* _physics = nullptr;
	void updateBodies();
	bool _present = true;

public:
	// A stage with no traffic: its cars are taken out of the physics world, or
	// they would stand in the empty streets, unseen.
	void SetPresent(bool present);
};

} // namespace Donut
