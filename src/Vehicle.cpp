// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Vehicle.h"
#include "Core/Log.h"
#include "Physics/BulletCast.h"
#include "Physics/WorldPhysics.h"
#include "Render/CompositeModel.h"
#include "P3D/P3DFile.h"
#include "P3D/P3D.generated.h"
#include "Render/BillboardBatch.h"
#include "Render/OpenGL/ShaderProgram.h"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletDynamics/Vehicle/btRaycastVehicle.h>
#include <LinearMath/btDefaultMotionState.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

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

// The wheels' rays, which pass through their own car. Bullet's default
// raycaster takes the first body a ray meets; with a chassis of several boxes
// (the car's own collision object), a wheel's ray can meet one of them.
class OwnCarRaycaster : public btVehicleRaycaster
{
public:
	OwnCarRaycaster(btDynamicsWorld* world, const btCollisionObject* car): _world(world), _car(car) {}

	void* castRay(const btVector3& from, const btVector3& to, btVehicleRaycasterResult& result) override
	{
		struct Closest : btCollisionWorld::ClosestRayResultCallback
		{
			const btCollisionObject* car;
			Closest(const btVector3& from, const btVector3& to, const btCollisionObject* car):
			    ClosestRayResultCallback(from, to), car(car)
			{
			}
			btScalar addSingleResult(btCollisionWorld::LocalRayResult& ray, bool normalInWorldSpace) override
			{
				if (ray.m_collisionObject == car)
					return m_closestHitFraction;
				return ClosestRayResultCallback::addSingleResult(ray, normalInWorldSpace);
			}
		} closest(from, to, _car);
		_world->rayTest(from, to, closest);
		if (!closest.hasHit())
			return nullptr;
		// As the default one: only what a car can stand on.
		const btRigidBody* body = btRigidBody::upcast(closest.m_collisionObject);
		if (!body || !body->hasContactResponse())
			return nullptr;
		result.m_hitPointInWorld = closest.m_hitPointWorld;
		result.m_hitNormalInWorld = closest.m_hitNormalWorld.normalized();
		result.m_distFraction = closest.m_closestHitFraction;
		return const_cast<btRigidBody*>(body);
	}

private:
	btDynamicsWorld* _world;
	const btCollisionObject* _car;
};

// The most a wheel's spring may push, for a car of that mass: three times its
// share of the car's weight, to take landings. Bullet's default is 6000 N
// whatever the car: a car over 2450 kg, its four wheels unable to hold it up,
// sank onto its own chassis and sat there at full throttle (the Itchy &
// Scratchy truck, 2500 kg; the Buzz Cola truck, 4000 kg).
// The median of the 90 car scripts' figures: a car's own is read against it,
// so that the median car handles as every car did before the scripts' tuning
// was applied.
constexpr float kMedianWheelTurnDeg = 30.0f;
constexpr float kMedianSteeringDrop = 0.272f;
constexpr float kMedianBrakeScale = 7.0f;
constexpr float kMedianSpringK = 0.4f;
constexpr float kMedianDamperC = 0.2f;
constexpr float kMedianHitPoints = 3.0f;
// The suspension every car had: these at the median spring and damper.
constexpr float kSuspensionStiffness = 30.0f;
constexpr float kSuspensionCompression = 4.4f;
constexpr float kSuspensionDamping = 2.8f;

