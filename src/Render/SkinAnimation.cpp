// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "SkinAnimation.h"

#include "Core/Log.h"
#include "P3D/P3D.generated.h"

#include <map>

namespace Donut
{
Matrix4x4 SkinAnimation::Evaluate(size_t trackIndex, float time, bool loop)
{
	const auto& track = _tracks[trackIndex];
	return track->Evaluate(time, loop);
}

Matrix4x4 SkinAnimation::Track::Evaluate(float time, bool loop)
{
	Vector3 translation = _translationKeys.Evalulate(time, Vector3::Zero, loop);
	Quaternion rotation = _rotationKeys.Evalulate(time, Quaternion::Identity, loop);

	Matrix4x4 transMat = Matrix4x4::MakeTranslate(translation);
	Matrix4x4 rotMat(rotation);

	return transMat * rotMat;
}

std::unique_ptr<SkinAnimation> SkinAnimation::FromP3D(const P3D::Animation& p3dAnim,
                                                     const std::vector<std::string>& jointNames,
                                                     const std::vector<Matrix4x4>& restPoses)
{
	auto animation = std::make_unique<SkinAnimation>(p3dAnim.GetName(), p3dAnim.GetNumFrames() / p3dAnim.GetFrameRate(),
	                                                 static_cast<int32_t>(p3dAnim.GetNumFrames()), p3dAnim.GetFrameRate());

	std::map<std::string, const P3D::AnimationGroup*> groups;
	if (const auto& groupList = p3dAnim.GetGroupList())
		for (const auto& group : groupList->GetGroups())
			groups[group->GetName()] = group.get();

	for (std::size_t j = 0; j < jointNames.size(); ++j)
	{
		auto track = std::make_unique<Track>(jointNames[j]);
		const Vector3 restTranslation = restPoses[j].Translation();
		const Quaternion restRotation = restPoses[j].ToQuat();

		const auto found = groups.find(jointNames[j]);
		const P3D::AnimationGroup* group = found != groups.end() ? found->second : nullptr;

		// A translation moves along all three axes (Vector3), or along one or two
		// with the rest held at `constants` (Vector1, Vector2): a swing moves
		// in a plane. Vector1's mapping names the axis that moves; Vector2's
		// the axis that holds, the values filling the other two in order.
		const auto* translation = group ? group->GetVector3ChannelsValue("TRAN") : nullptr;
		const auto* translation1 = group ? group->GetVector1ChannelsValue("TRAN") : nullptr;
		const auto* translation2 = group ? group->GetVector2ChannelsValue("TRAN") : nullptr;
		if (translation)
			for (std::size_t i = 0; i < translation->GetNumFrames(); ++i)
				track->AddTranslationKey(translation->GetFrames()[i], translation->GetValues()[i]);
		else if (translation1)
			for (std::size_t i = 0; i < translation1->GetNumFrames(); ++i)
			{
				Vector3 value = translation1->GetConstants();
				const float moving = translation1->GetValues()[i];
				switch (translation1->GetMapping())
				{
				case 0: value.X = moving; break;
				case 1: value.Y = moving; break;
				default: value.Z = moving; break;
				}
				track->AddTranslationKey(translation1->GetFrames()[i], value);
			}
		else if (translation2)
			for (std::size_t i = 0; i < translation2->GetNumFrames(); ++i)
			{
				Vector3 value = translation2->GetConstants();
				const Vector2 moving = translation2->GetValues()[i];
				switch (translation2->GetMapping())
				{
				case 0: value.Y = moving.X; value.Z = moving.Y; break;
				case 1: value.X = moving.X; value.Z = moving.Y; break;
				default: value.X = moving.X; value.Y = moving.Y; break;
				}
				track->AddTranslationKey(translation2->GetFrames()[i], value);
			}
		else
			track->AddTranslationKey(0, restTranslation);

		const auto* rotation = group ? group->GetQuaternionChannelsValue("ROT") : nullptr;
		const auto* compressed = group ? group->GetCompressedQuaternionChannelsValue("ROT") : nullptr;
		if (rotation)
		{
			for (std::size_t i = 0; i < rotation->GetNumFrames(); ++i)
				track->AddRotationKey(rotation->GetFrames()[i], rotation->GetValues()[i].Normal());
		}
		else if (compressed)
		{
			for (std::size_t i = 0; i < compressed->GetNumFrames(); ++i)
			{
				// Four int16 components, w in the low bits; Quaternion's
				// constructor is (x, y, z, w). Normalised: the int16 values are
				// not exactly unit, and a non-unit quaternion squashes the
				// matrix it makes.
				const uint64_t value = compressed->GetValues()[i];
				const float z = static_cast<int16_t>((value >> 48) & 0xFFFF) / static_cast<float>(0x7FFF);
				const float y = static_cast<int16_t>((value >> 32) & 0xFFFF) / static_cast<float>(0x7FFF);
				const float x = static_cast<int16_t>((value >> 16) & 0xFFFF) / static_cast<float>(0x7FFF);
				const float w = static_cast<int16_t>(value & 0xFFFF) / static_cast<float>(0x7FFF);
				track->AddRotationKey(compressed->GetFrames()[i], Quaternion(x, y, z, w).Normal());
			}
		}
		else
			track->AddRotationKey(0, restRotation);

		animation->AddTrack(track);
	}
	return animation;
}

} // namespace Donut
