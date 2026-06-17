// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "SkinAnimation.h"

namespace Donut
{
Matrix4x4 SkinAnimation::Evaluate(size_t trackIndex, float time)
{
	const auto& track = _tracks[trackIndex];
	return track->Evaluate(time);
}

Matrix4x4 SkinAnimation::Track::Evaluate(float time)
{
	Vector3 translation = _translationKeys.Evalulate(time, Vector3::Zero);
	Quaternion rotation = _rotationKeys.Evalulate(time, Quaternion::Identity);

	Matrix4x4 transMat = Matrix4x4::MakeTranslate(translation);
	Matrix4x4 rotMat(rotation);

	return transMat * rotMat;
}

} // namespace Donut
