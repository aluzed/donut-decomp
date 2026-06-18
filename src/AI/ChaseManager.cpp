// Copyright 2020 the donut authors. See AUTHORS.md

#include "AI/ChaseManager.h"
#include "Core/Log.h"
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
	if (_heat <= 0.0f) return;

	_heat -= static_cast<float>(dt) * 0.5f;
	if (_heat < 0.0f) _heat = 0.0f;

	int desiredCops = static_cast<int>(_heat / 2.0f);
	if (desiredCops > _maxCops) desiredCops = _maxCops;

	while (static_cast<int>(_copCars.size()) < desiredCops && _spawnTimer <= 0.0f)
	{
		Vector3 spawnPos = playerPos + Vector3(
			(rand() % 40 - 20) * 1.0f, 0,
			(rand() % 40 - 20) * 1.0f);
		spawnPos.Y = playerPos.Y;

		auto cop = std::make_unique<Vehicle>("cop" + std::to_string(_copCars.size()));
		cop->CreatePhysicsBody(_physics, spawnPos);
		_copCars.push_back(std::move(cop));
		_spawnTimer = 3.0f;
		Log::Info("ChaseManager: cop #{} spawned", _copCars.size());
	}

	if (_spawnTimer > 0.0f)
		_spawnTimer -= static_cast<float>(dt);

	for (auto& cop : _copCars)
	{
		if (cop->GetPosition().Y < -20.0f)
		{
			cop->CreatePhysicsBody(_physics, playerPos + Vector3(rand() % 30 - 15, 0, rand() % 30 - 15));
		}
	}
}

void ChaseManager::AddHeat(float amount)
{
	_heat += amount;
	if (_heat > 10.0f) _heat = 10.0f;
	Log::Info("ChaseManager: heat = {:.1f}", _heat);
}

} // namespace Donut
