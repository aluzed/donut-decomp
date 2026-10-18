#include "SceneSkins.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "P3D/P3D.generated.h"
#include "P3D/P3DFile.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/TextureBuffer.h"
#include "Render/SkinAnimation.h"
#include "Render/SkinModel.h"
#include "Skeleton.h"

#include <cmath>

namespace Donut
{

SceneSkins::SceneSkins() = default;
SceneSkins::~SceneSkins() = default;

std::unique_ptr<SceneSkins> SceneSkins::Load(const std::string& file)
{
	if (!FileSystem::exists(file))
		return nullptr;
	const P3D::P3DFile p3d(file);

	std::vector<std::unique_ptr<P3D::PolySkin>> polySkins;
	std::vector<std::unique_ptr<P3D::Skeleton>> skeletons;
	std::vector<std::unique_ptr<P3D::Animation>> animations;
	std::optional<TextureAnimation> eyesFound;
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::PolySkin: polySkins.push_back(P3D::PolySkin::Load(*chunk)); break;
		case P3D::ChunkType::Skeleton: skeletons.push_back(P3D::Skeleton::Load(*chunk)); break;
		case P3D::ChunkType::Animation:
			animations.push_back(P3D::Animation::Load(*chunk));
			if (auto eyes = TextureAnimation::FromChunk(*chunk))
				eyesFound = std::move(eyes);
			break;
		// Their shaders and textures are loaded with the props (CompositeModel).
		default: break;
		}
	}
	if (polySkins.empty())
		return nullptr;

	// The skeleton the skins are bound to: one per scene, holding the props'
	// joints and the characters' alike.
	const P3D::Skeleton* skeleton = nullptr;
	for (const auto& candidate : skeletons)
		if (candidate->GetName() == polySkins.front()->GetSkeletonName())
			skeleton = candidate.get();
	if (!skeleton)
	{
		Log::Warn("SceneSkins: no skeleton '{}' in {}", polySkins.front()->GetSkeletonName(), file);
		return nullptr;
	}

	std::unique_ptr<SceneSkins> scene(new SceneSkins());
	scene->_skeleton = std::make_unique<Skeleton>(*skeleton);
	scene->_boneBuffer = std::make_unique<GL::TextureBuffer>();
	for (const auto& polySkin : polySkins)
	{
		if (polySkin->GetSkeletonName() != skeleton->GetName())
			continue;
		auto skin = std::make_unique<SkinModel>();
		skin->LoadPolySkin(*polySkin);
		scene->_skins.push_back(std::move(skin));
	}

	std::vector<std::string> names;
	std::vector<Matrix4x4> rests;
	for (const auto& joint : scene->_skeleton->GetJoints())
	{
		names.push_back(joint.name);
		rests.push_back(joint.rest);
	}
	for (const auto& animation : animations)
		if (animation->GetGroupList() && animation->GetType().rfind("PTRN", 0) == 0)
		{
			scene->_animation = SkinAnimation::FromP3D(*animation, names, rests);
			break;
		}
	scene->_eyes = std::move(eyesFound);
	scene->Pose(0.0f, false);
	Log::Info("SceneSkins: {} skin(s) in {}{}", scene->_skins.size(), file,
	          scene->_animation ? "" : ", not animated");
	return scene;
}

void SceneSkins::Pose(float frame, bool loop)
{
	_frame = loop && _animation && _animation->GetFrameCount() > 0
	             ? std::fmod(frame, static_cast<float>(_animation->GetFrameCount()))
	             : frame;
	std::vector<Matrix4x4> matrices(_skeleton->GetJoints().size(), Matrix4x4::Identity);
	if (_animation)
	{
		_skeleton->UpdatePose(*_animation, frame / _animation->GetFrameRate(), loop);
		const auto& joints = _skeleton->GetJoints();
		for (std::size_t i = 0; i < joints.size(); ++i)
			matrices[i] = joints[i].finalGlobal;
	}
	_boneBuffer->SetBuffer(matrices.data(), matrices.size() * sizeof(Matrix4x4));
}

void SceneSkins::Draw(const Matrix4x4& viewProjection, const Matrix4x4& model, GL::ShaderProgram& skinShader)
{
	skinShader.Bind();
	skinShader.SetUniformValue("viewProj", viewProjection * model);
	skinShader.SetUniformValue("diffuseTex", 0);
	skinShader.SetUniformValue("boneBuffer", 1);
	glActiveTexture(GL_TEXTURE1);
	_boneBuffer->Bind();
	// The eyes as the scene has them at this frame: open, shut, half.
	Texture* eyes = _eyes ? Game::GetInstance().GetResourceManager().GetTexture(_eyes->TextureAt(_frame)) : nullptr;
	for (const auto& skin : _skins)
		skin->Draw(_eyes ? _eyes->shader : std::string(), eyes);
}

} // namespace Donut
