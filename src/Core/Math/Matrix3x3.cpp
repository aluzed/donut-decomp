// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Matrix3x3.h"

#include "Core/Math/Math.h"
#include "Core/Math/Quaternion.h"

namespace Donut
{

const Matrix3x3 Matrix3x3::Identity(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

Quaternion Matrix3x3::Quat() const
{
	return Quaternion(*this);
}

Matrix3x3 Matrix3x3::Transpose() const
{
	return Matrix3x3(M[0][0], M[1][0], M[2][0], M[0][1], M[1][1], M[2][1], M[0][2], M[1][2], M[2][2]);
}

Matrix3x3 Matrix3x3::Inverse() const
{
	float det = M[0][0] * (M[1][1] * M[2][2] - M[2][1] * M[1][2])
	          - M[0][1] * (M[1][0] * M[2][2] - M[2][0] * M[1][2])
	          + M[0][2] * (M[1][0] * M[2][1] - M[2][0] * M[1][1]);

	float invDet = 1.0f / det;

	return Matrix3x3(
		(M[1][1] * M[2][2] - M[2][1] * M[1][2]) * invDet,
		(M[0][2] * M[2][1] - M[0][1] * M[2][2]) * invDet,
		(M[0][1] * M[1][2] - M[0][2] * M[1][1]) * invDet,
		(M[1][2] * M[2][0] - M[1][0] * M[2][2]) * invDet,
		(M[0][0] * M[2][2] - M[0][2] * M[2][0]) * invDet,
		(M[1][0] * M[0][2] - M[0][0] * M[1][2]) * invDet,
		(M[1][0] * M[2][1] - M[2][0] * M[1][1]) * invDet,
		(M[2][0] * M[0][1] - M[0][0] * M[2][1]) * invDet,
		(M[0][0] * M[1][1] - M[1][0] * M[0][1]) * invDet);
}

} // namespace Donut
