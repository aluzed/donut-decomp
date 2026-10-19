// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Core/Log.h>
#include <Game.h>
#include <P3D/P3D.generated.h>
#include <P3D/P3DFile.h>
#include <Render/CompositeModel.h>
#include "Core/FileSystem.h"

namespace Donut
{
CompositeModel_Chunk::CompositeModel_Chunk(const P3D::P3DChunk& chunk)
{
	for (const auto& child : chunk.GetChildren())
	{
		switch (child->GetType())
		{
		case P3D::ChunkType::CompositeDrawable:
		{
			_drawables.push_back(P3D::CompositeDrawable::Load(*child));
			break;
		}
		case P3D::ChunkType::Skeleton:
		{
			_skeletons.push_back(P3D::Skeleton::Load(*child));
			break;
		}
		case P3D::ChunkType::Geometry:
		{
			_meshes.push_back(P3D::Geometry::Load(*child));
			break;
		}
		case P3D::ChunkType::Shader:
		{
			_shaders.push_back(P3D::Shader::Load(*child));
			break;
		}
		case P3D::ChunkType::Texture:
		{
			_textures.push_back(P3D::Texture::Load(*child));
			break;
		}
		case P3D::ChunkType::Animation:
		{
			_animations.push_back(P3D::Animation::Load(*child));
			break;
		}
		default: break;
		}
	}
}

CompositeModel::CompositeModel(const ICompositeModel& provider)
{
	const auto& drawables = provider.GetDrawables();
	const auto& skeletons = provider.GetSkeletons();
	const auto& meshes = provider.GetMeshes();
	const auto& shaders = provider.GetShaders();
	const auto& textures = provider.GetTextures();

	std::map<std::string, size_t> meshNames;
	std::map<std::string, std::vector<Matrix4x4>> jointTransforms;

	for (const auto& meshP3D : meshes)
	{
		meshNames.insert({meshP3D->GetName(), _meshes.size()});
		auto mesh = std::make_unique<Mesh>(*meshP3D);
		mesh->Commit();
		_meshes.push_back(std::move(mesh));
	}

	for (const auto& skeleton : skeletons)
	{
		const auto& skeletonJoints = skeleton->GetJoints();

		std::vector<Matrix4x4> transforms;
		transforms.reserve(skeletonJoints.size());
		for (const auto& joint : skeletonJoints)
			transforms.push_back(transforms.empty() ? Matrix4x4::Identity
			                                        : transforms[joint->GetParent()] * joint->GetRestPose());

		const auto& skeletonName = skeleton->GetName();
		jointTransforms.insert({skeletonName, std::move(transforms)});
	}

	for (const auto& shader : shaders) Game::GetInstance().GetResourceManager().LoadShader(*shader);

	for (const auto& texture : textures) Game::GetInstance().GetResourceManager().LoadTexture(*texture);

	for (const auto& drawable : drawables)
	{
		// A drawable with no props still has its skeleton kept: an effect-only
		// one (a vent's steam) is placed by it.
		const auto& propList = drawable->GetPropList();
		static const std::vector<std::unique_ptr<P3D::CompositeDrawableProp>> noProps;
		const auto& props = propList ? propList->GetProps() : noProps;

		const auto& transforms = jointTransforms[drawable->GetSkeletonName()];

		// The first drawable's skeleton is kept, to pose its props: by the
		// model's pose animation (Pose), or joint by joint (SetJointRotation:
		// a car's wheels and doors).
		const bool posable = _jointNames.empty();
		if (posable)
			for (const auto& skeleton : skeletons)
			{
				if (skeleton->GetName() != drawable->GetSkeletonName())
					continue;
				for (const auto& joint : skeleton->GetJoints())
				{
					_jointNames.push_back(joint->GetName());
					_jointRests.push_back(joint->GetRestPose());
					_jointParents.push_back(static_cast<int>(joint->GetParent()));
				}
				_jointGlobals = transforms;
				_jointRotations.assign(_jointNames.size(), Matrix4x4::Identity);
				// The pose animation ("PTRN"): the same files carry texture
				// animations ("TEX_eyeball_m") that do not move the joints.
				for (const auto& animation : provider.GetAnimations())
					if (animation->GetGroupList() && animation->GetType().rfind("PTRN", 0) == 0)
					{
						_animation = SkinAnimation::FromP3D(*animation, _jointNames, _jointRests);
						break;
					}
				break;
			}

		for (const auto& prop : props)
		{
			const auto& propName = prop->GetName();
			if (meshNames.find(propName) == meshNames.end())
			{
				// Not a mesh: a billboard group, drawn by the level where this
				// joint is.
				if (posable && !_jointNames.empty())
					_propJoints[propName] = static_cast<int>(prop->GetSkeletonJoint());
				continue;
			}

			const auto& transform = transforms[prop->GetSkeletonJoint()];
			_props.push_back(DrawableProp {meshNames[prop->GetName()], transform, prop->GetSkeletonJoint(),
			                               posable && !_jointNames.empty()});
			if (posable && !_jointNames.empty())
				_propJoints[propName] = static_cast<int>(prop->GetSkeletonJoint());
		}
	}
}

int CompositeModel::FindPropJoint(const std::string& prop) const
{
	const auto found = _propJoints.find(prop);
	return found != _propJoints.end() ? found->second : -1;
}

int CompositeModel::FirstPropJoint() const
{
	for (const auto& prop : _props)
		if (prop.posable)
			return static_cast<int>(prop.joint);
	return -1;
}

Matrix4x4 CompositeModel::GetJointTransform(int joint) const
{
	return joint >= 0 && joint < static_cast<int>(_jointGlobals.size()) ? _jointGlobals[joint] : Matrix4x4::Identity;
}

Matrix4x4 CompositeModel::GetEffectTransform(int joint) const
{
	if (_animation || _jointRests.empty())
		return GetJointTransform(joint);
	return _jointRests[0] * GetJointTransform(joint);
}

int CompositeModel::FindJoint(const std::string& name) const
{
	for (std::size_t i = 0; i < _jointNames.size(); ++i)
		if (_jointNames[i] == name)
			return static_cast<int>(i);
	return -1;
}

Vector3 CompositeModel::GetJointRestPosition(int joint) const
{
	return joint >= 0 && joint < static_cast<int>(_jointGlobals.size()) ? _restGlobal(joint).Translation() : Vector3::Zero;
}

Matrix4x4 CompositeModel::_restGlobal(int joint) const
{
	// As the rest pose is built: the root at the model's origin.
	return joint <= 0 ? Matrix4x4::Identity : _restGlobal(_jointParents[joint]) * _jointRests[joint];
}

void CompositeModel::SetJointRotation(int joint, const Quaternion& rotation)
{
	if (joint < 0 || joint >= static_cast<int>(_jointRotations.size()))
		return;
	_jointRotations[joint] = Matrix4x4(rotation);
	if (!_animation)
		updateJoints(-1.0f, false);
}

float CompositeModel::GetAnimationFrames() const
{
	return _animation ? static_cast<float>(_animation->GetFrameCount()) : 0.0f;
}

float CompositeModel::GetAnimationFrameRate() const
{
	return _animation ? _animation->GetFrameRate() : 0.0f;
}

void CompositeModel::Pose(float frame, bool loop)
{
	if (_animation)
		updateJoints(frame, loop);
}

void CompositeModel::updateJoints(float frame, bool loop)
{
	// Each joint placed in its parent's frame: by its animated local transform
	// when there is an animation (the root included), by its rest otherwise
	// (the root at the model's origin, as the rest pose is built); turned by
	// SetJointRotation's rotation either way.
	for (std::size_t i = 0; i < _jointParents.size(); ++i)
	{
		Matrix4x4 local = _animation ? _animation->Evaluate(i, frame, loop) : _jointRests[i];
		if (!_animation && i == 0)
			local = Matrix4x4::Identity;
		local = local * _jointRotations[i];
		_jointGlobals[i] = i == 0 ? local : _jointGlobals[_jointParents[i]] * local;
	}
	for (auto& prop : _props)
		if (prop.posable && prop.joint < _jointGlobals.size())
			prop.transform = _jointGlobals[prop.joint];
}

std::unique_ptr<CompositeModel> CompositeModel::LoadP3D(const std::string& filename)
{
	if (!FileSystem::exists(filename))
	{
		Log::Info("CompositeDrawable not found: {}", filename);
		return nullptr;
	}

	Log::Info("Loading CompositeDrawable: {}", filename);

	const auto p3d = P3D::P3DFile(filename);
	return std::make_unique<CompositeModel>(CompositeModel_Chunk(p3d.GetRoot()));
}

void CompositeModel::Draw(GL::ShaderProgram& shader, const Matrix4x4& viewProj, const Matrix4x4& modelMatrix, bool opaque)
{
	shader.Bind();
	for (const auto& prop : _props)
	{
		shader.SetUniformValue("viewProj", viewProj * modelMatrix * prop.transform);
		_meshes[prop.meshIndex]->Draw(shader, opaque);
	}
}
} // namespace Donut
