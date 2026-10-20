// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Physics/WorldPhysics.h"
#include "P3D/P3D.generated.h"
#include "Physics/BulletCast.h"
#include "Physics/BulletDebugDraw.h"
#include "Physics/BulletFenceShape.h"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionShapes/btBox2dShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LinearMath/btDefaultMotionState.h>

#include <cstdint>
#include <functional>

namespace Donut
{
WorldPhysics::WorldPhysics(LineRenderer* lineRenderer)
{
	_collisionConfiguration = new btDefaultCollisionConfiguration();
	_collisionDispatcher = new btCollisionDispatcher(_collisionConfiguration);
	_broadphase = new btDbvtBroadphase();
	_constraintSolver = new btSequentialImpulseConstraintSolver();

	_dynamicsWorld = new btDiscreteDynamicsWorld(_collisionDispatcher, _broadphase, _constraintSolver, _collisionConfiguration);

	// btPairCachingGhostObject (the character capsule) only accumulates overlapping
	// pairs if this callback is installed on the broadphase. Without it the ghost
	// reports no contacts at all, so CharacterController::recoverFromPenetration
	// never de-penetrates and horizontal movement has no collision whatsoever.
	_ghostPairCallback = new btGhostPairCallback();
	_broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(_ghostPairCallback);

	_debugDraw = std::make_unique<BulletDebugDraw>(lineRenderer);
	_debugDraw->setDebugMode(btIDebugDraw::DBG_NoDebug);

	_dynamicsWorld->setDebugDrawer(_debugDraw.get());
	_dynamicsWorld->setGravity(btVector3(0.0f, -9.8f, 0.0f));

	// _char = std::make_unique<CharacterController>(this, Vector3(Vector3(229.0f, 4.5f, -182.0f)));
}

WorldPhysics::~WorldPhysics()
{
	for (auto i = 0; i < _allocatedCollisionObjects.size(); i++)
	{
		if (_dynamicsWorld != nullptr)
			_dynamicsWorld->removeCollisionObject(_allocatedCollisionObjects[i]);

		delete _allocatedCollisionObjects[i];
	}

	_allocatedCollisionObjects.clear();

	for (auto i = 0; i < _allocatedVertexArrays.size(); i++) delete _allocatedVertexArrays[i];

	_allocatedVertexArrays.clear();

	for (auto i = 0; i < _allocatedIndexArrays.size(); i++) delete _allocatedIndexArrays[i];

	_allocatedIndexArrays.clear();

	delete _dynamicsWorld;
	delete _constraintSolver;
	delete _ghostPairCallback;
	delete _collisionConfiguration;
	delete _collisionDispatcher;
	delete _broadphase;
}

void WorldPhysics::Update(const float dt)
{
	_velocitiesBefore.clear();
	for (int i = 0; i < _dynamicsWorld->getNonStaticRigidBodies().size(); ++i)
	{
		const btRigidBody* body = _dynamicsWorld->getNonStaticRigidBodies()[i];
		if (body->isActive())
			_velocitiesBefore[body] = body->getLinearVelocity();
	}

	_dynamicsWorld->stepSimulation(dt);
	_dynamicsWorld->debugDrawWorld();
}

namespace
{
// The user index a prop's body carries.
constexpr int kPropBody = 1;

// The closest hit, optionally among static objects only.
struct LevelRayCallback : btCollisionWorld::ClosestRayResultCallback
{
	bool staticOnly;
	LevelRayCallback(const btVector3& from, const btVector3& to, bool onlyStatic)
	    : ClosestRayResultCallback(from, to), staticOnly(onlyStatic) {}

