// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <CharacterController.h>
#include <Physics/BulletDebugDraw.h>

#include <cstdint>

class btRigidBody;
class btMotionState;

namespace Donut
{

class LineRenderer;

// forward declare some shit
namespace P3D
{
class Intersect;
class CollisionOBBoxVolume;
class CollisionSphere;
class CollisionCylinder;
class CollisionVolume;
class Fence;
}; // namespace P3D

enum class PhysicsDebugDrawMode
{
	NoDebug = 0,
	DrawWireframe = (1 << 0),
	DrawAABB = (1 << 1),
	DrawFeaturesText = (1 << 2),
	DrawContactPoints = (1 << 3),
	DrawText = (1 << 6),
	FastWireframe = (1 << 13),
	DrawNormals = (1 << 14),
	DrawFrames = (1 << 15)
};

class WorldPhysics
{
public:
	WorldPhysics(LineRenderer*);
	~WorldPhysics();

	void Update(float dt) const;

	void AddIntersect(const P3D::Intersect&);
	void AddCollisionVolume(const P3D::CollisionVolume&);
	void AddP3DOBBoxVolume(const P3D::CollisionOBBoxVolume&);
	void AddP3DSphere(const P3D::CollisionSphere&);
	void AddP3DCylinder(const P3D::CollisionCylinder&);
	void AddP3DFence(const P3D::Fence&);

	// A collision volume that moves (an animated object's: the power plant's
	// platforms): a kinematic body, where the volume stands in the file, and
	// which of the collision object's owners it belongs to.
	struct KinematicVolume
	{
		btRigidBody* body;
		Matrix4x4 rest;
		int owner;
	};
	// Every leaf volume of `volume` as a kinematic body.
	std::vector<KinematicVolume> AddKinematicVolumes(const P3D::CollisionVolume& volume);
	// Moves one, to `transform` (rotation and translation only).
	void MoveKinematic(btRigidBody* body, const Matrix4x4& transform) const;

	// Adds `shape` as static (zero-mass) rigid body geometry; see the note in the
	// implementation for why a bare btCollisionObject is not enough.
	class btCollisionObject* addStaticBody(class btCollisionShape* shape, const class btTransform& transform);

	btDiscreteDynamicsWorld* GetDynamicsWorld() const { return _dynamicsWorld; }

	// Height of the ground under `position`, searched from `searchUp` metres above
	// it down to `searchDown` below. False when the ray finds nothing, which means
	// there is no collision geometry there at all.
	//
	// `staticOnly` ignores anything that moves -- cars, the character -- for
	// questions about the level itself: a route probe that met a parked car took
	// its roof for the road.
	bool FindGroundHeight(const Vector3& position, float searchUp, float searchDown, float& outHeight,
	                      bool staticOnly = false) const;

	// First thing hit going `range` metres along `direction` from `from`. Used to
	// ask what a stuck car is actually pressed against: a route that runs through
	// a wall and a car that has simply spun both look like "not moving" from the
	// outside, and they need opposite fixes.
	bool CastRay(const Vector3& from, const Vector3& direction, float range, Vector3& outPoint, Vector3& outNormal,
	             bool staticOnly = false) const;

	void SetDebugDrawMode(PhysicsDebugDrawMode mode) const { _debugDraw->setDebugMode((int)mode); }
	PhysicsDebugDrawMode GetDebugDrawMode() const { return (PhysicsDebugDrawMode)_debugDraw->getDebugMode(); }

private:
	btDiscreteDynamicsWorld* _dynamicsWorld;

	btDefaultCollisionConfiguration* _collisionConfiguration;
	btSequentialImpulseConstraintSolver* _constraintSolver;
	btCollisionDispatcher* _collisionDispatcher;
	btBroadphaseInterface* _broadphase;
	// required for btPairCachingGhostObject to accumulate overlapping pairs
	class btGhostPairCallback* _ghostPairCallback;

	std::unique_ptr<BulletDebugDraw> _debugDraw;

	// allocated objects for cleanup
	std::vector<btCollisionObject*> _allocatedCollisionObjects;
	std::vector<std::unique_ptr<btMotionState>> _motionStates;
	// While AddKinematicVolumes runs, addStaticBody makes kinematic bodies and
	// lists them here.
	std::vector<KinematicVolume>* _kinematicSink = nullptr;
	int _kinematicOwner = -1;
	std::vector<std::vector<uint32_t>*> _allocatedIndexArrays;
	std::vector<std::vector<Vector3>*> _allocatedVertexArrays;
};
} // namespace Donut
