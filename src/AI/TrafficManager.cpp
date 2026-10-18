// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/PathGraph.h"
#include "AI/TrafficManager.h"
#include "Core/Log.h"
#include "Core/Math/Math.h"
#include "Core/Math/Matrix4x4.h"
#include "Game.h"
#include "Level.h"
#include "Physics/WorldPhysics.h"
#include "Render/CompositeModel.h"
#include "Render/LineRenderer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/SimpleMesh.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace Donut
{

static float seekSteer(const Vector3& position, const Vector3& target, const Quaternion& rotation,
                       float speed, float maxSteer)
{
	Vector3 desired = target - position;
	if (desired.LengthSquared() < 0.01f) return 0.0f;

	desired.Normalize();
	Vector3 forward = rotation * Vector3::Forward;
	float dot = forward.X * desired.X + forward.Z * desired.Z;
	// Positive when the target is to the right, i.e. at a greater yaw (yaw is
	// atan2(x, z), so turning from +Z towards +X increases it). It had the
	// opposite sign, which went unnoticed while the yaw itself was read from
	// Euler() in degrees: every car turned away from its next node.
	float cross = forward.Z * desired.X - forward.X * desired.Z;

	float steer = cross > 0.1f ? maxSteer : cross < -0.1f ? -maxSteer : cross * maxSteer;
	if (dot < 0.0f) steer = steer > 0 ? maxSteer : -maxSteer;

	return steer;
}

static float arrivalSpeed(const Vector3& position, const Vector3& target, float maxSpeed, float slowDist)
{
	float dist = (target - position).Length();
	if (dist < 0.5f) return 0.0f;
	if (dist < slowDist) return maxSpeed * (dist / slowDist);
	return maxSpeed;
}

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
		car.maxSpeed = car.speed;
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

		float steer = seekSteer(car.position, target, car.rotation, car.maxSpeed, 2.5f);
		float speed = arrivalSpeed(car.position, target, car.maxSpeed, 15.0f);

		float yaw = car.rotation.Yaw();
		yaw += steer * static_cast<float>(dt);
		car.rotation = Quaternion::MakeFromEuler(Vector3(0, yaw, 0));

		Vector3 forward = car.rotation * Vector3::Forward;
		car.position += forward * speed * static_cast<float>(dt);
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

TrafficManager::~TrafficManager() = default;

void TrafficManager::LoadModels(const std::string& levelScript)
{
	std::ifstream file(levelScript);
	if (!file.good())
		return;

	// Only the traffic lines: the rest of the level script sets up things --
	// a chase manager, the player's car -- that are not wanted from here.
	std::vector<int> pool;
	std::string line;
	while (std::getline(file, line))
	{
		const auto call = line.find("AddTrafficModel");
		if (call == std::string::npos || line.find("//") < call)
			continue;
		const auto open = line.find('"', call);
		const auto close = open == std::string::npos ? open : line.find('"', open + 1);
		if (close == std::string::npos)
			continue;
		const std::string name = line.substr(open + 1, close - open - 1);
		int weight = 1;
		if (const auto comma = line.find(',', close); comma != std::string::npos)
			weight = std::max(1, std::atoi(line.c_str() + comma + 1));

		auto model = CompositeModel::LoadP3D("art/cars/" + name + ".p3d");
		if (!model)
			continue;
		for (int i = 0; i < weight; ++i)
			pool.push_back(static_cast<int>(_models.size()));
		_models.push_back(std::move(model));
		Log::Info("TrafficManager: traffic model '{}' (weight {})", name, weight);
	}

	if (pool.empty())
		return;
	for (std::size_t i = 0; i < _cars.size(); ++i)
		_cars[i].model = pool[i % pool.size()];
}

void TrafficManager::DrawModels(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj)
{
	// A car model's origin is the middle of its chassis, as for the mission cars,
	// which ride about this high on their suspension. The traffic drives along
	// the level's path loops, whose points lie on the ground or under it: drawn
	// there, a pickup was half buried in the verge.
	constexpr float kRideHeight = 0.7f;
	const WorldPhysics& physics = Game::GetInstance().GetWorldPhysics();

	std::vector<Matrix4x4> transforms(_cars.size());
	for (std::size_t i = 0; i < _cars.size(); ++i)
	{
		Vector3 position = _cars[i].position;
		float groundY = 0.0f;
		if (physics.FindGroundHeight(position, 3.0f, 6.0f, groundY, true))
			position.Y = groundY + kRideHeight;
		transforms[i] = Matrix4x4::MakeTranslate(position) * Matrix4x4(_cars[i].rotation);
	}

	for (const bool opaque : {true, false})
		for (std::size_t i = 0; i < _cars.size(); ++i)
			if (_cars[i].model >= 0)
				_models[_cars[i].model]->Draw(worldShader, viewProj, transforms[i], opaque);
}

void TrafficManager::DrawSolid(SimpleMesh& carMesh, GL::ShaderProgram& shader, const Matrix4x4& viewProj)
{
	for (const auto& car : _cars)
	{
		if (car.model >= 0)
			continue;
		Vector4 col(car.color.X, car.color.Y, car.color.Z, 1.0f);
		carMesh.Draw(shader,
			Matrix4x4::MakeTranslate(car.position) * Matrix4x4(car.rotation),
			viewProj, col);
	}
}

} // namespace Donut
