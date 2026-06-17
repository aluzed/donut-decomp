// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"

#include <BulletDynamics/Vehicle/btRaycastVehicle.h>

#include <memory>
#include <string>

namespace Donut
{

class CompositeModel;
namespace GL { class ShaderProgram; }
class WorldPhysics;

class Vehicle
{
public:
	Vehicle(const std::string& name);
	~Vehicle();

	void LoadModel(const std::string& modelPath);
	void CreatePhysicsBody(WorldPhysics& physics, const Vector3& position);
	void Update(double dt);
	void Draw(Matrix4x4& viewProj, GL::ShaderProgram& shader, bool opaque);

	void SetEngineForce(float force);
	void SetSteeringValue(float steering);
	void SetBrake(float brake);
	void ApplyInput(float throttle, float steer, float brake);

	void SetMass(float mass) { _mass = mass; }
	void SetGasScale(float scale) { _gasScale = scale; }
	void SetTopSpeedKmh(float speed) { _topSpeedKmh = speed; }
	void SetTireGrip(float grip) { _tireGrip = grip; }
	void SetSuspensionLimit(float limit) { _suspensionLimit = limit; }

	void SetPosition(const Vector3& pos);
	void SetRotation(const Quaternion& rot);
	const Vector3& GetPosition() const { return _position; }
	const Quaternion& GetRotation() const { return _rotation; }
	const std::string& GetName() const { return _name; }
	bool HasModel() const { return _model != nullptr; }
	float GetSpeedKmh() const;

private:
	std::string _name;
	Vector3 _position;
	Quaternion _rotation;

	float _mass = 1500.0f;
	float _gasScale = 1.0f;
	float _topSpeedKmh = 180.0f;
	float _tireGrip = 1.0f;
	float _suspensionLimit = 0.3f;

	std::unique_ptr<CompositeModel> _model;
	std::unique_ptr<btRaycastVehicle> _rayVehicle;
};

} // namespace Donut
