// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Physics/WorldPhysics.h"
#include "P3D/P3D.generated.h"
#include "Physics/BulletCast.h"
#include "Physics/BulletDebugDraw.h"
#include "Physics/BulletFenceShape.h"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionShapes/btBox2dShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#include <cstdint>

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

void WorldPhysics::Update(const float dt) const
{
	_dynamicsWorld->stepSimulation(dt);
	_dynamicsWorld->debugDrawWorld();
}

bool WorldPhysics::FindGroundHeight(const Vector3& position, float searchUp, float searchDown, float& outHeight) const
{
	const btVector3 from(position.X, position.Y + searchUp, position.Z);
	const btVector3 to(position.X, position.Y - searchDown, position.Z);

	btCollisionWorld::ClosestRayResultCallback callback(from, to);
	_dynamicsWorld->rayTest(from, to, callback);
	if (!callback.hasHit())
		return false;

	outHeight = callback.m_hitPointWorld.getY();
	return true;
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
	btRigidBody::btRigidBodyConstructionInfo info(0.0f, nullptr, shape, btVector3(0, 0, 0));
	info.m_startWorldTransform = transform;

	auto* body = new btRigidBody(info);
	body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	_dynamicsWorld->addRigidBody(body);

	_allocatedCollisionObjects.push_back(body);
	return body;
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

	addStaticBody(trimeshShape, btTransform::getIdentity());

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

void WorldPhysics::AddP3DOBBoxVolume(const P3D::CollisionOBBoxVolume& volume)
{
	const auto centre = volume.GetVectors()[0];
	const auto rotX = volume.GetVectors()[1];
	const auto rotY = volume.GetVectors()[2];
	const auto rotZ = volume.GetVectors()[3];

	const Quaternion rotation = Matrix3x3(rotX.X, rotX.Y, rotX.Z, rotY.X, rotY.Y, rotY.Z, rotZ.X, rotZ.Y, rotZ.Z).Quat();

	const auto he = volume.GetHalfExtents();
	const auto bulletShape = new btBoxShape(BulletCast<btVector3>(volume.GetHalfExtents()));

	btTransform worldTransform;
	worldTransform.setIdentity();
	worldTransform.setOrigin(BulletCast<btVector3>(centre));
	worldTransform.setRotation(BulletCast<btQuaternion>(rotation));

	addStaticBody(bulletShape, worldTransform);
}

void WorldPhysics::AddP3DSphere(const P3D::CollisionSphere& sphere)
{
	const auto bulletSphere = new btSphereShape(sphere.GetRadius());

	btTransform worldTransform;
	worldTransform.setIdentity();
	worldTransform.setOrigin(BulletCast<btVector3>(sphere.GetVectors()[0]));

	addStaticBody(bulletSphere, worldTransform);
}

void WorldPhysics::AddP3DCylinder(const P3D::CollisionCylinder& cylinder)
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

	btConvexShape* shape = nullptr;
	if (cylinder.GetFlatEnd() == 1)
		shape = new btCylinderShape(btVector3(radius, halfLength, radius));
	else
		shape = new btCapsuleShape(radius, halfLength * 2);

	btTransform worldTransform;
	worldTransform.setIdentity();
	worldTransform.setOrigin(BulletCast<btVector3>(cylinder.GetVectors()[0]));
	worldTransform.setRotation(BulletCast<btQuaternion>(rotation));

	addStaticBody(shape, worldTransform);
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
