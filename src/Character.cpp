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
#include "Core/Log.h"

#include <cstdint>

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
		default: Log::Warn("unhandled chunk {:#x} in character {}", static_cast<uint32_t>(chunk->GetType()), name); break;
		}
	}
}

bool Character::HasMesh() const
{
	return _skinModel && _skinModel->HasMesh();
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
	// Unanimated, a skin is drawn in its bind pose, which identity bones give.
	for (auto i = 0; i < joints.size(); i++)
		matrices[i] = _currentAnimation != nullptr ? joints[i].finalGlobal : Matrix4x4::Identity;

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
	if (!p3dAnim.GetGroupList())
		return;

	std::vector<std::string> names;
	std::vector<Matrix4x4> rests;
	for (const auto& joint : _skeleton->GetJoints())
	{
		names.push_back(joint.name);
		rests.push_back(joint.rest);
	}
	_animations[p3dAnim.GetName()] = SkinAnimation::FromP3D(p3dAnim, names, rests);
}

} // namespace Donut
