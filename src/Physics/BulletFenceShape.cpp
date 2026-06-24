// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Physics/BulletFenceShape.h>

#include <LinearMath/btAabbUtil2.h>

namespace Donut
{

// Vertical extrusion (half-height) of the fence wall. The P3D Fence chunk only
// carries start/end/normal (no height), so we use a generous wall that covers
// the typical play-space vertically.
static constexpr btScalar kFenceHalfHeight = 100.0f;

BulletFenceShape::BulletFenceShape(const btVector3& start, const btVector3& end, const btVector3& normal)
    : btConcaveShape(), m_start(start), m_end(end), m_normal(normal.normalized())
{
	m_shapeType = BroadphaseNativeTypes::STATIC_PLANE_PROXYTYPE;
	m_localScaling.setValue(1, 1, 1);
}

void BulletFenceShape::getAabb(const btTransform& t, btVector3& aabbMin, btVector3& aabbMax) const
{
	// Four corners of the vertical wall (local space), transformed into world.
	const btVector3 corners[4] = {
		t * (m_start + btVector3(0, kFenceHalfHeight, 0)),
		t * (m_start - btVector3(0, kFenceHalfHeight, 0)),
		t * (m_end + btVector3(0, kFenceHalfHeight, 0)),
		t * (m_end - btVector3(0, kFenceHalfHeight, 0)),
	};

	aabbMin = corners[0];
	aabbMax = corners[0];
	for (int i = 1; i < 4; ++i)
	{
		aabbMin.setMin(corners[i]);
		aabbMax.setMax(corners[i]);
	}
}

void BulletFenceShape::processAllTriangles(btTriangleCallback* callback, const btVector3& aabbMin,
                                           const btVector3& aabbMax) const
{
	// Build the two triangles of the vertical quad spanned by m_start..m_end.
	// Choose the winding so the resulting face normal aligns with m_normal.
	const btVector3 topStart = m_start + btVector3(0, kFenceHalfHeight, 0);
	const btVector3 botStart = m_start - btVector3(0, kFenceHalfHeight, 0);
	const btVector3 topEnd = m_end + btVector3(0, kFenceHalfHeight, 0);
	const btVector3 botEnd = m_end - btVector3(0, kFenceHalfHeight, 0);

	// Winding A (topStart, botStart, botEnd) produces normal nA = perpendicular
	// to (m_end - m_start) rotated 90° CCW in the XZ plane. Pick the winding
	// whose normal best agrees with m_normal.
	const btVector3 d = m_end - m_start;
	const btVector3 nA = btVector3(-d.z(), 0.0f, d.x());
	const bool flip = nA.dot(m_normal) < 0.0f;

	btVector3 triangle[3];
	if (!flip)
	{
		triangle[0] = topStart;
		triangle[1] = botStart;
		triangle[2] = botEnd;
		callback->processTriangle(triangle, 0, 0);

		triangle[0] = topStart;
		triangle[1] = botEnd;
		triangle[2] = topEnd;
		callback->processTriangle(triangle, 0, 1);
	}
	else
	{
		triangle[0] = topStart;
		triangle[1] = botEnd;
		triangle[2] = botStart;
		callback->processTriangle(triangle, 0, 0);

		triangle[0] = topStart;
		triangle[1] = topEnd;
		triangle[2] = botEnd;
		callback->processTriangle(triangle, 0, 1);
	}
}

void BulletFenceShape::calculateLocalInertia(btScalar mass, btVector3& inertia) const
{
	inertia.setValue(btScalar(0.), btScalar(0.), btScalar(0.));
}

void BulletFenceShape::setLocalScaling(const btVector3& scaling)
{
	m_localScaling = scaling;
}

const btVector3& BulletFenceShape::getLocalScaling() const
{
	return m_localScaling;
}

} // namespace Donut
