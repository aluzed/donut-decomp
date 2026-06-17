// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "AnimCamera.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Render/SkinAnimation.h"

#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>

namespace Donut
{
AnimCamera::AnimCamera(const P3D::P3DChunk& chunk): _time(0.0)
{
	for (const auto& child : chunk.GetChildren())
	{
		switch (child->GetType())
		{
		case P3D::ChunkType::Animation:
		{
			auto animation = P3D::Animation::Load(*child);
			_trans =
			    std::make_unique<SkinAnimation>(animation->GetName(), animation->GetNumFrames() / animation->GetFrameRate(),
			                                    static_cast<int32_t>(animation->GetNumFrames()), animation->GetFrameRate());

			_forward =
			    std::make_unique<SkinAnimation>(animation->GetName(), animation->GetNumFrames() / animation->GetFrameRate(),
			                                    static_cast<int32_t>(animation->GetNumFrames()), animation->GetFrameRate());

			_up = std::make_unique<SkinAnimation>(animation->GetName(), animation->GetNumFrames() / animation->GetFrameRate(),
			                                      static_cast<int32_t>(animation->GetNumFrames()), animation->GetFrameRate());

			for (auto const& group : animation->GetGroupList()->GetGroups())
			{
				auto transTrack = std::make_unique<SkinAnimation::Track>(group->GetName());
				auto forwardTrack = std::make_unique<SkinAnimation::Track>(group->GetName());
				auto upTrack = std::make_unique<SkinAnimation::Track>(group->GetName());

				if (const auto& vector3Channel = group->GetVector3ChannelsValue("TRAN"))
				{
					const auto& frames = vector3Channel->GetFrames();
					const auto& values = vector3Channel->GetValues();

					for (std::size_t i = 0; i < vector3Channel->GetNumFrames(); ++i)
					{ transTrack->AddTranslationKey(frames[i], values[i]); }
				}

				_trans->AddTrack(transTrack);
				_forward->AddTrack(forwardTrack);
				_up->AddTrack(upTrack);
			}

			break;
		}
		case P3D::ChunkType::Camera:
		{
			auto camera = P3D::Camera::Load(*child);
			break;
		}
		case P3D::ChunkType::MultiController:
		{
			auto multiController = P3D::MultiController::Load(*child);
			break;
		}
		default: break;
		}
	}
}

std::unique_ptr<AnimCamera> AnimCamera::LoadP3D(const std::string& filename)
{
	if (!FileSystem::exists(filename))
	{
		Log::Info("AnimCamera not found: {}", filename);
		return nullptr;
	}

	Log::Info("Loading AnimCamera: {}", filename);

	const auto p3d = P3D::P3DFile(filename);
	return std::make_unique<AnimCamera>(p3d.GetRoot());
}

Matrix4x4 AnimCamera::Update(double dt)
{
	return Matrix4x4::Identity;
}
} // namespace Donut
