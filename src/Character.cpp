// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Character.h>
#include <CharacterController.h>
#include <Game.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Physics/BulletCast.h>
#include <Render/OpenGL/ShaderProgram.h>
#include <Render/SkinModel.h>
#include <Skeleton.h>
#include "Core/FileSystem.h"
#include <fmt/format.h>

namespace Donut
{

Character::Character(std::string name): _name(std::move(name)), _position(Vector3(0.0f)), _rotation(Quaternion::Identity)
{
	_characterController = std::make_unique<CharacterController>(this, &Game::GetInstance().GetWorldPhysics());
	_boneBuffer = std::make_unique<GL::TextureBuffer>();
}

Character::~Character() = default;

void Character::LoadModel(const std::string& name)
{
	// we need to empty
	_skinModel.reset(new SkinModel());

	_modelName = name;

	const std::string modelPath = fmt::format("art/chars/{0}_m.p3d", name);
	const P3D::P3DFile p3d(modelPath);

	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::Shader: Game::GetInstance().GetResourceManager().LoadShader(*P3D::Shader::Load(*chunk)); break;
		case P3D::ChunkType::Texture: Game::GetInstance().GetResourceManager().LoadTexture(*P3D::Texture::Load(*chunk)); break;
		case P3D::ChunkType::PolySkin: _skinModel->LoadPolySkin(*P3D::PolySkin::Load(*chunk)); break;
		case P3D::ChunkType::Skeleton: _skeleton = std::make_unique<Skeleton>(*P3D::Skeleton::Load(*chunk)); break;
		// Part of P3D's composite-drawable / animation system. We render the
		// character directly from its PolySkin + Skeleton and load its
		// animations from the separate <name>_a.p3d, so these are intentionally
		// ignored here (acknowledged to keep the loader output clean).
		case P3D::ChunkType::CompositeDrawable:
		case P3D::ChunkType::MultiController:
		case P3D::ChunkType::Animation:
		case P3D::ChunkType::FrameController: break;
		default: fmt::print("unhandled chunk {:#x} in character {}\n", static_cast<uint32_t>(chunk->GetType()), name); break;
		}
	}
}

void Character::LoadAnimations(const std::string& name)
{
	_animations.clear();

	const std::string animPath = fmt::format("art/chars/{0}_a.p3d", name);
	// const std::string choPath = fmt::format("art/chars/{0}.cho", name);

	if (!FileSystem::exists(animPath))
		return;

	const P3D::P3DFile p3d(animPath);
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (!chunk->IsType(P3D::ChunkType::Animation))
			continue;

		addAnimation(*P3D::Animation::Load(*chunk));
	}

	// default to using the first in the map
	SetAnimation(_animations.begin()->first);
}

void Character::Draw(const Matrix4x4& viewProjection, GL::ShaderProgram& shaderProgram, const ResourceManager& rm)
{
	Matrix4x4 model = Matrix4x4::MakeTranslate(_position) * Matrix4x4(_rotation);
	Matrix4x4 mvp = viewProjection * model;

	shaderProgram.Bind();
	shaderProgram.SetUniformValue("viewProj", mvp);
	shaderProgram.SetUniformValue("diffuseTex", 0);
	shaderProgram.SetUniformValue("boneBuffer", 1);

	glActiveTexture(GL_TEXTURE1);
	_boneBuffer->Bind();

	_skinModel->Draw();
}

void Character::SetAnimation(const std::string& animationName)
{
	if (_animations.find(animationName) == _animations.end())
		return;

	_currentAnimation = _animations.at(animationName).get();
	_animName = animationName;

	_animTime = 0.0;
	Update(0);
}