float maxSuspensionForce(float mass)
{
	return std::max(6000.0f, mass * 9.8f / 4.0f * 3.0f);
}
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
	_modelName = std::filesystem::path(modelPath).stem().string();
	if (_model)
	{
		// The physics wheels are front right, front left, rear right, rear left
		// (the car faces +Z, its left is +X); each wheel joint goes to the one
		// on its side of the car.
		for (int wheel = 0; wheel < 4; ++wheel)
		{
			const int found = _model->FindJoint(fmt::format("w{}", wheel));
			if (found >= 0)
			{
				const Vector3 at = _model->GetJointRestPosition(found);
				const int slot = (at.Z > 0.0f ? 0 : 2) + (at.X > 0.0f ? 1 : 0);
				_wheelJoints[slot] = found;
				_wheelHubs[slot] = at;
				if (const float radius = _model->GetJointPropRadius(found); radius > 0.1f)
					_wheelRadii[slot] = radius;
			}
		}
		// The physics' wheels where the model's are, as large: on one set of
		// wheels for every car (0.8 m out, 1.8 m along, 0.4 m across), a
		// truck's and a buggy's sank into the road or stood off it.
		_wheelsFromModel = std::all_of(std::begin(_wheelJoints), std::end(_wheelJoints), [](int j) { return j >= 0; });
		if (_wheelsFromModel)
			Log::Info("Vehicle: '{}' wheels at y {:.2f} to {:.2f}, {:.2f} to {:.2f} m across", _modelName,
			          std::min({_wheelHubs[0].Y, _wheelHubs[1].Y, _wheelHubs[2].Y, _wheelHubs[3].Y}),
			          std::max({_wheelHubs[0].Y, _wheelHubs[1].Y, _wheelHubs[2].Y, _wheelHubs[3].Y}),
			          2.0f * std::min({_wheelRadii[0], _wheelRadii[1], _wheelRadii[2], _wheelRadii[3]}),
			          2.0f * std::max({_wheelRadii[0], _wheelRadii[1], _wheelRadii[2], _wheelRadii[3]}));
		// Its chassis as the game has it: the boxes on the root joint of its own
		// collision object (named as the car), rather than one box for every car, 4.4 m long and
		// 0.4 m below its middle: the cars run from a 1.5 m dune buggy to a
		// 10 m fire truck, and most of their bodies stand higher off the road.
		const P3D::P3DFile file(modelPath);
		for (const auto& chunk : file.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::CollisionObject))
			{
				auto collision = std::make_unique<P3D::CollisionObject>(*chunk);
				std::string name = collision->GetName().substr(0, collision->GetName().find('\0'));
				if (collision->GetVolume() && name.size() == _modelName.size() &&
				    std::equal(name.begin(), name.end(), _modelName.begin(),
				               [](char a, char b) { return std::tolower(a) == std::tolower(b); }))
					_collision = std::move(collision);
			}
		_doorJoint = _model->FindJoint("DoorDRot");
		_seatJoint = _model->FindJoint("dl");
		_hoodJoint = _model->FindJoint("HoodTrans");
		if (const int hub = _model->FindJoint("w0"); hub >= 0 && _seatJoint >= 0)
			_high = _model->GetJointRestPosition(_seatJoint).Y - _model->GetJointRestPosition(hub).Y > 0.5f;
		// Its lights: brake1Shape..brake4Shape, rev1Shape..rev4Shape.
		for (const auto& chunk : file.GetRoot().GetChildren())
			if (chunk->IsType(P3D::ChunkType::BillboardQuadGroup))
			{
				const auto group = P3D::BillboardQuadGroup::Load(*chunk);
				const std::string& name = group->GetName();
				const bool brake = name.rfind("brake", 0) == 0;
				if (!brake && name.rfind("rev", 0) != 0)
					continue;
				const int joint = _model->FindPropJoint(name);
				if (joint >= 0)
					_lights.push_back({std::make_unique<BillboardBatch>(*group), joint, brake});
			}
		// "DoorDTrans" stands at the origin; the hinge is where the door is.
		if (_doorJoint >= 0)
			_driverSide = _model->GetJointRestPosition(_doorJoint).X >= 0.0f ? 1.0f : -1.0f;
	}
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
	float spawnY = position.Y + rideHeight();
	float groundY = 0.0f;
	if (findSpawnGround(physics, position, groundY))
	{
		spawnY = groundY + rideHeight();
	}
	else
	{
		Log::Warn("Vehicle: no ground under '{}' spawn ({:.1f}, {:.1f}, {:.1f}), dropping it from the given height",
		          _name, position.X, position.Y, position.Z);
	}

	btVector3 pos(position.X, spawnY, position.Z);

	_chassisShapes.clear();
	btCollisionShape* chassisShape =
	    _collision ? WorldPhysics::CreateBoxesShape(*_collision->GetVolume(), 0, _chassisShapes) : nullptr;
	if (chassisShape)
	{
		// Not one that reaches below its wheels: dragged along the road, it
		// would hold the car still. The wheels are the model's, or the ones
		// every car without wheel joints gets: 0.2m of suspension and a 0.4m
		// wheel under its middle (the Itchy & Scratchy truck's boxes reach
		// 0.71m down: it wedged over and over, nothing in front of it).
		btVector3 low, high;
		btTransform identity;
		identity.setIdentity();
		chassisShape->getAabb(identity, low, high);
		float wheelsBottom = _wheelsFromModel ? 0.0f : -0.6f;
		for (int i = 0; _wheelsFromModel && i < 4; ++i)
			wheelsBottom = std::min(wheelsBottom, _wheelHubs[i].Y - _wheelRadii[i]);
		if (low.y() < wheelsBottom + 0.05f)
		{
			Log::Info("Vehicle: '{}' chassis reaches {:.2f} m, its wheels {:.2f} m: one box instead", _modelName,
			          low.y(), wheelsBottom);
			_chassisShapes.clear();
			chassisShape = nullptr;
		}
	}
	if (!chassisShape)
	{
		chassisShape = new btBoxShape(btVector3(0.9f, 0.4f, 2.2f));
		_chassisShapes.emplace_back(chassisShape);
	}
	// The body turns about its centre of mass, which the car script puts
	// somewhere in the model (SetCMOffsetY -0.1: a little low; Z 0.35: towards
	// the front): its shapes and wheels are placed about it, and it stands
	// that far from the model's origin.
	const Vector3 centre = centreOfMass();
	if (centre.LengthSquared() > 0.0f)
	{
		auto* about = new btCompoundShape();
		btTransform offset;
		offset.setIdentity();
		offset.setOrigin(BulletCast<btVector3>(-centre));
		about->addChildShape(offset, chassisShape);
		_chassisShapes.emplace_back(about);
		chassisShape = about;
		pos += BulletCast<btVector3>(centre);
	}
	btDefaultMotionState* motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), pos));

	// The car script's mass, not one figure for every car: they run from 250kg to
	// 8000kg. Engine force scales with it, so acceleration does not change; what
	// does is how the car carries itself through a collision.
	btScalar mass = _mass;
	btVector3 inertia(0, 0, 0);
	chassisShape->calculateLocalInertia(mass, inertia);

	btRigidBody* chassis = new btRigidBody(mass, motionState, chassisShape, inertia);

	_raycaster = std::make_unique<OwnCarRaycaster>(physics.GetDynamicsWorld(), chassis);
	btVehicleRaycaster* raycaster = _raycaster.get();

	btRaycastVehicle::btVehicleTuning tuning;
	// Springs and dampers from the car script (SetSpringK, SetDamperC), against
	// the median car's.
	const float spring = std::clamp(GetSetting("SetSpringK", kMedianSpringK) / kMedianSpringK, 0.5f, 2.0f);
	const float damper = std::clamp(GetSetting("SetDamperC", kMedianDamperC) / kMedianDamperC, 0.5f, 3.0f);
	tuning.m_suspensionStiffness = kSuspensionStiffness * spring;
	tuning.m_suspensionCompression = kSuspensionCompression * damper;
	tuning.m_suspensionDamping = kSuspensionDamping * damper;
	tuning.m_maxSuspensionTravelCm = 500.0f;
	tuning.m_maxSuspensionForce = maxSuspensionForce(mass);
	tuning.m_frictionSlip = kFrictionSlip;

	_rayVehicle = std::make_unique<btRaycastVehicle>(tuning, chassis, raycaster);
	_rayVehicle->setCoordinateSystem(0, 1, 2);

	btVector3 wheelDirection(0, -1, 0);
	btVector3 wheelAxle(-1, 0, 0);
	btScalar wheelRadius = 0.4f;
	btScalar suspensionRestLength = 0.2f;

	if (_wheelsFromModel)
	{
		// Each from just above its hub, the suspension at rest bringing it to
		// the hub: the model's wheel on the road where the physics' is.
		for (int i = 0; i < 4; ++i)
		{
			const Vector3 hub = _wheelHubs[i] - centre;
			_rayVehicle->addWheel(btVector3(hub.X, hub.Y + suspensionRestLength, hub.Z), wheelDirection, wheelAxle,
			                      suspensionRestLength, _wheelRadii[i], tuning, i < 2);
		}
	}
	else
	{
		const btVector3 shift = BulletCast<btVector3>(centre);
		_rayVehicle->addWheel(btVector3(-0.8f, 0.0f, 1.8f) - shift, wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
		_rayVehicle->addWheel(btVector3(0.8f, 0.0f, 1.8f) - shift, wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, true);
		_rayVehicle->addWheel(btVector3(-0.8f, 0.0f, -1.8f) - shift, wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);
		_rayVehicle->addWheel(btVector3(0.8f, 0.0f, -1.8f) - shift, wheelDirection, wheelAxle, suspensionRestLength, wheelRadius, tuning, false);
	}

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

	float spawnY = position.Y + rideHeight();
	float groundY = 0.0f;
	// The level's ground, not whatever is in the way: a car moved to where it
	// already stands found its own roof and was dropped from 1.3m up.
	if (!onGround)
		spawnY = position.Y;
	else if (_physicsWorld && findSpawnGround(*_physicsWorld, position, groundY))
		spawnY = groundY + rideHeight();

	btTransform transform;
	transform.setIdentity();
	// The body's origin is its centre of mass, that far from the model's.
	transform.setOrigin(btVector3(position.X, spawnY, position.Z) + BulletCast<btVector3>(rotation * centreOfMass()));
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

float Vehicle::rideHeight() const
{
	// Its wheels' bottoms on the road, its suspension at rest, and a little
	// over to settle onto it; never less than the old figure for one wheel
	// set.
	if (!_wheelsFromModel)
		return kSpawnRideHeight;
	float bottom = 0.0f;
	for (int i = 0; i < 4; ++i)
		bottom = std::min(bottom, _wheelHubs[i].Y - _wheelRadii[i]);
	return std::max(kSpawnRideHeight, -bottom + 0.3f);
}

void Vehicle::SetPosition(const Vector3& pos)
{
	_position = pos;
	if (_model)
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));

	// The wheels roll and the front ones steer, as the physics wheels do.
	if (_model && _rayVehicle)
		for (int i = 0; i < _rayVehicle->getNumWheels() && i < 4; ++i)
			if (_wheelJoints[i] >= 0)
			{
				const btWheelInfo& wheel = _rayVehicle->getWheelInfo(i);
				// Up or down with its suspension, from where it rests.
				const float travel = _wheelsFromModel
				                         ? wheel.m_suspensionRestLength1 - wheel.m_raycastInfo.m_suspensionLength
				                         : 0.0f;
				_model->SetJointRotation(_wheelJoints[i],
				                         Quaternion(Vector3(0.0f, 1.0f, 0.0f), wheel.m_steering) *
				                             Quaternion(Vector3(-1.0f, 0.0f, 0.0f), wheel.m_rotation),
				                         Vector3(0.0f, std::clamp(travel, -0.3f, 0.3f), 0.0f));
			}
}

