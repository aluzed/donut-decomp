#include "PedestrianManager.h"

#include "Character.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"

#include "Core/Math/Matrix4x4.h"
#include "Physics/WorldPhysics.h"
#include "Render/SimpleMesh.h"
#include "Render/OpenGL/ShaderProgram.h"

#include <btBulletDynamicsCommon.h>
#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <fstream>

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
	// At the player's height to start with, then stood on the ground in Update:
	// a fixed 0.5m put them under every street that is not at sea level.
	Vector3 pos = nearPosition + Vector3(std::cos(angle) * r, 0, std::sin(angle) * r);

	Pedestrian p;
	p.position = pos;
	p.velocity = Vector3::Zero;
	p.targetDirection = pos + Vector3(std::cos(angleDist(_rng)), 0, std::sin(angleDist(_rng)));
	p.targetTimer = 2.0f + std::uniform_real_distribution<float>(0.0f, 3.0f)(_rng);
	p.active = true;
	for (std::size_t i = 0; i < _bodyInUse.size(); ++i)
		if (!_bodyInUse[i])
		{
			_bodyInUse[i] = true;
			p.body = static_cast<int>(i);
			// A body used before may have last been sent flying.
			playOn(*_bodies[i], "_loco_walk", true);
			break;
		}
	_pedestrians.push_back(p);
}

void PedestrianManager::Update(double deltaTime, const Vector3& playerPosition)
{
	for (auto& p : _pedestrians)
	{
		if (!p.active) continue;

		if (p.flying >= 0.0f)
		{
			// Through the air, turning over, for a moment; then off the street.
			p.flying += static_cast<float>(deltaTime);
			p.velocity.Y -= 9.8f * static_cast<float>(deltaTime);
			p.position += p.velocity * static_cast<float>(deltaTime);
			if (p.flying > 1.6f)
				p.active = false;
			if (p.body >= 0)
			{
				Character& body = *_bodies[p.body];
				body.SetPosition(p.position);
				body.SetRotation(Quaternion::MakeFromEuler(
				    Vector3(p.flying * 9.0f, std::atan2(p.velocity.X, p.velocity.Z) + 3.14159265f, 0.0f)));
				body.Update(deltaTime);
			}
			continue;
		}

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

		// On the ground, not at a fixed 0.5m: Springfield has hills, and a
		// pedestrian drawn as a person shows it.
		float groundY = 0.0f;
		if (_worldPhysics.FindGroundHeight(p.position, 4.0f, 8.0f, groundY, true))
			p.position.Y = groundY;

		if (p.body >= 0)
		{
			Character& body = *_bodies[p.body];
			body.SetPosition(p.position);
			if (p.velocity.LengthSquared() > 0.01f)
				// Half a turn on: a character model faces -Z, as the player's does.
				body.SetRotation(Quaternion::MakeFromEuler(
				    Vector3(0.0f, std::atan2(p.velocity.X, p.velocity.Z) + 3.14159265f, 0.0f)));
			body.Update(deltaTime);
		}
	}

	despawnFar(_despawnRadius, playerPosition);
}

void PedestrianManager::Draw(GL::ShaderProgram& shader, SimpleMesh& mesh, const Matrix4x4& viewProj)
{
	for (const auto& p : _pedestrians)
	{
		if (!p.active || p.body >= 0) continue;
		mesh.Draw(shader,
			Matrix4x4::MakeTranslate(p.position),
			viewProj, Vector4(0.3f, 0.8f, 0.3f, 1.0f));
	}
}

