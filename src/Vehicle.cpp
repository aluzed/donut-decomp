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

#include <cmath>

namespace Donut
{

namespace
{
// Chassis centre height above the road at spawn. The wheels hang
// suspensionRestLength + wheelRadius = 0.6m below the centre, so anything less
// than that leaves them buried; the extra clears the box and lets the car settle
// onto its own suspension.
constexpr float kSpawnRideHeight = 0.9f;

// Engine force per driven wheel, as an acceleration: the force is this times the
// chassis mass. SetEngineForce drives all four wheels, so the car accelerates at
// four times this -- 6 m/s^2, 0-100 km/h in about 4.7s, on the flat.
//
// Nothing in the model stops it short of _topSpeedKmh. There is no drag, and the
// long-held belief that every car ran out of acceleration at about 56 km/h was
// wrong: 58 km/h was what a test drive happened to reach (VEH-SINK), not a
// ceiling. Measured from a standing start down M1race's 140m straight, 0.83 made
// 3.3 m/s^2 and 103 km/h by the end of it.
//
// 0.83 was kept because more power wrecked the race opponent. It no longer does.
// Over a full M1race lap (186 waypoints, 1861m), with no wedge, skip or fall at
// any setting:
//
//   0.83   118.7s   mean 52 km/h      1.5   110.1s   mean 55 km/h
//   1.2    113.0s   mean 55 km/h      2.0   107.2s   mean 63 km/h
//
// The lap is bound by the corners now, not the engine. 1.5 is a believable
// arcade car, and it lets the opponent finish 10s inside the 120s stage instead
// of 1.3s. The player's car shares this value, boost (x3) included.
constexpr float kEngineAcceleration = 1.5f;

// The chassis mass the brake figures in ApplyInput were tuned on.
constexpr float kBrakeReferenceMass = 1200.0f;

// Steering lock, tapered with speed. The wheels turn kMaxSteerStill radians when
// the car is stopped and kMaxSteerFast once it is doing kSteerTaperKmh, which is
// what keeps a fast car pointing where it is going.
constexpr float kMaxSteerStill = 0.5f;
constexpr float kMaxSteerFast = 0.12f;
constexpr float kSteerTaperKmh = 90.0f;
} // namespace

Vehicle::Vehicle(const std::string& name): _name(name), _position(Vector3::Zero), _rotation(Quaternion::Identity) {}

Vehicle::~Vehicle()
{
	// A car dropped without DestroyPhysics left its raycast vehicle registered
	// as an action, and the next physics step called into freed memory: the
	// Hit & Run police car, removed on a bust, crashed the game.
	if (_physicsWorld && _rayVehicle)
		DestroyPhysics(*_physicsWorld);
}

void Vehicle::LoadModel(const std::string& modelPath)
{
	_model = CompositeModel::LoadP3D(modelPath);
	if (_model)
	{
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
		Log::Info("Vehicle: loaded model {}", modelPath);
	}
}

namespace
{
// The ground a car is put down on at `position`: the surface just under it,
// looking only a little above it first. Searching from 50m up found the deck
// of any bridge overhead -- level 2 m6's monkey under the overpass got the car
// dropped on the road 18m above it. Only when nothing is that close does the
// search reach 50m up, for points given below the surface.
bool findSpawnGround(const WorldPhysics& physics, const Vector3& position, float& groundY)
{
	return physics.FindGroundHeight(position, 3.0f, 50.0f, groundY, true) ||
	       physics.FindGroundHeight(position, 50.0f, 50.0f, groundY, true);
}
} // namespace

void Vehicle::CreatePhysicsBody(WorldPhysics& physics, const Vector3& position)
{
	SetPosition(position);

	// A locator or path node names a spot on the map, not the height of the road
	// there: spawning at position + 1.5m dropped the race car into the terrain at
	// (108, 1.1, -559), where it came to rest with its chassis box wedged in the
	// ground and *no wheel touching it*. applyEngineForce only reaches the road
	// through the suspension rays, so the opponent sat at 0 km/h being pushed out
	// of the geometry a centimetre at a time. Find the ground and sit the wheels
	// on it instead.
	float spawnY = position.Y + kSpawnRideHeight;
	float groundY = 0.0f;
	if (findSpawnGround(physics, position, groundY))
	{
		spawnY = groundY + kSpawnRideHeight;
	}
	else
	{
		Log::Warn("Vehicle: no ground under '{}' spawn ({:.1f}, {:.1f}, {:.1f}), dropping it from the given height",
		          _name, position.X, position.Y, position.Z);
	}

	btVector3 pos(position.X, spawnY, position.Z);

	btCollisionShape* chassisShape = new btBoxShape(btVector3(0.9f, 0.4f, 2.2f));
	btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), pos));

	// The car script's mass, not one figure for every car: they run from 250kg to
	// 8000kg. Engine force scales with it, so acceleration does not change; what
	// does is how the car carries itself through a collision.
	btScalar mass = _mass;
	btVector3 inertia(0, 0, 0);
	chassisShape->calculateLocalInertia(mass, inertia);

	btRigidBody* chassis = new btRigidBody(mass, motionState, chassisShape, inertia);

	btVehicleRaycaster* raycaster = new btDefaultVehicleRaycaster(physics.GetDynamicsWorld());

	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_suspensionStiffness = 30.0f;
	tuning.m_suspensionCompression = 4.4f;
	tuning.m_suspensionDamping = 2.8f;
	tuning.m_maxSuspensionTravelCm = 500.0f;
	tuning.m_frictionSlip = 10.5f;

	_rayVehicle = std::make_unique<btRaycastVehicle>(tuning, chassis, raycaster);
	_rayVehicle->setCoordinateSystem(0, 1, 2);

	btVector3 wheelDirection(0, -1, 0);
	btVector3 wheelAxle(-1, 0, 0);
	btScalar wheelRadius = 0.4f;
	btScalar suspensionRestLength = 0.2f;

	_rayVehicle->addWheel(btVector3(-0.8f, 0.0f,  1.8f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
	_rayVehicle->addWheel(btVector3( 0.8f, 0.0f,  1.8f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
	_rayVehicle->addWheel(btVector3(-0.8f, 0.0f, -1.8f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);
	_rayVehicle->addWheel(btVector3( 0.8f, 0.0f, -1.8f), wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);

	// A btRaycastVehicle drives its chassis through applyEngineForce/setBrake,
	// which do NOT activate the body. Once Bullet puts the chassis to sleep the
	// car simply ignores the throttle: it sat parked with the engine at full
	// force. Vehicles are expected to opt out of deactivation.
	chassis->setActivationState(DISABLE_DEACTIVATION);

	physics.GetDynamicsWorld()->addRigidBody(chassis);
	physics.GetDynamicsWorld()->addAction(_rayVehicle.get());
	_physicsWorld = &physics;

	Log::Info("Vehicle: physics body created for '{}' at ({:.1f}, {:.1f}, {:.1f}), ground {:.1f} -- {:.0f}kg, gas "
	          "scale {:.1f}, top speed {:.0f} km/h",
	          _name, position.X, spawnY, position.Z, groundY, _mass, _gasScale, _topSpeedKmh);
}

void Vehicle::Teleport(const Vector3& position, const Quaternion& rotation, bool onGround)
{
	SetPosition(position);
	SetRotation(rotation);

	if (!_rayVehicle)
		return;

	float spawnY = position.Y + kSpawnRideHeight;
	float groundY = 0.0f;
	// The level's ground, not whatever is in the way: a car moved to where it
	// already stands found its own roof and was dropped from 1.3m up.
	if (!onGround)
		spawnY = position.Y;
	else if (_physicsWorld && findSpawnGround(*_physicsWorld, position, groundY))
		spawnY = groundY + kSpawnRideHeight;

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(position.X, spawnY, position.Z));
	transform.setRotation(btQuaternion(rotation.X, rotation.Y, rotation.Z, rotation.W));

	btRigidBody* chassis = _rayVehicle->getRigidBody();
	chassis->setWorldTransform(transform);
	if (chassis->getMotionState())
		chassis->getMotionState()->setWorldTransform(transform);
	chassis->setLinearVelocity(btVector3(0, 0, 0));
	chassis->setAngularVelocity(btVector3(0, 0, 0));
	chassis->clearForces();
	_rayVehicle->resetSuspension();

	Log::Info("Vehicle: '{}' teleported to ({:.1f}, {:.1f}, {:.1f})", _name, position.X, spawnY, position.Z);
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

void Vehicle::SetMass(float mass)
{
	_mass = mass;
	if (!_rayVehicle)
		return;

	btRigidBody* chassis = _rayVehicle->getRigidBody();
	btVector3 inertia(0, 0, 0);
	chassis->getCollisionShape()->calculateLocalInertia(mass, inertia);
	chassis->setMassProps(mass, inertia);
	chassis->updateInertiaTensor();
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

void Vehicle::ApplyInput(float throttle, float steer, float brake, float boost)
{
	// throttle runs -1..1: negative drives the wheels backwards. There was no way
	// to reverse before -- the AI's "back out of the wall" manoeuvre passed
	// throttle 0 and brake 1, which just stops a car that is already stopped, so
	// nothing wedged against anything ever got free.

	// Engine force from the mass it has to move, rather than a flat 1000N, scaled by
	// the car's own gas scale; and cut at the car's own top speed, which nothing
	// else enforces -- the model has no drag (see kEngineAcceleration).
	if (GetSpeedKmh() >= _topSpeedKmh * boost)
		throttle = throttle > 0.0f ? 0.0f : throttle;

	const float mass = _rayVehicle && _rayVehicle->getRigidBody()->getInvMass() > 0.0f
	                       ? 1.0f / _rayVehicle->getRigidBody()->getInvMass()
	                       : _mass;

	const float force = throttle * (_gasScale / kReferenceGasScale) * mass * kEngineAcceleration * boost;
	SetEngineForce(force);

	// Speed-sensitive steering. Half a radian of lock is right at walking pace and
	// spins the car at 90 km/h: the front wheels ask for a corner far tighter than
	// the chassis will hold, it snaps round, and the AI was then found stationary
	// at full throttle with the steering hard over -- facing back the way it came.
	// Tapering the lock with speed is what every car does, and it is what lets the
	// engine be worth anything: without it, more power only meant spinning sooner.
	const float speedKmh = std::fabs(GetSpeedKmh());
	float t = speedKmh / kSteerTaperKmh;
	if (t > 1.0f) t = 1.0f;
	const float maxSteer = kMaxSteerStill + (kMaxSteerFast - kMaxSteerStill) * t;

	SetSteeringValue(steer * maxSteer);
	// btRaycastVehicle takes the brake as a cap on the friction impulse per step,
	// so the same figure stops a heavy car more slowly. These were set on the one
	// 1200kg chassis every car used to share; since the car scripts give each its
	// own mass, a 2500kg bus braked at half the rate the race planner counts on,
	// overshot a right-angle corner at 60 km/h and wedged itself sideways.
	SetBrake((brake * 100.0f + 10.0f) * (mass / kBrakeReferenceMass));
}

void Vehicle::Jump()
{
	if (_rayVehicle)
	{
		btRigidBody* chassis = _rayVehicle->getRigidBody();
		if (chassis)
		{
			chassis->applyCentralImpulse(btVector3(0, 5000.0f, 0));
			Log::Info("Vehicle: jump!");
		}
	}
}

void Vehicle::DestroyPhysics(WorldPhysics& physics)
{
	if (_rayVehicle)
	{
		btRigidBody* chassis = _rayVehicle->getRigidBody();
		if (chassis)
			physics.GetDynamicsWorld()->removeRigidBody(chassis);

		physics.GetDynamicsWorld()->removeAction(_rayVehicle.get());
		delete chassis;
		_rayVehicle.reset();
		Log::Info("Vehicle: physics destroyed for '{}'", _name);
	}
}

void Vehicle::Draw(Matrix4x4& viewProj, GL::ShaderProgram& shader, bool opaque)
{
	if (_model)
		_model->Draw(shader, viewProj, _model->GetTransform(), opaque);
}

Vector3 Vehicle::GetVelocity() const
{
	if (!_rayVehicle)
		return Vector3::Zero;
	const btVector3& v = _rayVehicle->getRigidBody()->getLinearVelocity();
	return Vector3(v.x(), v.y(), v.z());
}

bool Vehicle::IsAirborne() const
{
	if (!_rayVehicle)
		return false;
	for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		if (_rayVehicle->getWheelInfo(i).m_raycastInfo.m_isInContact)
			return false;
	return true;
}

float Vehicle::GetSpeedKmh() const
{
	if (_rayVehicle)
		return _rayVehicle->getCurrentSpeedKmHour();
	return 0.0f;
}

} // namespace Donut
