// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

#include <algorithm>

namespace Donut
{

class BoundingBox
{
public:
	BoundingBox(): _min(Vector3(0.0f)), _max(Vector3(0.0f)) {}
	BoundingBox(const Vector3& min, const Vector3& max): _min(min), _max(max) {}

	Vector3 GetMin() const { return _min; }
	Vector3 GetMax() const { return _max; }
	Vector3 GetCenter() const { return (_min + _max) * 0.5f; }
	Vector3 GetSize() const { return _max - _min; }

	bool Contains(const Vector3& point) const
	{
		return point.X >= _min.X && point.X <= _max.X &&
		       point.Y >= _min.Y && point.Y <= _max.Y &&
		       point.Z >= _min.Z && point.Z <= _max.Z;
	}

	bool Intersects(const BoundingBox& other) const
	{
		return _min.X <= other._max.X && _max.X >= other._min.X &&
		       _min.Y <= other._max.Y && _max.Y >= other._min.Y &&
		       _min.Z <= other._max.Z && _max.Z >= other._min.Z;
	}

	bool IsInFrontOfPlane(const Vector4& plane) const
	{
		Vector3 p = _min;
		Vector3 n = _max;
		if (plane.X >= 0) { p.X = _max.X; n.X = _min.X; }
		if (plane.Y >= 0) { p.Y = _max.Y; n.Y = _min.Y; }
		if (plane.Z >= 0) { p.Z = _max.Z; n.Z = _min.Z; }
		return (plane.X * p.X + plane.Y * p.Y + plane.Z * p.Z + plane.W) >= 0;
	}

	BoundingBox Transform(const Matrix4x4& m) const
	{
		Vector3 corners[8] = {
			m * Vector3(_min.X, _min.Y, _min.Z),
			m * Vector3(_max.X, _min.Y, _min.Z),
			m * Vector3(_min.X, _max.Y, _min.Z),
			m * Vector3(_min.X, _min.Y, _max.Z),
			m * Vector3(_max.X, _max.Y, _min.Z),
			m * Vector3(_max.X, _min.Y, _max.Z),
			m * Vector3(_min.X, _max.Y, _max.Z),
			m * Vector3(_max.X, _max.Y, _max.Z),
		};
		Vector3 newMin = corners[0], newMax = corners[0];
		for (int i = 1; i < 8; ++i)
		{
			newMin.X = std::min(newMin.X, corners[i].X);
			newMin.Y = std::min(newMin.Y, corners[i].Y);
			newMin.Z = std::min(newMin.Z, corners[i].Z);
			newMax.X = std::max(newMax.X, corners[i].X);
			newMax.Y = std::max(newMax.Y, corners[i].Y);
			newMax.Z = std::max(newMax.Z, corners[i].Z);
		}
		return BoundingBox(newMin, newMax);
	}

	void Merge(const Vector3& point)
	{
		_min.X = std::min(_min.X, point.X);
		_min.Y = std::min(_min.Y, point.Y);
		_min.Z = std::min(_min.Z, point.Z);
		_max.X = std::max(_max.X, point.X);
		_max.Y = std::max(_max.Y, point.Y);
		_max.Z = std::max(_max.Z, point.Z);
	}

	void Expand(float amount)
	{
		_min -= Vector3(amount);
		_max += Vector3(amount);
	}

private:
	Vector3 _min;
	Vector3 _max;
};

} // namespace Donut
