#pragma once

#include "Core/Math/Vector3.h"
#include <memory>
#include <random>
#include <vector>

namespace Donut
{

class SimpleMesh;
class WorldPhysics;
namespace GL { class ShaderProgram; }

struct Pedestrian
{
	Vector3 position;
	Vector3 velocity;
	Vector3 targetDirection;
	float targetTimer;
	bool active;
};

class PedestrianManager
{
public:
	PedestrianManager(WorldPhysics& physics);
	~PedestrianManager();

	void Spawn(const Vector3& nearPosition, float radius);
	void Update(double deltaTime, const Vector3& playerPosition);
	void Draw(GL::ShaderProgram& shader, SimpleMesh& mesh, const class Matrix4x4& viewProj);

private:
	void despawnFar(float despawnRadius, const Vector3& playerPosition);

	WorldPhysics& _worldPhysics;
	std::vector<Pedestrian> _pedestrians;
	std::mt19937 _rng;
	int _maxPedestrians = 15;
	float _spawnRadius = 50.0f;
	float _despawnRadius = 80.0f;
};

} // namespace Donut