	bool needsCollision(btBroadphaseProxy* proxy) const override
	{
		// A prop is not the level, even standing: a lamp post by the road
		// held a passing van up on its top.
		const auto* object = static_cast<btCollisionObject*>(proxy->m_clientObject);
		if (staticOnly && (!object->isStaticObject() || object->getUserIndex() == kPropBody))
			return false;
		return ClosestRayResultCallback::needsCollision(proxy);
	}
};
} // namespace

bool WorldPhysics::FindGroundHeight(const Vector3& position, float searchUp, float searchDown, float& outHeight,
                                    bool staticOnly) const
{
	const btVector3 from(position.X, position.Y + searchUp, position.Z);
	const btVector3 to(position.X, position.Y - searchDown, position.Z);

	LevelRayCallback callback(from, to, staticOnly);
	_dynamicsWorld->rayTest(from, to, callback);
	if (!callback.hasHit())
		return false;

	outHeight = callback.m_hitPointWorld.getY();
	return true;
}

bool WorldPhysics::CastRay(const Vector3& from, const Vector3& direction, float range, Vector3& outPoint,
                           Vector3& outNormal, bool staticOnly) const
{
	const btVector3 start(from.X, from.Y, from.Z);
	const btVector3 end(from.X + direction.X * range, from.Y + direction.Y * range, from.Z + direction.Z * range);

	LevelRayCallback callback(start, end, staticOnly);
	_dynamicsWorld->rayTest(start, end, callback);
	if (!callback.hasHit())
		return false;

	outPoint = Vector3(callback.m_hitPointWorld.getX(), callback.m_hitPointWorld.getY(), callback.m_hitPointWorld.getZ());
	outNormal =
	    Vector3(callback.m_hitNormalWorld.getX(), callback.m_hitNormalWorld.getY(), callback.m_hitNormalWorld.getZ());
	return true;
}

namespace
{
// The level's ground under a ray, and which of its triangles.
struct SurfaceRayCallback : LevelRayCallback
{
	int triangle = -1;
	SurfaceRayCallback(const btVector3& from, const btVector3& to): LevelRayCallback(from, to, true) {}

