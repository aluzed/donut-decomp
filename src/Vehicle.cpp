// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Vehicle.h"
#include "Core/Log.h"
#include "Physics/BulletCast.h"
#include "Physics/WorldPhysics.h"
#include "Render/CompositeModel.h"
#include "Render/OpenGL/ShaderProgram.h"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletDynamics/Vehicle/btRaycastVehicle.h>
#include <LinearMath/btDefaultMotionState.h>

namespace Donut
{

Vehicle::Vehicle(const std::string& name): _name(name), _position(Vector3::Zero), _rotation(Quaternion::Identity) {}

Vehicle::~Vehicle() = default;

void Vehicle::LoadModel(const std::string& modelPath)
{
	_model = CompositeModel::LoadP3D(modelPath);
	if (_model)
	{
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
		Log::Info("Vehicle: loaded model {}", modelPath);
	}
}

void Vehicle::CreatePhysicsBody(WorldPhysics& physics, const Vector3& position)
{
	SetPosition(position);

	btVector3 pos = BulletCast<btVector3>(position + Vector3(0, 2.0f, 0));

	btCollisionShape* chassisShape = new btBoxShape(btVector3(1.0f, 0.4f, 2.5f));
	btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), pos));

	btScalar mass = 1500.0f;
	btVector3 inertia(0, 0, 0);
	chassisShape->calculateLocalInertia(mass, inertia);

	btRigidBody* chassis = new btRigidBody(mass, motionState, chassisShape, inertia);

	btVehicleRaycaster* raycaster = new btDefaultVehicleRaycaster(physics.GetDynamicsWorld());

	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_suspensionStiffness = 20.0f;
	tuning.m_suspensionCompression = 4.4f;
	tuning.m_suspensionDamping = 2.3f;
	tuning.m_maxSuspensionTravelCm = 500.0f;
	tuning.m_frictionSlip = 10.5f;

	_rayVehicle = std::make_unique<btRaycastVehicle>(tuning, chassis, raycaster);
	_rayVehicle->setCoordinateSystem(0, 1, 2);

	btVector3 wheelDirection(0, -1, 0);
	btVector3 wheelAxle(-1, 0, 0);
	btScalar wheelRadius = 0.6f;
	btScalar suspensionRestLength = 0.4f;

	_rayVehicle->addWheel(btVector3(-0.9f, 0.0f,  1.5f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
	_rayVehicle->addWheel(btVector3( 0.9f, 0.0f,  1.5f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
	_rayVehicle->addWheel(btVector3(-0.9f, 0.0f, -1.5f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);
	_rayVehicle->addWheel(btVector3( 0.9f, 0.0f, -1.5f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);

	physics.GetDynamicsWorld()->addRigidBody(chassis);
	physics.GetDynamicsWorld()->addAction(_rayVehicle.get());

	Log::Info("Vehicle: physics body created for '{}' at ({:.1f}, {:.1f}, {:.1f})",
	          _name, position.X, position.Y, position.Z);
}

void Vehicle::SetPosition(const Vector3& pos)
{
	_position = pos;
	if (_model)
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
}

void Vehicle::SetRotation(const Quaternion& rot)
{
	_rotation = rot;
	if (_model)
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
}

void Vehicle::Update(double dt)
{
	if (_rayVehicle)
	{
		for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
			_rayVehicle->updateWheelTransform(i, true);

		btTransform trans = _rayVehicle->getChassisWorldTransform();
		_position = BulletCast<Vector3>(trans.getOrigin());
		_rotation = BulletCast<Quaternion>(trans.getRotation());
	}

	if (_model)
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
}

void Vehicle::SetEngineForce(float force)
{
	if (!_rayVehicle) return;
	for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		_rayVehicle->applyEngineForce(force, i);
}

void Vehicle::SetSteeringValue(float steering)
{
	if (!_rayVehicle) return;
	_rayVehicle->setSteeringValue(steering, 0);
	_rayVehicle->setSteeringValue(steering, 1);
}

void Vehicle::SetBrake(float brake)
{
	if (!_rayVehicle) return;
	for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		_rayVehicle->setBrake(brake, i);
}

void Vehicle::ApplyInput(float throttle, float steer, float brake)
{
	float force = throttle * _gasScale * 1000.0f;
	SetEngineForce(force);
	SetSteeringValue(steer * 0.5f);
	SetBrake(brake * 100.0f);
}

void Vehicle::Draw(Matrix4x4& viewProj, GL::ShaderProgram& shader, bool opaque)
{
	if (_model)
		_model->Draw(shader, viewProj, _model->GetTransform(), opaque);
}

} // namespace Donut
