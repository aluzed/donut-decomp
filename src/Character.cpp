// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Core/MemoryStream.h"
#include <Character.h>
#include <CharacterController.h>
#include <Game.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Physics/BulletCast.h>
#include <Render/Mesh.h>
#include <Render/OpenGL/ShaderProgram.h>
#include <Render/SkinModel.h>
#include <Skeleton.h>

#include <cstdint>
#include <map>

namespace Donut
{

namespace
{
// A mouth fully open: the jaw turned this far (radians), about its own Z (seen
// from the front, the one that opens it straight down; X and Y twist it).
// Not more: Marge's hair is partly bound to her jaw.
constexpr float kJawOpen = 0.25f;
constexpr float kJawAxis[3] = {0.0f, 0.0f, 1.0f};
} // namespace

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

	_rigidProps.clear();
	std::map<std::string, const P3D::P3DChunk*> geometries;
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		switch (chunk->GetType())
		{
		case P3D::ChunkType::Geometry: geometries[P3D::Geometry::Load(*chunk)->GetName()] = chunk.get(); break;
		case P3D::ChunkType::Shader: Game::GetInstance().GetResourceManager().LoadShader(*P3D::Shader::Load(*chunk)); break;
		case P3D::ChunkType::Texture: Game::GetInstance().GetResourceManager().LoadTexture(*P3D::Texture::Load(*chunk)); break;
		case P3D::ChunkType::PolySkin: _skinModel->LoadPolySkin(*P3D::PolySkin::Load(*chunk)); break;
		case P3D::ChunkType::Skeleton: _skeleton = std::make_unique<Skeleton>(*P3D::Skeleton::Load(*chunk)); break;
		// Part of P3D's composite-drawable / animation system. We render the
		// character directly from its PolySkin + Skeleton and load its
		// animations from the separate <name>_a.p3d, so these are intentionally
		// ignored here (acknowledged to keep the loader output clean).
		// The eyes' blink; the pose animations are in <name>_a.p3d.
		case P3D::ChunkType::Animation:
			if (auto blink = TextureAnimation::FromChunk(*chunk))
				_blink = std::move(blink);
			break;
		case P3D::ChunkType::CompositeDrawable:
		{
			// Its props: the skin, and any rigid mesh on a joint.
			const auto drawable = P3D::CompositeDrawable::Load(*chunk);
			if (const auto& props = drawable->GetPropList())
				for (const auto& prop : props->GetProps())
					if (const auto found = geometries.find(prop->GetName()); found != geometries.end())
					{
						auto mesh = std::make_unique<Mesh>(*P3D::Geometry::Load(*found->second));
						mesh->Commit();
						_rigidProps.push_back({std::move(mesh), prop->GetSkeletonJoint()});
					}
			break;
		}
		case P3D::ChunkType::MultiController:
		case P3D::ChunkType::FrameController: break;
		default: Log::Warn("unhandled chunk {:#x} in character {}", static_cast<uint32_t>(chunk->GetType()), name); break;
		}
	}
}

const Skeleton::Joint* Character::FindJoint(const std::string& name) const
{
	if (!_skeleton)
		return nullptr;
	for (const auto& joint : _skeleton->GetJoints())
		if (joint.name == name)
			return &joint;
	return nullptr;
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
	// At its feet: the model's origin, the capsule's middle less its height.
	Matrix4x4 model = Matrix4x4::MakeTranslate(_position - Vector3(0.0f, CentreHeight(), 0.0f)) * Matrix4x4(_rotation);
	Matrix4x4 mvp = viewProjection * model;

	shaderProgram.Bind();
	shaderProgram.SetUniformValue("viewProj", mvp);
	shaderProgram.SetUniformValue("diffuseTex", 0);
	shaderProgram.SetUniformValue("boneBuffer", 1);

	glActiveTexture(GL_TEXTURE1);
	_boneBuffer->Bind();

	// Mid-blink, the eyes' shader draws the key's texture instead of its own.
	Texture* eyes = nullptr;
	if (_blink && _blinkTime >= 0.0)
		eyes = rm.GetTexture(_blink->TextureAt(static_cast<float>(_blinkTime * _blink->frameRate)));
	_skinModel->Draw(_blink ? _blink->shader : std::string(), eyes);
	if (!_rigidProps.empty())
		drawRigidProps(viewProjection, model);
}