void PedestrianManager::LoadModels(const std::string& levelScript)
{
	std::ifstream file(levelScript);
	if (!file.good())
		return;

	std::vector<std::string> people;
	std::string line;
	while (std::getline(file, line))
	{
		const auto call = line.find("AddPed");
		if (call == std::string::npos || line.find("//") < call)
			continue;
		const auto open = line.find('"', call);
		const auto close = open == std::string::npos ? open : line.find('"', open + 1);
		if (close == std::string::npos)
			continue;
		const std::string name = line.substr(open + 1, close - open - 1);
		if (FileSystem::exists(fmt::format("art/chars/{}_m.p3d", name)) &&
		    std::find(people.begin(), people.end(), name) == people.end())
			people.push_back(name);
	}
	if (people.empty())
		return;

	for (int i = 0; i < _maxPedestrians; ++i)
	{
		const std::string& name = people[static_cast<std::size_t>(i) % people.size()];
		auto body = std::make_unique<Character>(name);
		body->LoadModel(name);
		body->LoadAnimations(name);
		// None of the pedestrian models extracted so far comes with animations of
		// its own. They share the 33-joint skeleton of Apu and Bart (Marge's has
		// 36), so they borrow theirs: Bart's for children, Apu's for adults.
		// Without, they slid about in their bind pose, arms out.
		if (body->GetAnimations().empty())
		{
			const bool child = name.rfind("boy", 0) == 0 || name.rfind("girl", 0) == 0;
			body->LoadAnimations(child ? "bart" : "apu");
		}
		playOn(*body, "_loco_walk", true);
		_bodies.push_back(std::move(body));
	}
	_bodyInUse.assign(_bodies.size(), false);
	Log::Info("PedestrianManager: {} bodies from {} extracted pedestrian models", _bodies.size(), people.size());
}

void PedestrianManager::DrawBodies(const Matrix4x4& viewProj, GL::ShaderProgram& skinShader,
                                   const ResourceManager& resources)
{
	for (const auto& p : _pedestrians)
		if (p.active && p.body >= 0)
			_bodies[p.body]->Draw(viewProj, skinShader, resources);
}

bool PedestrianManager::playOn(Character& body, const std::string& suffix, bool loop)
{
	// By the end of its name: "apu_loco_walk", "brt_flail". Failing the walk, any
	// walk at all.
	std::string found;
	for (const auto& [anim, data] : body.GetAnimations())
		if (anim.size() > suffix.size() && anim.compare(anim.size() - suffix.size(), suffix.size(), suffix) == 0)
			found = anim;
		else if (found.empty() && suffix == "_loco_walk" && anim.find("walk") != std::string::npos)
			found = anim;
	if (found.empty())
		return false;
	if (body.GetAnimName() != found)
		body.SetAnimation(found, loop);
	return true;
}

int PedestrianManager::KnockDown(const Vector3& position, float radius, const Vector3& carVelocity)
{
	int hit = 0;
	for (auto& pedestrian : _pedestrians)
	{
		if (!pedestrian.active || pedestrian.flying >= 0.0f)
			continue;
		Vector3 d = pedestrian.position - position;
		d.Y = 0.0f;
		if (d.LengthSquared() < radius * radius)
		{
			// Sent flying the way the car was going, and up; they are gone
			// when they land.
			pedestrian.flying = 0.0f;
			pedestrian.velocity = Vector3(carVelocity.X * 0.8f, 6.0f, carVelocity.Z * 0.8f);
			// Arms and legs everywhere on the way.
			if (pedestrian.body >= 0)
				playOn(*_bodies[pedestrian.body], "_flail", true);
			++hit;
		}
	}
	return hit;
}

int PedestrianManager::CountNear(const Vector3& position, float radius) const
{
	int near = 0;
	for (const auto& pedestrian : _pedestrians)
	{
		if (!pedestrian.active || pedestrian.flying >= 0.0f)
			continue;
		Vector3 d = pedestrian.position - position;
		d.Y = 0.0f;
		if (d.LengthSquared() < radius * radius)
			++near;
	}
	return near;
}

void PedestrianManager::despawnFar(float despawnRadius, const Vector3& playerPosition)
{
	auto it = _pedestrians.begin();
	while (it != _pedestrians.end())
	{
		if (!it->active)
		{
			if (it->body >= 0)
				_bodyInUse[it->body] = false;
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
