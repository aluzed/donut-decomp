// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <CharacterController.h>
#include <Physics/BulletDebugDraw.h>

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

class btRigidBody;
class btMotionState;
class btCompoundShape;
class btTriangleIndexVertexArray;

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
class CollisionObject;
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

// What the ground is, as the level's collision gives it per triangle (the
// game's terrain types, in its order).
enum class Surface : uint8_t
{
	Road, // and pavement: anything not marked otherwise
	Grass,
	Sand,
	Gravel,
	Water,
	Wood,
	Metal,
	Dirt,
};

class WorldPhysics
{
public:
	WorldPhysics(LineRenderer*);
	~WorldPhysics();

	void Update(float dt);

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

	// A prop a car can knock about (a mailbox, a bin, a lamp post): one rigid
	// body made of `volume`'s leaf volumes, given in the prop's own space, placed
	// by `placement` (rotation and translation). `mass` in kilograms. A `fixed`
	// prop stands as static geometry until released (ReleaseProp); the others
	// are dynamic from the start, asleep until something touches them.
	// `centre` takes the body's transform to the prop's: the body is centred on
	// the volumes, the prop about its base.
	struct PropBody
	{
		btRigidBody* body = nullptr;
		Matrix4x4 centre;
	};
	PropBody AddPropBody(const P3D::CollisionVolume& volume, const Matrix4x4& placement, float mass, bool fixed);
	// A fixed prop, knocked loose by `hitter`: dynamic from now on. The hitter
	// was stopped dead by it, as by a wall; it gets back the speed it had,
	// less what it gave the prop, which leaves with it.
	void ReleaseProp(btRigidBody* body, float mass, const btCollisionObject* hitter);
	// A fixed prop that breaks up as it is hit (a hydrant, a stop sign): out
	// of the world, the hitter given back its speed as above.
	void RemoveProp(btRigidBody* body, float mass, const btCollisionObject* hitter);
	static Matrix4x4 GetBodyTransform(const btRigidBody* body);
	// The boxes of a volume tree on one joint (its volumes' object reference)
	// as one shape, in that joint's space, each child shape added to `owned`;
	// null when it has none. A car's own collision object is its chassis in
	// boxes on its root joint, its wheels in spheres and its doors in boxes on
	// their own joints, in their space.
	static btCompoundShape* CreateBoxesShape(const P3D::CollisionVolume& volume, uint32_t joint,
	                                         std::vector<std::unique_ptr<btCollisionShape>>& owned);
	// Every pair of bodies in touch now, and how hard they push on each other
	// (the impulse their contacts took this step, N·s).
	struct Contact
	{
		const btCollisionObject* a;
		const btCollisionObject* b;
		float impulse;
	};
	std::vector<Contact> GetContacts() const;

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

	// What the ground right under `point` is (a wheel's contact, a foot's):
	// looked for half a metre above it to a metre below. Road where nothing of
	// the level's ground is found.
	Surface GetSurfaceUnder(const Vector3& point) const;

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
	std::vector<std::unique_ptr<btCollisionShape>> _propShapes;
	// The velocity of every moving body before the last step.
	std::unordered_map<const btCollisionObject*, btVector3> _velocitiesBefore;
	// The hitter's speed back, less what it gave a prop of `mass`; the speed it
	// leaves with, zero when there is none to give.
	btVector3 giveBackSpeed(const btCollisionObject* hitter, float mass);
	// While AddKinematicVolumes runs, addStaticBody makes kinematic bodies and
	// lists them here.
	std::vector<KinematicVolume>* _kinematicSink = nullptr;
	int _kinematicOwner = -1;
	// The level's ground meshes' surface, a byte a triangle (Surface, 0x80 set
	// indoors).
	std::unordered_map<const btCollisionObject*, std::vector<uint8_t>> _surfaces;
	// The static bodies' shapes, and the ground meshes' triangles they read.
	std::vector<std::unique_ptr<btCollisionShape>> _staticShapes;
	std::vector<std::unique_ptr<btTriangleIndexVertexArray>> _meshInterfaces;
	std::vector<std::vector<uint32_t>*> _allocatedIndexArrays;
	std::vector<std::vector<Vector3>*> _allocatedVertexArrays;
};
} // namespace Donut
