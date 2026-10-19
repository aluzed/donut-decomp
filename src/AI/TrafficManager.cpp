// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AI/PathGraph.h"
#include "AI/TrafficManager.h"
#include "Core/Log.h"
#include "Core/Math/Math.h"
#include "Core/Math/Matrix4x4.h"
#include "Game.h"
#include "Level.h"
#include "Physics/WorldPhysics.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/BillboardBatch.h"
#include "Render/CompositeModel.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/LineRenderer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/SimpleMesh.h"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btDefaultMotionState.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace Donut
{

struct TrafficManager::Body
{
	btBoxShape shape {btVector3(0.9f, 0.7f, 2.2f)};
	btDefaultMotionState motion;
	std::unique_ptr<btRigidBody> rigid;
};

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

int TrafficManager::chooseNextNode(int from, int node) const
{
	const auto& nodes = _graph.GetNodes();
	const auto& here = nodes[node];
	Vector3 heading = from >= 0 ? here.position - nodes[from].position : Vector3::Zero;
	heading.Y = 0.0f;
	if (heading.LengthSquared() > 0.0001f)
		heading.Normalize();

	// Weigh each way on by how straight it carries on: a junction is turned
	// at now and then, a bend is simply followed.
	int best = -1;
	float bestScore = -1e9f;
	for (const int next : here.neighbors)
	{
		if (next == from || nodes[next].shortcut)
			continue;
		Vector3 way = nodes[next].position - here.position;
		way.Y = 0.0f;
		if (way.LengthSquared() > 0.0001f)
			way.Normalize();
		const float straight = way.X * heading.X + way.Z * heading.Z;
		const float score = straight + static_cast<float>(rand() % 100) * 0.008f;
		if (score > bestScore)
		{
			bestScore = score;
			best = next;
		}
	}
	return best >= 0 ? best : from;
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

	for (int i = 0, tries = 0; i < maxCars && tries < 1000; ++tries)
	{
		// On a road that goes somewhere, heading for one of its neighbours.
		TrafficCar car;
		car.currentNode = rand() % nodes.size();
		if (nodes[car.currentNode].shortcut || nodes[car.currentNode].neighbors.empty())
			continue;
		car.targetNode = chooseNextNode(-1, car.currentNode);
		if (car.targetNode < 0)
			continue;
		++i;

		car.speed = 6.0f + (rand() % 10) * 0.8f;
		car.maxSpeed = car.speed;
		car.color = colors[i % colors.size()];
		car.position = nodes[car.currentNode].position;
		const Vector3 way = nodes[car.targetNode].position - car.position;
		car.rotation = Quaternion::MakeFromEuler(Vector3(0.0f, std::atan2(way.X, way.Z), 0.0f));
		_cars.push_back(car);
	}

	Log::Info("TrafficManager: spawned {} cars on graph with {} nodes", _cars.size(), nodes.size());
}

void TrafficManager::Update(double dt)
{
	// Each car looks this far down its road and steers for that point. It used
	// to steer for the very next node and slow to a stop at each one; with the
	// nodes 2-13m apart, a node that came alongside the car was out of reach of
	// its turning circle, and it orbited the node for good.
	constexpr float kLookahead = 8.0f;
	const auto& nodes = _graph.GetNodes();

	for (auto& car : _cars)
	{
		if (car.currentNode < 0 || car.targetNode < 0)
			continue;

		for (int skip = 0; skip < 8 && (nodes[car.targetNode].position - car.position).Length() < kLookahead; ++skip)
		{
			const int next = chooseNextNode(car.currentNode, car.targetNode);
			if (next < 0)
				break;
			car.currentNode = car.targetNode;
			car.targetNode = next;
		}

		const Vector3 target = nodes[car.targetNode].position;
		const float steer = seekSteer(car.position, target, car.rotation, car.maxSpeed, 2.5f);

		float yaw = car.rotation.Yaw();
		yaw += steer * static_cast<float>(dt);
		car.rotation = Quaternion::MakeFromEuler(Vector3(0, yaw, 0));

		// Slower through the turns: down to a third of its speed for a point
		// off to the side or behind.
		Vector3 forward = car.rotation * Vector3::Forward;
		Vector3 toTarget = target - car.position;
		toTarget.Y = 0.0f;
		float facing = 1.0f;
		if (toTarget.LengthSquared() > 0.0001f)
		{
			toTarget.Normalize();
			facing = forward.X * toTarget.X + forward.Z * toTarget.Z;
		}
		float speed = car.maxSpeed * std::clamp(facing, 0.35f, 1.0f);
		// Stopped by a car in its way: within 9m ahead, and not over 2.5m to
		// either side.
		for (const auto& obstacle : _obstacles)
		{
			const Vector3 to = obstacle - car.position;
			const float along = to.X * forward.X + to.Z * forward.Z;
			const float side = std::abs(to.X * forward.Z - to.Z * forward.X);
			if (along > 0.0f && along < 9.0f && side < 2.5f && std::abs(to.Y) < 4.0f)
				speed = 0.0f;
		}
		// Kept waiting, the driver leans on the horn: after 1.5s, then every 5s.
		const float wasBlocked = car.blocked;
		car.blocked = speed == 0.0f ? car.blocked + static_cast<float>(dt) : 0.0f;
		if (car.model >= 0 && car.blocked >= 1.5f && std::fmod(wasBlocked + 3.5f, 5.0f) > std::fmod(car.blocked + 3.5f, 5.0f))
			_honks.emplace_back(_modelNames[car.model], car.grounded);
		car.position += forward * speed * static_cast<float>(dt);
		// The roads rise and fall; the car follows the height of its way.
		car.position.Y = target.Y;
	}
	updateBodies();
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

TrafficManager::~TrafficManager()
{
	// Out of the physics world before it goes (Game tears traffic down first).
	if (_physics && _present)
		for (auto& body : _bodies)
			_physics->GetDynamicsWorld()->removeRigidBody(body->rigid.get());
}

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
		// Its brake lights, brake1Shape..brake4Shape, where their joints are.
		std::vector<std::pair<std::unique_ptr<BillboardBatch>, int>> lights;
		const P3D::P3DFile file("art/cars/" + name + ".p3d");
		for (const auto& chunk : file.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::BillboardQuadGroup))
			{
				const auto group = P3D::BillboardQuadGroup::Load(*chunk);
				const int joint = model->FindPropJoint(group->GetName());
				if (group->GetName().rfind("brake", 0) == 0 && joint >= 0)
					lights.emplace_back(std::make_unique<BillboardBatch>(*group), joint);
			}
		_brakeLights.push_back(std::move(lights));
		_models.push_back(std::move(model));
		_modelNames.push_back(name);
		Log::Info("TrafficManager: traffic model '{}' (weight {})", name, weight);
	}

	if (pool.empty())
		return;
	for (std::size_t i = 0; i < _cars.size(); ++i)
		_cars[i].model = pool[i % pool.size()];
}

