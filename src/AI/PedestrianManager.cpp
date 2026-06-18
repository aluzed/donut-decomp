#include "PedestrianManager.h"

#include "Core/Math/Matrix4x4.h"
#include "Physics/WorldPhysics.h"
#include "Render/SimpleMesh.h"
#include "Render/OpenGL/ShaderProgram.h"

#include <btBulletDynamicsCommon.h>
#include <cmath>

namespace Donut
{

PedestrianManager::PedestrianManager(WorldPhysics& physics)
    : _worldPhysics(physics), _rng(std::random_device{}())
{
}

PedestrianManager::~PedestrianManager() = default;

void PedestrianManager::Spawn(const Vector3& nearPosition, float radius)
{
	_pedestrians.reserve(_maxPedestrians);
	int count = static_cast<int>(_pedestrians.size());
	if (count >= _maxPedestrians) return;

	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
	std::uniform_real_distribution<float> radDist(0.0f, radius);

	float angle = angleDist(_rng);
	float r = radDist(_rng);
	Vector3 pos = nearPosition + Vector3(std::cos(angle) * r, 0, std::sin(angle) * r);
	pos.Y = 0.5f;

	Pedestrian p;
	p.position = pos;
	p.velocity = Vector3::Zero;
	p.targetDirection = pos + Vector3(std::cos(angleDist(_rng)), 0, std::sin(angleDist(_rng)));
	p.targetTimer = 2.0f + std::uniform_real_distribution<float>(0.0f, 3.0f)(_rng);
	p.active = true;
	_pedestrians.push_back(p);
}

void PedestrianManager::Update(double deltaTime, const Vector3& playerPosition)
{
	for (auto& p : _pedestrians)
	{
		if (!p.active) continue;

		p.targetTimer -= static_cast<float>(deltaTime);

		Vector3 toTarget = p.targetDirection - p.position;
		toTarget.Y = 0;
		float dist = toTarget.Length();

		if (p.targetTimer <= 0.0f || dist < 1.0f)
		{
			std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
			p.targetDirection = p.position + Vector3(std::cos(angleDist(_rng)) * 5.0f, 0, std::sin(angleDist(_rng)) * 5.0f);
			p.targetTimer = 2.0f + std::uniform_real_distribution<float>(0.0f, 3.0f)(_rng);
		}

		float speed = 1.2f;
		Vector3 desiredDir = toTarget.Normalized();
		if (desiredDir.LengthSquared() > 0.0f)
		{
			p.velocity = desiredDir * speed;
			p.position += p.velocity * static_cast<float>(deltaTime);
		}
	}

	despawnFar(_despawnRadius, playerPosition);
}

void PedestrianManager::Draw(GL::ShaderProgram& shader, SimpleMesh& mesh, const Matrix4x4& viewProj)
{
	for (const auto& p : _pedestrians)
	{
		if (!p.active) continue;
		mesh.Draw(shader,
			Matrix4x4::MakeTranslate(p.position),
			viewProj, Vector4(0.3f, 0.8f, 0.3f, 1.0f));
	}
}

void PedestrianManager::despawnFar(float despawnRadius, const Vector3& playerPosition)
{
	auto it = _pedestrians.begin();
	while (it != _pedestrians.end())
	{
		if (!it->active)
		{
			it = _pedestrians.erase(it);
			continue;
		}
		Vector3 d = it->position - playerPosition;
		d.Y = 0;
		if (d.LengthSquared() > despawnRadius * despawnRadius)
			it->active = false;
		++it;
	}
}

} // namespace Donut
