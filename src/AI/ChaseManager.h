// Copyright 2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

#include <memory>
#include <vector>

namespace Donut
{

class Vehicle;
class WorldPhysics;

class ChaseManager
{
public:
	ChaseManager(WorldPhysics& physics, const std::string& copCar, int count);
	~ChaseManager() = default;

	void Update(double dt, const Vector3& playerPos);
	void AddHeat(float amount);
	float GetHeat() const { return _heat; }
	bool IsActive() const { return _heat > 0.0f; }
	bool IsBusted() const { return _busted; }
	bool IsEvaded() const { return _evaded && _heat <= 0.0f; }

	const std::vector<std::unique_ptr<Vehicle>>& GetCopCars() const { return _copCars; }

private:
	WorldPhysics& _physics;
	std::string _copCarModel;
	float _heat = 0.0f;
	int _maxCops;
	std::vector<std::unique_ptr<Vehicle>> _copCars;
	float _spawnTimer = 0.0f;
	bool _busted = false;
	bool _evaded = false;
};

} // namespace Donut
