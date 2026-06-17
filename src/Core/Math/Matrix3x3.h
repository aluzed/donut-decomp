// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform.h"

#include <string>

namespace Donut
{

struct Matrix3x3
{
	float M[3][3];

	FORCEINLINE Matrix3x3();
	FORCEINLINE Matrix3x3(float xx, float xy, float xz, float yx, float yy, float yz, float zx, float zy, float zz);

	FORCEINLINE Matrix3x3 operator*(const Matrix3x3& Other) const;
	FORCEINLINE Vector3 operator*(const Vector3& vec) const;
	FORCEINLINE void operator*=(const Matrix3x3& Other);
	FORCEINLINE Matrix3x3 operator+(const Matrix3x3& Other) const;
	FORCEINLINE void operator+=(const Matrix3x3& Other);
	FORCEINLINE Matrix3x3 operator*(float Other) const;
	FORCEINLINE void operator*=(float Other);
	inline bool operator==(const Matrix3x3& Other) const;
	inline bool operator!=(const Matrix3x3& Other) const;

	FORCEINLINE Matrix3x3 operator-() const;

	inline Matrix3x3 Inverse() const;
	Matrix3x3 Transpose() const;

	Quaternion Quat() const;

	std::string ToString() const;

	static const Matrix3x3 Identity;
};

#pragma warning(suppress : 26495)
FORCEINLINE Matrix3x3::Matrix3x3() {}

FORCEINLINE Matrix3x3::Matrix3x3(float xx, float xy, float xz, float yx, float yy, float yz, float zx, float zy, float zz)
{
	M[0][0] = xx;
	M[0][1] = xy;
	M[0][2] = xz;
	M[1][0] = yx;
	M[1][1] = yy;
	M[1][2] = yz;
	M[2][0] = zx;
	M[2][1] = zy;
	M[2][2] = zz;
}

FORCEINLINE Matrix3x3 Matrix3x3::operator*(const Matrix3x3& o) const
{
	return Matrix3x3(
		M[0][0] * o.M[0][0] + M[0][1] * o.M[1][0] + M[0][2] * o.M[2][0],
		M[0][0] * o.M[0][1] + M[0][1] * o.M[1][1] + M[0][2] * o.M[2][1],
		M[0][0] * o.M[0][2] + M[0][1] * o.M[1][2] + M[0][2] * o.M[2][2],
		M[1][0] * o.M[0][0] + M[1][1] * o.M[1][0] + M[1][2] * o.M[2][0],
		M[1][0] * o.M[0][1] + M[1][1] * o.M[1][1] + M[1][2] * o.M[2][1],
		M[1][0] * o.M[0][2] + M[1][1] * o.M[1][2] + M[1][2] * o.M[2][2],
		M[2][0] * o.M[0][0] + M[2][1] * o.M[1][0] + M[2][2] * o.M[2][0],
		M[2][0] * o.M[0][1] + M[2][1] * o.M[1][1] + M[2][2] * o.M[2][1],
		M[2][0] * o.M[0][2] + M[2][1] * o.M[1][2] + M[2][2] * o.M[2][2]);
}

FORCEINLINE Vector3 Matrix3x3::operator*(const Vector3& v) const
{
	return Vector3(M[0][0] * v.X + M[0][1] * v.Y + M[0][2] * v.Z, M[1][0] * v.X + M[1][1] * v.Y + M[1][2] * v.Z,
	               M[2][0] * v.X + M[2][1] * v.Y + M[2][2] * v.Z);
}

FORCEINLINE void Matrix3x3::operator*=(const Matrix3x3& o)
{
	*this = *this * o;
}

FORCEINLINE Matrix3x3 Matrix3x3::operator+(const Matrix3x3& o) const
{
	return Matrix3x3(
		M[0][0] + o.M[0][0], M[0][1] + o.M[0][1], M[0][2] + o.M[0][2],
		M[1][0] + o.M[1][0], M[1][1] + o.M[1][1], M[1][2] + o.M[1][2],
		M[2][0] + o.M[2][0], M[2][1] + o.M[2][1], M[2][2] + o.M[2][2]);
}

FORCEINLINE void Matrix3x3::operator+=(const Matrix3x3& o)
{
	M[0][0] += o.M[0][0]; M[0][1] += o.M[0][1]; M[0][2] += o.M[0][2];
	M[1][0] += o.M[1][0]; M[1][1] += o.M[1][1]; M[1][2] += o.M[1][2];
	M[2][0] += o.M[2][0]; M[2][1] += o.M[2][1]; M[2][2] += o.M[2][2];
}

FORCEINLINE Matrix3x3 Matrix3x3::operator*(float s) const
{
	return Matrix3x3(
		M[0][0] * s, M[0][1] * s, M[0][2] * s,
		M[1][0] * s, M[1][1] * s, M[1][2] * s,
		M[2][0] * s, M[2][1] * s, M[2][2] * s);
}

FORCEINLINE void Matrix3x3::operator*=(float s)
{
	M[0][0] *= s; M[0][1] *= s; M[0][2] *= s;
	M[1][0] *= s; M[1][1] *= s; M[1][2] *= s;
	M[2][0] *= s; M[2][1] *= s; M[2][2] *= s;
}

inline bool Matrix3x3::operator==(const Matrix3x3& o) const
{
	return M[0][0] == o.M[0][0] && M[0][1] == o.M[0][1] && M[0][2] == o.M[0][2] &&
	       M[1][0] == o.M[1][0] && M[1][1] == o.M[1][1] && M[1][2] == o.M[1][2] &&
	       M[2][0] == o.M[2][0] && M[2][1] == o.M[2][1] && M[2][2] == o.M[2][2];
}

inline bool Matrix3x3::operator!=(const Matrix3x3& o) const
{
	return !(*this == o);
}

FORCEINLINE Matrix3x3 Matrix3x3::operator-() const
{
	return Matrix3x3(-M[0][0], -M[0][1], -M[0][2], -M[1][0], -M[1][1], -M[1][2], -M[2][0], -M[2][1], -M[2][2]);
}

} // namespace Donut
