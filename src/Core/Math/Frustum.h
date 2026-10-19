// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/BoundingBox.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector4.h"

namespace Donut
{

// The six planes of what a projection sees (Gribb and Hartmann: each a row of
// the matrix added to or taken from the fourth). Taken from a model's whole
// transform (viewProj * model), they are in the model's own space, so its
// bounds are tested as they are, untransformed.
class Frustum
{
public:
	explicit Frustum(const Matrix4x4& m)
	{
		// Column-major storage: row i of the matrix is M[0..3][i].
		const auto row = [&](int i) { return Vector4(m.M[0][i], m.M[1][i], m.M[2][i], m.M[3][i]); };
		const Vector4 x = row(0), y = row(1), z = row(2), w = row(3);
		_planes[0] = w + x;
		_planes[1] = w - x;
		_planes[2] = w + y;
		_planes[3] = w - y;
		_planes[4] = w + z;
		_planes[5] = w - z;
	}

	// Whether any of `box` may be seen: false only when it lies wholly behind
	// one of the planes.
	bool Intersects(const BoundingBox& box) const
	{
		for (const auto& plane : _planes)
			if (!box.IsInFrontOfPlane(plane))
				return false;
		return true;
	}

private:
	Vector4 _planes[6];
};

// How many meshes the last frame drew and how many it left out, unseen.
struct CullStats
{
	static inline int drawn = 0;
	static inline int culled = 0;
	static void Count(bool seen) { ++(seen ? drawn : culled); }
};

} // namespace Donut