	btScalar addSingleResult(btCollisionWorld::LocalRayResult& result, bool normalInWorldSpace) override
	{
		const btScalar fraction = LevelRayCallback::addSingleResult(result, normalInWorldSpace);
		triangle = result.m_localShapeInfo ? result.m_localShapeInfo->m_triangleIndex : -1;
		return fraction;
	}
};
} // namespace

Surface WorldPhysics::GetSurfaceUnder(const Vector3& point) const
{
	const btVector3 from(point.X, point.Y + 0.5f, point.Z);
	const btVector3 to(point.X, point.Y - 1.0f, point.Z);
	SurfaceRayCallback callback(from, to);
	_dynamicsWorld->rayTest(from, to, callback);
	if (!callback.hasHit() || callback.triangle < 0)
		return Surface::Road;
	const auto found = _surfaces.find(callback.m_collisionObject);
	if (found == _surfaces.end() || static_cast<std::size_t>(callback.triangle) >= found->second.size())
		return Surface::Road;
	const uint8_t type = found->second[callback.triangle] & 0x7F;
	return type <= static_cast<uint8_t>(Surface::Dirt) ? static_cast<Surface>(type) : Surface::Road;
}

btCollisionObject* WorldPhysics::addStaticBody(btCollisionShape* shape, const btTransform& transform)
{
	// Static geometry has to be a zero-mass btRigidBody, not a bare
	// btCollisionObject: btDefaultVehicleRaycaster (the wheel raycaster behind
	// btRaycastVehicle) discards any hit whose object does not upcast to
	// btRigidBody. With plain collision objects the wheels never found the
	// ground, so vehicles had no suspension and no traction and sank through
	// the world -- while the character, which sweeps the world directly, was
	// unaffected.
	if (_kinematicSink)
	{
		// Moved by hand each frame, through its motion state.
		auto motionState = std::make_unique<btDefaultMotionState>(transform);
		btRigidBody::btRigidBodyConstructionInfo info(0.0f, motionState.get(), shape, btVector3(0, 0, 0));
		auto* body = new btRigidBody(info);
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		body->setActivationState(DISABLE_DEACTIVATION);
		_dynamicsWorld->addRigidBody(body);
		_allocatedCollisionObjects.push_back(body);
		_motionStates.push_back(std::move(motionState));
		Matrix4x4 rest = Matrix4x4(BulletCast<Quaternion>(transform.getRotation()));
		rest = Matrix4x4::MakeTranslate(BulletCast<Vector3>(transform.getOrigin())) * rest;
		_kinematicSink->push_back({body, rest, _kinematicOwner});
		return body;
	}

	btRigidBody::btRigidBodyConstructionInfo info(0.0f, nullptr, shape, btVector3(0, 0, 0));
	info.m_startWorldTransform = transform;

	auto* body = new btRigidBody(info);
	body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	_dynamicsWorld->addRigidBody(body);

	_allocatedCollisionObjects.push_back(body);
	return body;
}

std::vector<WorldPhysics::KinematicVolume> WorldPhysics::AddKinematicVolumes(const P3D::CollisionVolume& volume)
{
	std::vector<KinematicVolume> volumes;
	std::function<void(const P3D::CollisionVolume&)> add = [&](const P3D::CollisionVolume& v) {
		if (v.GetNumSubVolumes() > 0)
		{
			for (const auto& sub : v.GetSubVolumes())
				add(*sub);
			return;
		}
		_kinematicSink = &volumes;
		_kinematicOwner = v.GetOwnerIndex();
		AddCollisionVolume(v);
		_kinematicSink = nullptr;
	};
	add(volume);
	return volumes;
}

void WorldPhysics::MoveKinematic(btRigidBody* body, const Matrix4x4& transform) const
{
	btTransform t;
	t.setIdentity();
	t.setOrigin(BulletCast<btVector3>(transform.Translation()));
	t.setRotation(BulletCast<btQuaternion>(transform.ToQuat().Normal()));
	body->getMotionState()->setWorldTransform(t);
	body->setWorldTransform(t);
}

void WorldPhysics::AddIntersect(const P3D::Intersect& intersect)
{
	// copy this shit over first (todo: free it?)
	auto verts = new std::vector<Vector3>(intersect.GetPositions());
	auto indices = new std::vector<uint32_t>(intersect.GetIndices());

	btIndexedMesh indexedMesh;
	indexedMesh.m_vertexBase = reinterpret_cast<const unsigned char*>(verts->data());
	indexedMesh.m_vertexStride = sizeof(Vector3);
	indexedMesh.m_numVertices = (int)verts->size();
	indexedMesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(indices->data());
	indexedMesh.m_triangleIndexStride = sizeof(uint32_t) * 3;
	indexedMesh.m_numTriangles = (int)indices->size() / 3;

	auto meshInterface = new btTriangleIndexVertexArray();
	meshInterface->addIndexedMesh(indexedMesh, PHY_INTEGER);

	auto trimeshShape = new btBvhTriangleMeshShape(meshInterface, true);

	const btCollisionObject* body = addStaticBody(trimeshShape, btTransform::getIdentity());
	if (!intersect.GetSurfaceTypes().empty())
		_surfaces[body] = intersect.GetSurfaceTypes();

	// add for cleanup
	_allocatedVertexArrays.push_back(verts);
	_allocatedIndexArrays.push_back(indices);
}

void WorldPhysics::AddCollisionVolume(const P3D::CollisionVolume& volume)
{
	// process subvolumes
	if (volume.GetNumSubVolumes() > 0)
	{
		for (auto const& subvolume : volume.GetSubVolumes()) AddCollisionVolume(*subvolume);

		// if it has sub volumes it probably doesn't have an actual volume so just return
		return;
	}

	// process volume
	auto const& obbox = volume.GetObBox();
	auto const& sphere = volume.GetSphere();
	auto const& cylinder = volume.GetCylinder();

	if (obbox != nullptr)
		AddP3DOBBoxVolume(*obbox);
	else if (sphere != nullptr)
		AddP3DSphere(*sphere);
	else if (cylinder != nullptr)
		AddP3DCylinder(*cylinder);
}

namespace
{
// A leaf volume's shape, and where it stands in the space its volume is given in.
btCollisionShape* obboxShape(const P3D::CollisionOBBoxVolume& volume, btTransform& transform)
{
	const auto centre = volume.GetVectors()[0];
	const auto rotX = volume.GetVectors()[1];
	const auto rotY = volume.GetVectors()[2];
	const auto rotZ = volume.GetVectors()[3];

	const Quaternion rotation = Matrix3x3(rotX.X, rotX.Y, rotX.Z, rotY.X, rotY.Y, rotY.Z, rotZ.X, rotZ.Y, rotZ.Z).Quat();

	transform.setIdentity();
	transform.setOrigin(BulletCast<btVector3>(centre));
	transform.setRotation(BulletCast<btQuaternion>(rotation));
	return new btBoxShape(BulletCast<btVector3>(volume.GetHalfExtents()));
}

btCollisionShape* sphereShape(const P3D::CollisionSphere& sphere, btTransform& transform)
{
	transform.setIdentity();
	transform.setOrigin(BulletCast<btVector3>(sphere.GetVectors()[0]));
	return new btSphereShape(sphere.GetRadius());
}

btCollisionShape* cylinderShape(const P3D::CollisionCylinder& cylinder, btTransform& transform)
{
	const float radius = cylinder.GetRadius();
	const float halfLength = cylinder.GetLength();
	Vector3 cylDir = cylinder.GetVectors()[1];
	Vector3 up(0.0f, 1.0f, 0.0f);
	Vector3 axis = up.Cross(cylDir);
	float axisLen = axis.Length();
	Quaternion rotation;
	if (axisLen > 0.0001f)
	{
		axis = axis.Normalized();
		cylDir = cylDir.Normalized();
		float angle = acosf(up.Dot(cylDir));
		rotation = Quaternion(axis, angle);
	}
	else
	{
		rotation = Quaternion::Identity;
	}

	transform.setIdentity();
	transform.setOrigin(BulletCast<btVector3>(cylinder.GetVectors()[0]));
	transform.setRotation(BulletCast<btQuaternion>(rotation));
	if (cylinder.GetFlatEnd() == 1)
		return new btCylinderShape(btVector3(radius, halfLength, radius));
	return new btCapsuleShape(radius, halfLength * 2);
}

btCollisionShape* leafShape(const P3D::CollisionVolume& volume, btTransform& transform)
{
	if (volume.GetObBox())
		return obboxShape(*volume.GetObBox(), transform);
	if (volume.GetSphere())
		return sphereShape(*volume.GetSphere(), transform);
	if (volume.GetCylinder())
		return cylinderShape(*volume.GetCylinder(), transform);
	return nullptr;
}
} // namespace

void WorldPhysics::AddP3DOBBoxVolume(const P3D::CollisionOBBoxVolume& volume)
{
	btTransform worldTransform;
	auto* shape = obboxShape(volume, worldTransform);
	addStaticBody(shape, worldTransform);
}

void WorldPhysics::AddP3DSphere(const P3D::CollisionSphere& sphere)
{
	btTransform worldTransform;
	auto* shape = sphereShape(sphere, worldTransform);
	addStaticBody(shape, worldTransform);
}

void WorldPhysics::AddP3DCylinder(const P3D::CollisionCylinder& cylinder)
{
	btTransform worldTransform;
	auto* shape = cylinderShape(cylinder, worldTransform);
	addStaticBody(shape, worldTransform);
}

WorldPhysics::PropBody WorldPhysics::AddPropBody(const P3D::CollisionVolume& volume, const Matrix4x4& placement,
                                                 float mass, bool fixed)
{
	std::vector<std::pair<btCollisionShape*, btTransform>> leaves;
	std::function<void(const P3D::CollisionVolume&)> collect = [&](const P3D::CollisionVolume& v) {
		for (const auto& sub : v.GetSubVolumes())
			collect(*sub);
		btTransform local;
		if (auto* shape = leafShape(v, local))
			leaves.emplace_back(shape, local);
	};
	collect(volume);
	if (leaves.empty())
		return {};

	// Centred on its volumes, so that it tips and turns about the middle of
	// what it is rather than its base.
	btVector3 centre(0, 0, 0);
	for (const auto& leaf : leaves)
		centre += leaf.second.getOrigin();
	centre /= static_cast<btScalar>(leaves.size());

	auto compound = std::make_unique<btCompoundShape>();
	for (auto& [shape, local] : leaves)
	{
		local.setOrigin(local.getOrigin() - centre);
		compound->addChildShape(local, shape);
		_propShapes.emplace_back(shape);
	}

	btTransform start;
	start.setIdentity();
	start.setOrigin(BulletCast<btVector3>(placement.Translation()));
	start.setRotation(BulletCast<btQuaternion>(placement.ToQuat().Normal()));
	btTransform offset;
	offset.setIdentity();
	offset.setOrigin(centre);
	start = start * offset;

	auto motionState = std::make_unique<btDefaultMotionState>(start);
	btVector3 inertia(0, 0, 0);
	if (!fixed)
		compound->calculateLocalInertia(mass, inertia);
	btRigidBody::btRigidBodyConstructionInfo info(fixed ? 0.0f : mass, motionState.get(), compound.get(), inertia);
	info.m_friction = 0.6f;
	info.m_restitution = 0.2f;
	// Slowed as it tumbles, or it rolls down the street for ever.
	info.m_linearDamping = 0.1f;
	info.m_angularDamping = 0.3f;
	auto* body = new btRigidBody(info);
	body->setUserIndex(kPropBody);
	if (fixed)
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	_dynamicsWorld->addRigidBody(body);
	if (!fixed)
		// Standing still where it was put, until something runs into it.
		body->setActivationState(ISLAND_SLEEPING);

	_allocatedCollisionObjects.push_back(body);
	_motionStates.push_back(std::move(motionState));
	_propShapes.push_back(std::move(compound));

	return {body, Matrix4x4::MakeTranslate(BulletCast<Vector3>(-centre))};
}

void WorldPhysics::ReleaseProp(btRigidBody* body, float mass, const btCollisionObject* hitter)
{
	_dynamicsWorld->removeRigidBody(body);
	btVector3 inertia(0, 0, 0);
	body->getCollisionShape()->calculateLocalInertia(mass, inertia);
	body->setMassProps(mass, inertia);
	body->setCollisionFlags(body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
	body->updateInertiaTensor();
	_dynamicsWorld->addRigidBody(body);
	body->activate(true);
	body->setLinearVelocity(giveBackSpeed(hitter, mass) * 1.2f);
}

void WorldPhysics::RemoveProp(btRigidBody* body, float mass, const btCollisionObject* hitter)
{
	_dynamicsWorld->removeRigidBody(body);
	giveBackSpeed(hitter, mass);
}

btVector3 WorldPhysics::giveBackSpeed(const btCollisionObject* hitter, float mass)
{
	if (!hitter)
		return btVector3(0, 0, 0);
	auto* other = const_cast<btRigidBody*>(btRigidBody::upcast(hitter));
	const auto before = _velocitiesBefore.find(hitter);
	if (!other || other->getInvMass() <= 0.0f || before == _velocitiesBefore.end())
		return btVector3(0, 0, 0);
	const float hitterMass = 1.0f / other->getInvMass();
	const btVector3 after = before->second * (hitterMass / (hitterMass + mass));
	other->setLinearVelocity(after);
	return after;
}

Matrix4x4 WorldPhysics::GetBodyTransform(const btRigidBody* body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);
	Matrix4x4 m = Matrix4x4(BulletCast<Quaternion>(t.getRotation()));
	return Matrix4x4::MakeTranslate(BulletCast<Vector3>(t.getOrigin())) * m;
}

std::vector<WorldPhysics::Contact> WorldPhysics::GetContacts() const
{
	std::vector<Contact> contacts;
	const int manifolds = _collisionDispatcher->getNumManifolds();
	for (int i = 0; i < manifolds; ++i)
	{
		const btPersistentManifold* manifold = _collisionDispatcher->getManifoldByIndexInternal(i);
		if (manifold->getNumContacts() == 0)
			continue;
		float impulse = 0.0f;
		for (int c = 0; c < manifold->getNumContacts(); ++c)
			impulse += manifold->getContactPoint(c).getAppliedImpulse();
		contacts.push_back({manifold->getBody0(), manifold->getBody1(), impulse});
	}
	return contacts;
}

void WorldPhysics::AddP3DFence(const P3D::Fence& fence)
{
	// BulletFenceShape stores LOCAL coordinates relative to the collision
	// object's world transform. We anchor the transform at the fence start so
	// the shape is expressed in that local frame (start = origin, end = delta).
	const btVector3 start = BulletCast<btVector3>(fence.GetStart());
	const btVector3 end = BulletCast<btVector3>(fence.GetEnd());

	auto fenceShape = new BulletFenceShape(
		btVector3(0, 0, 0),
		end - start,
		BulletCast<btVector3>(fence.GetNormal()));

	btTransform worldTransform;
	worldTransform.setIdentity();
	worldTransform.setOrigin(start);

	addStaticBody(fenceShape, worldTransform);
}

} // namespace Donut