void Vehicle::DrawLights(const Matrix4x4& viewProj, GL::ShaderProgram& billboardShader)
{
	if (!_model || (!_braking && !_reversing))
		return;
	// The blending the billboards set is put back after: the HUD drawn later
	// relies on it.
	const GLboolean blending = glIsEnabled(GL_BLEND);
	GLint source = GL_SRC_ALPHA, destination = GL_ONE_MINUS_SRC_ALPHA;
	glGetIntegerv(GL_BLEND_SRC_RGB, &source);
	glGetIntegerv(GL_BLEND_DST_RGB, &destination);
	glEnable(GL_BLEND);
	billboardShader.Bind();
	for (const bool opaque : {true, false})
		for (const auto& light : _lights)
			if (light.brake ? _braking : _reversing)
			{
				billboardShader.SetUniformValue("viewProj",
				                                viewProj * _model->GetTransform() * _model->GetJointTransform(light.joint));
				light.batch->Draw(billboardShader, opaque);
			}
	billboardShader.SetUniformValue("viewProj", viewProj);
	glBlendFunc(source, destination);
	if (!blending)
		glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

bool Vehicle::GetDriverSeat(Vector3& seat) const
{
	if (!_model || _seatJoint < 0)
		return false;
	seat = _model->GetTransform() * _model->GetJointRestPosition(_seatJoint);
	return true;
}

Vector3 Vehicle::GetHoodPosition() const
{
	const Matrix4x4 placement = Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation);
	if (!_model || _hoodJoint < 0)
		return placement * Vector3(0.0f, 0.8f, 1.6f);
	return _model->GetTransform() * _model->GetJointRestPosition(_hoodJoint);
}