void Character::drawRigidProps(const Matrix4x4& viewProjection, const Matrix4x4& model)
{
	if (!_skeleton || !Game::GetInstance().HasLevel())
		return;
	GL::ShaderProgram& world = Game::GetInstance().GetLevel().GetWorldShader();
	world.Bind();
	world.SetUniformValue("lighting", 0);
	for (const auto& prop : _rigidProps)
	{
		if (prop.joint >= _skeleton->GetJoints().size())
			continue;
		const Skeleton::Joint& joint = _skeleton->GetJoint(prop.joint);
		// The joint where it is now; unanimated, at rest, as the skin is drawn.
		const Matrix4x4 placed = _currentAnimation != nullptr ? joint.pose : joint.restGlobalInverse.Inverse();
		world.SetUniformValue("viewProj", viewProjection * model * placed);
		prop.mesh->Draw(world, true);
		prop.mesh->Draw(world, false);
	}
}

bool Character::IsAnimationFinished() const
{
	return !_animLoops && _currentAnimation != nullptr && _animTime >= _currentAnimation->GetLength();
}

float Character::GetAnimationProgress() const
{
	if (_currentAnimation == nullptr || _currentAnimation->GetLength() <= 0.0f)
		return 0.0f;
	const double t = _animTime / _currentAnimation->GetLength();
	return static_cast<float>(_animLoops ? t - std::floor(t) : std::min(t, 1.0));
}

void Character::SetAnimation(const std::string& animationName, bool loop)
{
	if (_animations.find(animationName) == _animations.end())
		return;

	_currentAnimation = _animations.at(animationName).get();
	_animName = animationName;
	_animLoops = loop;

	_animTime = 0.0;
	Update(0);
}

void Character::Update(double deltatime)
{
	// A blink every few seconds.
	if (_blink)
	{
		_blinkAt -= deltatime;
		if (_blinkTime >= 0.0)
			_blinkTime += deltatime;
		if (_blinkTime * _blink->frameRate > _blink->keys.back().first)
			_blinkTime = -1.0;
		if (_blinkAt <= 0.0)
		{
			_blinkTime = 0.0;
			_blinkAt = 2.5 + (std::rand() % 30) / 10.0;
		}
	}

	// update bone buffers
	_animTime += deltatime;

	if (_currentAnimation != nullptr)
		_skeleton->UpdatePose(*_currentAnimation, _animTime, _animLoops);
	// Its mouth, opened by its jaw as it speaks.
	if (_jaw == -2)
	{
		const auto* jaw = FindJoint("Jaw");
		_jaw = jaw ? static_cast<int>(jaw - _skeleton->GetJoints().data()) : -1;
	}
	if (_jaw >= 0 && _mouthOpen > 0.0f && _currentAnimation != nullptr)
		_skeleton->TurnJoint(static_cast<std::size_t>(_jaw),
		                     Matrix4x4(Quaternion(Vector3(kJawAxis[0], kJawAxis[1], kJawAxis[2]), _mouthOpen * kJawOpen)));

	// update our bonebuffer
	auto joints = _skeleton->GetJoints();
	std::vector<Matrix4x4> matrices(joints.size());
	// Unanimated, a skin is drawn in its bind pose, which identity bones give.
	for (auto i = 0; i < joints.size(); i++)
		matrices[i] = _currentAnimation != nullptr ? joints[i].finalGlobal : Matrix4x4::Identity;

	_boneBuffer->SetBuffer(matrices.data(), matrices.size() * sizeof(Matrix4x4));
}

float Character::CentreHeight() const
{
	return _characterController->GetCentreHeight();
}

void Character::SetPosition(const Vector3& feet)
{
	SetCentre(feet + Vector3(0.0f, CentreHeight(), 0.0f));
}

void Character::SetCentre(const Vector3& centre)
{
	_position = centre;
	_characterController->warp(BulletCast<btVector3>(centre));
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
