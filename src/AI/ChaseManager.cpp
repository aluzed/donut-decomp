// Copyright 2020 the donut authors. See AUTHORS.md

#include "AI/ChaseManager.h"
#include "Core/Log.h"
#include "Core/Math/Math.h"
#include "Physics/WorldPhysics.h"
#include "Vehicle.h"

namespace Donut
{

ChaseManager::ChaseManager(WorldPhysics& physics, const std::string& copCar, int count)
    : _physics(physics), _copCarModel(copCar), _maxCops(count)
{
}

void ChaseManager::Update(double dt, const Vector3& playerPos)
{
	if (_heat <= 0.0f)
	{
		if (_copCars.empty() && _evaded)
			_evaded = false;
		return;
	}

	_heat -= static_cast<float>(dt) * 0.3f;
	if (_heat < 0.0f) _heat = 0.0f;

	if (_heat <= 0.0f && !_copCars.empty())
		_evaded = true;

	int desiredCops = static_cast<int>(_heat / 2.0f);
	if (desiredCops > _maxCops) desiredCops = _maxCops;
	if (desiredCops < 1) desiredCops = 1;

	while (static_cast<int>(_copCars.size()) < desiredCops && _spawnTimer <= 0.0f)
	{
		Vector3 spawnPos = playerPos + Vector3(
			(rand() % 30 - 15) * 1.0f, 0,
			(rand() % 30 - 15) * 1.0f);
		spawnPos.Y = playerPos.Y + 2.0f;

		auto cop = std::make_unique<Vehicle>("cop" + std::to_string(_copCars.size()));
		cop->CreatePhysicsBody(_physics, spawnPos);
		_copCars.push_back(std::move(cop));
		_spawnTimer = 2.0f;
	}

	if (_spawnTimer > 0.0f)
		_spawnTimer -= static_cast<float>(dt);

	for (auto& cop : _copCars)
	{
		if (cop->GetPosition().Y < -20.0f)
			continue;

		Vector3 dir = playerPos - cop->GetPosition();
		float dist = dir.Length();

		if (dist < 2.5f)
		{
			_busted = true;
			break;
		}

		if (dist > 0.1f)
		{
			dir.Normalize();
			cop->SetPosition(cop->GetPosition() + dir * 15.0f * static_cast<float>(dt));
			float yaw = atan2f(dir.X, dir.Z);
			cop->SetRotation(Quaternion::MakeFromEuler(Vector3(0, yaw, 0)));
		}
	}
}

void ChaseManager::AddHeat(float amount)
{
	_heat += amount;
	if (_heat > 10.0f) _heat = 10.0f;
	_busted = false;
	_evaded = false;
}

} // namespace Donut