void Vehicle::SetDoorOpen(float amount)
{
	// Hinged at its front edge, it swings out, about 70 degrees.
	if (_model && _doorJoint >= 0)
		_model->SetJointRotation(_doorJoint, Quaternion(Vector3(0.0f, 1.0f, 0.0f), 1.2f * amount));
}

void Vehicle::SetRotation(const Quaternion& rot)
{
	_rotation = rot;
	if (_model)
		_model->SetTransform(Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation));
}

namespace
{
// How well a tyre holds on each ground, against the road's: grass, dirt and
// gravel give, sand and a wet floor give more.
float surfaceGrip(Surface surface)
{
	switch (surface)
	{
	case Surface::Grass: return 0.75f;
	case Surface::Dirt: return 0.75f;
	case Surface::Gravel: return 0.7f;
	case Surface::Sand: return 0.6f;
	case Surface::Water: return 0.6f;
	case Surface::Metal: return 0.9f;
	default: return 1.0f;
	}
}
} // namespace

void Vehicle::Update(double dt)
{
	if (_rayVehicle)
	{
		for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		{
			// The ground under each wheel, and its grip for the next step.
			btWheelInfo& wheel = _rayVehicle->getWheelInfo(i);
			if (i < kWheels)
			{
				_wheelTouching[i] = wheel.m_raycastInfo.m_isInContact;
				_wheelContacts[i] = BulletCast<Vector3>(wheel.m_raycastInfo.m_contactPointWS);
				_wheelNormals[i] = BulletCast<Vector3>(wheel.m_raycastInfo.m_contactNormalWS);
				_wheelAxles[i] = BulletCast<Vector3>(wheel.m_raycastInfo.m_wheelAxleWS);
				// How fast the tyre goes sideways over the ground: the chassis'
				// velocity where it touches, along its axle.
				const btRigidBody* chassis = _rayVehicle->getRigidBody();
				const btVector3 at = wheel.m_raycastInfo.m_contactPointWS - chassis->getCenterOfMassPosition();
				const btVector3 velocity = chassis->getLinearVelocity() + chassis->getAngularVelocity().cross(at);
				_wheelSlides[i] = _wheelTouching[i] ? std::fabs(velocity.dot(wheel.m_raycastInfo.m_wheelAxleWS)) : 0.0f;
				if (_physicsWorld && _wheelTouching[i])
					_wheelSurfaces[i] = _physicsWorld->GetSurfaceUnder(_wheelContacts[i]);
				wheel.m_frictionSlip = kFrictionSlip * surfaceGrip(_wheelSurfaces[i]);
			}
			// updateWheelTransform clears the wheel's contact, which only the
			// next physics step finds again: drawn at 240 frames a second over
			// 60 steps, the car was airborne, its wheels touching nothing,
			// three frames in four.
			const bool touching = wheel.m_raycastInfo.m_isInContact;
			_rayVehicle->updateWheelTransform(i, true);
			wheel.m_raycastInfo.m_isInContact = touching;
		}

		btTransform trans = _rayVehicle->getChassisWorldTransform();
		_rotation = BulletCast<Quaternion>(trans.getRotation());
		// The model's origin, from the body's (its centre of mass).
		_position = BulletCast<Vector3>(trans.getOrigin()) - _rotation * centreOfMass();
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
	for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		_rayVehicle->getWheelInfo(i).m_maxSuspensionForce = maxSuspensionForce(mass);
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
	// Braking: the brake, or the throttle against the way it is rolling.
	const float rolling = GetSpeedKmh();
	_braking = brake > 0.1f || (throttle < -0.1f && rolling > 2.0f);
	_reversing = throttle < -0.1f && rolling < 1.0f;
	// Stamping on it at speed: the tyres lock and mark the road.
	_brakingHard = (brake > 0.5f || throttle < -0.5f) && rolling > kSkidBrakeKmh;
	// Floored from a standstill: the tyres spin, the more the slower it goes.
	_peelOut = throttle > 0.9f && rolling >= 0.0f && rolling < kPeelOutKmh ? 1.0f - rolling / kPeelOutKmh : 0.0f;

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

	// Some cars change gear, as it were: past a share of their top speed the
	// script gives them another gas scale (SetHighSpeedGasScale,
	// SetGasScaleSpeedThreshold).
	float gasScale = _gasScale;
	if (const float high = GetSetting("SetHighSpeedGasScale", 0.0f); high > 0.0f)
		if (GetSpeedKmh() > GetSetting("SetGasScaleSpeedThreshold", 1.0f) * _topSpeedKmh)
			gasScale = high;
	const float force = throttle * (gasScale / kReferenceGasScale) * mass * kEngineAcceleration * boost;
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
	// Each car's own lock (SetMaxWheelTurnAngle, 9 to 55 degrees) and how much
	// of it it loses at speed (SetHighSpeedSteeringDrop), against the median
	// car's: a bus turns less tightly than a buggy.
	const float lock = GetLockFactor();
	const float keep = std::clamp((1.0f - GetSetting("SetHighSpeedSteeringDrop", kMedianSteeringDrop)) /
	                                  (1.0f - kMedianSteeringDrop),
	                              0.5f, 1.4f);
	const float maxSteer = lock * (kMaxSteerStill + (kMaxSteerFast * keep - kMaxSteerStill) * t);

	SetSteeringValue(steer * maxSteer);
	// btRaycastVehicle takes the brake as a cap on the friction impulse per step,
	// so the same figure stops a heavy car more slowly. These were set on the one
	// 1200kg chassis every car used to share; since the car scripts give each its
	// own mass, a 2500kg bus braked at half the rate the race planner counts on,
	// overshot a right-angle corner at 60 km/h and wedged itself sideways.
	// And each car's own brakes (SetBrakeScale, 1 to 16), against the median.
	const float brakes = GetBrakeFactor();
	SetBrake((brake * 100.0f * brakes + 10.0f) * (mass / kBrakeReferenceMass));
}

void Vehicle::SetSetting(const std::string& name, float value)
{
	_settings[name] = value;
	// The springs and dampers, should the script come after the body.
	if (_rayVehicle && (name == "SetSpringK" || name == "SetDamperC"))
	{
		const float spring = std::clamp(GetSetting("SetSpringK", kMedianSpringK) / kMedianSpringK, 0.5f, 2.0f);
		const float damper = std::clamp(GetSetting("SetDamperC", kMedianDamperC) / kMedianDamperC, 0.5f, 3.0f);
		for (int i = 0; i < _rayVehicle->getNumWheels(); ++i)
		{
			btWheelInfo& wheel = _rayVehicle->getWheelInfo(i);
			wheel.m_suspensionStiffness = kSuspensionStiffness * spring;
			wheel.m_wheelsDampingCompression = kSuspensionCompression * damper;
			wheel.m_wheelsDampingRelaxation = kSuspensionDamping * damper;
		}
	}
}

float Vehicle::GetSetting(const std::string& name, float fallback) const
{
	const auto found = _settings.find(name);
	return found != _settings.end() ? found->second : fallback;
}

float Vehicle::GetDamageScale() const
{
	return std::clamp(kMedianHitPoints / std::max(0.01f, GetSetting("SetHitPoints", kMedianHitPoints)), 0.03f, 5.0f);
}

float Vehicle::GetBrakeFactor() const
{
	return std::clamp(GetSetting("SetBrakeScale", kMedianBrakeScale) / kMedianBrakeScale, 0.3f, 2.0f);
}

float Vehicle::GetLockFactor() const
{
	return std::clamp(GetSetting("SetMaxWheelTurnAngle", kMedianWheelTurnDeg) / kMedianWheelTurnDeg, 0.3f, 1.8f);
}

Vector3 Vehicle::centreOfMass() const
{
	// Within the car: the scripts' figures run to a metre.
	return Vector3(std::clamp(GetSetting("SetCMOffsetX", 0.0f), -1.0f, 1.0f),
	               std::clamp(GetSetting("SetCMOffsetY", 0.0f), -1.0f, 1.0f),
	               std::clamp(GetSetting("SetCMOffsetZ", 0.0f), -1.5f, 1.5f));
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
		if (chassis)
			delete chassis->getMotionState();
		delete chassis;
		_rayVehicle.reset();
		_raycaster.reset();
		_chassisShapes.clear();
		Log::Info("Vehicle: physics destroyed for '{}'", _name);
	}
}

