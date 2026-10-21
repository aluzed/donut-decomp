// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AnimCamera.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"

#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>

#include <algorithm>

namespace Donut
{

std::unique_ptr<AnimCamera> AnimCamera::LoadP3D(const std::string& filename, const std::string& camera)
{
	if (!FileSystem::exists(filename))
	{
		Log::Info("AnimCamera: no {}", filename);
		return nullptr;
	}
	const P3D::P3DFile p3d(filename);
	const P3D::P3DChunk* chosen = nullptr;
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (!chunk->IsType(P3D::ChunkType::Animation))
			continue;
		const auto animation = P3D::Animation::Load(*chunk);
		std::string name = animation->GetName();
		name = name.substr(0, name.find('\0'));
		if (animation->GetType().rfind("CAM", 0) != 0)
			continue;
		if (chosen == nullptr || name == "CAM_" + camera)
			chosen = chunk.get();
		if (name == "CAM_" + camera)
			break;
	}
	auto move = std::make_unique<AnimCamera>();
	if (chosen == nullptr || !move->load(*chosen))
	{
		Log::Info("AnimCamera: no camera move '{}' in {}", camera, filename);
		return nullptr;
	}
	Log::Info("AnimCamera: '{}' from {}, {:.1f}s", camera, filename, move->GetDuration());
	return move;
}

bool AnimCamera::load(const P3D::P3DChunk& chunk)
{
	const auto animation = P3D::Animation::Load(chunk);
	_frames = animation->GetNumFrames();
	_frameRate = animation->GetFrameRate() > 0.0f ? animation->GetFrameRate() : 30.0f;
	if (!animation->GetGroupList())
		return false;
	for (const auto& group : animation->GetGroupList()->GetGroups())
		for (auto [param, track] : {std::pair {"TRAN", &_position}, {"LOOK", &_look}, {"UP", &_up}})
			if (const auto* channel = group->GetVector3ChannelsValue(param))
			{
				track->frames.assign(channel->GetFrames().begin(), channel->GetFrames().end());
				track->values = channel->GetValues();
			}
	return !_position.values.empty() && !_look.values.empty();
}

Vector3 AnimCamera::Track::At(float frame) const
{
	if (values.empty())
		return Vector3(0.0f, 1.0f, 0.0f);
	const auto next = std::upper_bound(frames.begin(), frames.end(), frame);
	if (next == frames.begin())
		return values.front();
	if (next == frames.end())
		return values.back();
	const std::size_t i = static_cast<std::size_t>(next - frames.begin());
	const float t = (frame - frames[i - 1]) / std::max(1.0f, frames[i] - frames[i - 1]);
	return values[i - 1] + (values[i] - values[i - 1]) * t;
}

AnimCamera::Pose AnimCamera::At(double seconds) const
{
	const float frame = std::clamp(static_cast<float>(seconds) * _frameRate, 0.0f, _frames);
	Pose pose {_position.At(frame), _look.At(frame), _up.At(frame)};
	if (pose.look.LengthSquared() > 0.0001f)
		pose.look = pose.look.Normalized();
	return pose;
}

} // namespace Donut
