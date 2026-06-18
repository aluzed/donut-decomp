#pragma once

#include <btBulletDynamicsCommon.h>
#include <functional>
#include <vector>

namespace Donut
{

class CollisionCallbacks : public btCollisionWorld::ContactResultCallback
{
public:
	using Callback = std::function<void(const btCollisionObject*, const btCollisionObject*, const btManifoldPoint&)>;

	CollisionCallbacks() = default;

	btScalar addSingleResult(btManifoldPoint& cp,
	                         const btCollisionObject* colObj0, int partId0, int index0,
	                         const btCollisionObject* colObj1, int partId1, int index1) override
	{
		for (auto& cb : _callbacks)
			cb(colObj0, colObj1, cp);
		return 0.f;
	}

	void AddCallback(Callback cb) { _callbacks.push_back(std::move(cb)); }

private:
	std::vector<Callback> _callbacks;
};

struct VehicleDamage
{
	float health = 100.0f;
	bool destroyed = false;

	void ApplyDamage(float amount)
	{
		health -= amount;
		if (health <= 0.0f)
		{
			health = 0.0f;
			destroyed = true;
		}
	}
};

} // namespace Donut