void Character::Update(double deltatime)
{
	// update bone buffers
	_animTime += deltatime;

	if (_currentAnimation != nullptr)
		_skeleton->UpdatePose(*_currentAnimation, _animTime);

	// update our bonebuffer
	auto joints = _skeleton->GetJoints();
	std::vector<Matrix4x4> matrices(joints.size());
	for (auto i = 0; i < joints.size(); i++) matrices[i] = joints[i].finalGlobal;

	// NOTE: the compressed-quaternion animation decode is still wrong and
	// produces non-orthonormal bone matrices that flatten the skinned mesh.
	// Until that's fixed, render the bind pose (identity bones) so the
	// character at least stands correctly instead of collapsing. See the
	// animation-decode ticket.
	for (auto& m : matrices) m = Matrix4x4::Identity;

	_boneBuffer->SetBuffer(matrices.data(), matrices.size() * sizeof(Matrix4x4));
}

void Character::SetPosition(const Vector3& position)
{
	_position = position;
	_characterController->warp(BulletCast<btVector3>(position));
}
void Character::SetRotation(const Quaternion& rotation)
{
	_rotation = rotation;
}

void Character::addAnimation(const P3D::Animation& p3dAnim)
{
	const auto& animGroupList = p3dAnim.GetGroupList();
	if (!animGroupList)
		return;

	auto animation = std::make_unique<SkinAnimation>(p3dAnim.GetName(), p3dAnim.GetNumFrames() / p3dAnim.GetFrameRate(),
	                                                 static_cast<int32_t>(p3dAnim.GetNumFrames()), p3dAnim.GetFrameRate());

	const auto& groups = animGroupList->GetGroups();
	std::map<std::string, size_t> groupNameIndex;
	for (const auto& group : groups) { groupNameIndex.insert({group->GetName(), groupNameIndex.size()}); }

	for (auto const& joint : _skeleton->GetJoints())
	{
		auto track = std::make_unique<SkinAnimation::Track>(joint.name);

		const auto& jointRestPose = joint.rest;
		const auto& jointTranslation = jointRestPose.Translation();
		const auto& jointRotation = jointRestPose.ToQuat();

		if (groupNameIndex.find(joint.name) == groupNameIndex.end())
		{
			track->AddTranslationKey(0, jointTranslation);
			track->AddRotationKey(0, jointRotation);
		}
		else
		{
			const auto& animGroup = groups.at(groupNameIndex.at(joint.name));
			const auto& vector2Channel = animGroup->GetVector2ChannelsValue("TRAN");
			const auto& vector3Channel = animGroup->GetVector3ChannelsValue("TRAN");
			const auto& quaternionChannel = animGroup->GetQuaternionChannelsValue("ROT");
			const auto& compressedQuaternionChannel = animGroup->GetCompressedQuaternionChannelsValue("ROT");

			if (vector3Channel)
			{
				const auto& frames = vector3Channel->GetFrames();
				const auto& values = vector3Channel->GetValues();

				for (std::size_t i = 0; i < vector3Channel->GetNumFrames(); ++i)
				{ track->AddTranslationKey(frames[i], values[i]); }
			}
			else
			{
				track->AddTranslationKey(0, jointTranslation);
			}

			if (compressedQuaternionChannel)
			{
				const auto& frames = compressedQuaternionChannel->GetFrames();
				const auto& values = compressedQuaternionChannel->GetValues();

				for (std::size_t i = 0; i < compressedQuaternionChannel->GetNumFrames(); ++i)
				{
					const uint64_t& value = values[i];
					float z = (int16_t)((value >> 48) & 0xFFFF) / (float)0x7FFF;
					float y = (int16_t)((value >> 32) & 0xFFFF) / (float)0x7FFF;
					float x = (int16_t)((value >> 16) & 0xFFFF) / (float)0x7FFF;
					float w = (int16_t)(value & 0xFFFF) / (float)0x7FFF;

					// Quaternion's ctor is (x, y, z, w). Normalize: the int16
					// compressed components aren't exactly unit, and a non-unit
					// quaternion makes ToRotationMatrix produce a non-orthonormal
					// (squashing) matrix — which flattened the skinned body.
					track->AddRotationKey(frames[i], Quaternion(x, y, z, w).Normal());
				}
			}
			else
			{
				track->AddRotationKey(0, jointRotation);
			}
		}

		animation->AddTrack(track);
	}

	_animations[p3dAnim.GetName()] = std::move(animation);
}

} // namespace Donut