void TrafficManager::DrawModels(GL::ShaderProgram& worldShader, const Matrix4x4& viewProj)
{
	std::vector<Matrix4x4> transforms(_cars.size());
	for (std::size_t i = 0; i < _cars.size(); ++i)
		transforms[i] = Matrix4x4::MakeTranslate(_cars[i].grounded) * Matrix4x4(_cars[i].rotation);

	for (const bool opaque : {true, false})
		for (std::size_t i = 0; i < _cars.size(); ++i)
			if (_cars[i].model >= 0)
				_models[_cars[i].model]->Draw(worldShader, viewProj, transforms[i], opaque);
}

void TrafficManager::DrawBrakeLights(GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj)
{
	billboardShader.Bind();
	for (const bool opaque : {true, false})
		for (const auto& car : _cars)
		{
			if (car.model < 0 || car.blocked <= 0.0f)
				continue;
			const Matrix4x4 placed = Matrix4x4::MakeTranslate(car.grounded) * Matrix4x4(car.rotation);
			for (const auto& [batch, joint] : _brakeLights[car.model])
			{
				billboardShader.SetUniformValue("viewProj", viewProj * placed * _models[car.model]->GetJointTransform(joint));
				batch->Draw(billboardShader, opaque);
			}
		}
	billboardShader.SetUniformValue("viewProj", viewProj);
}

void TrafficManager::SetPresent(bool present)
{
	if (present == _present)
		return;
	_present = present;
	if (!_physics)
		return;
	for (auto& body : _bodies)
		if (present)
			_physics->GetDynamicsWorld()->addRigidBody(body->rigid.get());
		else
			_physics->GetDynamicsWorld()->removeRigidBody(body->rigid.get());
}

void TrafficManager::updateBodies()
{
	// A car model's origin is the middle of its chassis, which rides about this
	// high on its suspension. The traffic drives along the level's path loops,
	// whose points lie on the ground or under it: drawn there, a pickup was half
	// buried in the verge.
	constexpr float kRideHeight = 0.7f;
	WorldPhysics& physics = Game::GetInstance().GetWorldPhysics();
	if (_physics != &physics)
	{
		_bodies.clear();
		_physics = &physics;
	}

	for (std::size_t i = 0; i < _cars.size(); ++i)
	{
		auto& car = _cars[i];
		car.grounded = car.position;
		float groundY = 0.0f;
		if (physics.FindGroundHeight(car.position, 3.0f, 6.0f, groundY, true))
			car.grounded.Y = groundY + kRideHeight;

		if (i >= _bodies.size())
		{
			auto body = std::make_unique<Body>();
			btRigidBody::btRigidBodyConstructionInfo info(0.0f, &body->motion, &body->shape);
			body->rigid = std::make_unique<btRigidBody>(info);
			body->rigid->setCollisionFlags(body->rigid->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			body->rigid->setActivationState(DISABLE_DEACTIVATION);
			if (_present)
				physics.GetDynamicsWorld()->addRigidBody(body->rigid.get());
			_bodies.push_back(std::move(body));
		}
		const btTransform transform(btQuaternion(car.rotation.X, car.rotation.Y, car.rotation.Z, car.rotation.W),
		                            btVector3(car.grounded.X, car.grounded.Y, car.grounded.Z));
		_bodies[i]->motion.setWorldTransform(transform);
		_bodies[i]->rigid->setWorldTransform(transform);
	}
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
