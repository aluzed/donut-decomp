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
	void ApplyInput(float throttle, float steer, float brake, float boost = 1.0f);
	void Jump();
	void DestroyPhysics(WorldPhysics& physics);

	// The world this car was created in, so an agent driving it can ask what is
	// in front of it. Null until CreatePhysicsBody has run.
	class WorldPhysics* GetPhysics() const { return _physicsWorld; }

	// Takes effect on the physics body too when it already exists.
	void SetMass(float mass);
	// The car script's engine strength, as the game data gives it: 2 to 11, 4.5
	// for the family sedan. Read relative to kReferenceGasScale.
	void SetGasScale(float scale) { _gasScale = scale; }
	void SetTopSpeedKmh(float speed) { _topSpeedKmh = speed; }
	void SetTireGrip(float grip) { _tireGrip = grip; }
	void SetSuspensionLimit(float limit) { _suspensionLimit = limit; }

	// Moves the car bodily: resets the chassis transform, clears its momentum and
	// lets the suspension settle again. SetPosition only moves the model.
	// Puts the car down on the ground at `position`; `onGround` false leaves it
	// at that very height, to fall from there.
	void Teleport(const Vector3& position, const Quaternion& rotation, bool onGround = true);

	void SetPosition(const Vector3& pos);
	void SetRotation(const Quaternion& rot);
	const Vector3& GetPosition() const { return _position; }
	const Quaternion& GetRotation() const { return _rotation; }
	const std::string& GetName() const { return _name; }
	// The model's file name without its folder or extension, "famil_v": the
	// prefix of the car's own texture names.
	const std::string& GetModelName() const { return _modelName; }
	bool HasModel() const { return _model != nullptr; }
	float GetSpeedKmh() const;
	// No wheel touching anything.
	bool IsAirborne() const;
	// The chassis' velocity, in m/s.
	Vector3 GetVelocity() const;


private:
	std::string _name;
	Vector3 _position;
	Quaternion _rotation;

	class WorldPhysics* _physicsWorld = nullptr;
	float _mass = 1500.0f;
	// The gas scale kEngineAcceleration was tuned on -- snake_v's, the M1race
	// opponent -- so that car keeps exactly the acceleration it was measured at,
	// and a car with no script drives like it.
	static constexpr float kReferenceGasScale = 8.0f;
	float _gasScale = kReferenceGasScale;
	float _topSpeedKmh = 180.0f;
	float _tireGrip = 1.0f;
	float _suspensionLimit = 0.3f;

	std::unique_ptr<CompositeModel> _model;
	std::string _modelName;
	std::unique_ptr<btRaycastVehicle> _rayVehicle;
};

} // namespace Donut