void Vehicle::Draw(Matrix4x4& viewProj, GL::ShaderProgram& shader, bool opaque)
{
	// Lit by the level's lights (its textures are not lit, as the world's
	// vertex colours are), then the shader left unlit for the rest.
	if (!_model)
		return;
	shader.Bind();
	shader.SetUniformValue("lighting", 1);
	_model->Draw(shader, viewProj, _model->GetTransform(), opaque);
	shader.SetUniformValue("lighting", 0);
}

Vector3 Vehicle::GetVelocity() const
{
	if (!_rayVehicle)
		return Vector3::Zero;
	const btVector3& v = _rayVehicle->getRigidBody()->getLinearVelocity();
	return Vector3(v.x(), v.y(), v.z());
}

void Vehicle::SetVelocity(const Vector3& velocity)
{
	if (!_rayVehicle)
		return;
	btRigidBody* chassis = _rayVehicle->getRigidBody();
	chassis->activate(true);
	chassis->setLinearVelocity(btVector3(velocity.X, velocity.Y, velocity.Z));
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

void Vehicle::GetFootprint(float& halfWidth, float& halfLength) const
{
	if (!_model || !_model->GetRestFootprint(halfWidth, halfLength))
	{
		halfWidth = 0.9f;
		halfLength = 2.2f;
	}
}

} // namespace Donut
