// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Donut
{

class Level;
class LineRenderer;
class WorldPhysics;
class PathGraph;
class SimpleMesh;
class BillboardBatch;
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
		// How long a car in its way has kept it stopped.
		float blocked = 0.0f;
		// How far its wheels have turned, radians.
		float wheelAngle = 0.0f;
	};

	TrafficManager(Level& level, LineRenderer& lineRenderer, const PathGraph& graph);
	~TrafficManager();

	void Update(double dt);
	void Draw();
	void DrawSolid(SimpleMesh& carMesh, GL::ShaderProgram& shader, const Matrix4x4& viewProj);
	// The cars that have a model, drawn with it (`worldShader`, as for mission
	// cars); DrawSolid draws the rest as boxes.
	void DrawModels(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj);
	// The brake lights of the cars held up (a car or the player in the way),
	// with the billboard program.
	void DrawBrakeLights(GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj);

	// Gives the cars the models a level script names for its traffic
	// (AddTrafficModel("minivanA", 2) ...), shared out by those weights.
	void LoadModels(const std::string& levelScript);

	const std::vector<TrafficCar>& GetCars() const { return _cars; }
	// Where the cars that are not traffic stand this frame (the player's, a
	// mission's). A traffic car stops short of one in its way: it is a
	// kinematic body, of unlimited mass, and drove through them, throwing them
	// tens of metres into the air.
	void SetObstacles(std::vector<Vector3> obstacles) { _obstacles = std::move(obstacles); }
	// The cars that have been kept waiting long enough to sound their horn
	// since the last call: the model's name ("compactA") and where it is.
	std::vector<std::pair<std::string, Vector3>> TakeHonks() { return std::exchange(_honks, {}); }

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
	std::vector<std::pair<std::string, Vector3>> _honks;
	std::vector<std::string> _modelNames;
	// Each model's wheel joints (w0..w3), -1 where it has none.
	std::vector<std::vector<int>> _wheelJoints;
	// Each model's ride height: its middle over the road, its wheels on it.
	std::vector<float> _rideHeights;
	// And the radius its wheels roll by.
	std::vector<float> _wheelRadii;
	// Each model's brake lights: the car file's brake billboards and their joints.
	std::vector<std::vector<std::pair<std::unique_ptr<BillboardBatch>, int>>> _brakeLights;
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
	// At most `count` cars drive (SetMaxTraffic); the others are parked out of
	// the world until the limit rises. Negative: all of them.
	void SetMaxCars(int count);

private:
	std::vector<TrafficCar> _parked;
};

} // namespace Donut
