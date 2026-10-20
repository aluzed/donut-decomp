// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <P3D/P3D.generated.h>
#include <Render/SkinAnimation.h>
#include <Skeleton.h>
#include <cassert>
#include <cstdint>

namespace Donut
{
Skeleton::Skeleton(const P3D::Skeleton& skeleton): _name(skeleton.GetName())
{
	assert(skeleton.GetVersion() == 0);

	uint32_t jointIndex = 0;
	_joints.reserve(skeleton.GetNumJoints());
	for (const auto& joint : skeleton.GetJoints())
	{
		_joints.emplace_back(joint->GetName(), joint->GetParent(), joint->GetRestPose());
		_jointNameIndexMap[joint->GetName()] = jointIndex++;
	}

	// calculate global transforms through the hierarchy
	for (auto i = 0; i < _joints.size(); i++)
		_joints[i].restGlobalInverse = _joints[_joints[i].parent].restGlobalInverse * _joints[i].rest;

	// inverse those globals afterwards
	for (auto i = 0; i < _joints.size(); i++) _joints[i].restGlobalInverse = _joints[i].restGlobalInverse.Inverse();
}

void Skeleton::ResetPose()
{
	for (auto& joint : _joints) joint.pose = Matrix4x4::Identity;

	updateJoints();
}

void Skeleton::UpdatePose(SkinAnimation& animation, double time, bool loop)
{
	time *= animation.GetFrameRate();

	_joints[0].pose = Matrix4x4::Identity;

	// Motion_Root carries the animation's travel: a walk moves it forward
	// over the cycle, and it jumps back when the cycle starts again. The
	// character is moved by its controller, so the root stays where the first
	// frame has it, across the ground; up and down it keeps.
	const auto motionRoot = _jointNameIndexMap.find("Motion_Root");
	const std::size_t motionIndex = motionRoot != _jointNameIndexMap.end() ? motionRoot->second : _joints.size();
	for (std::size_t i = 0; i < _joints.size(); i++)
	{
		Matrix4x4 local = animation.Evaluate(i, static_cast<float>(time), loop);
		if (i == motionIndex)
		{
			const Vector3 start = animation.Evaluate(i, 0.0f, loop).Translation();
			const Vector3 now = local.Translation();
			local = Matrix4x4::MakeTranslate(Vector3(start.X - now.X, 0.0f, start.Z - now.Z)) * local;
		}
		_joints[i].pose = _joints[_joints[i].parent].pose * local;
	}

	updateJoints();
}

void Skeleton::updateJoints()
{
	for (auto i = 0; i < _joints.size(); i++)
	{
		Joint& j = _joints[i];
		j.finalGlobal = j.pose * j.restGlobalInverse;
	}
}

} // namespace Donut
