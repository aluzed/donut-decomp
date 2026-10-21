// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Render/BillboardBatch.h"
#include "Render/Mesh.h"
#include "ResourceManager.h"

#include <map>
#include <string>

namespace Donut
{
class ICompositeModel
{
public:
	virtual const std::vector<std::unique_ptr<P3D::CompositeDrawable>>& GetDrawables() const = 0;
	virtual const std::vector<std::unique_ptr<P3D::Skeleton>>& GetSkeletons() const = 0;
	virtual const std::vector<std::unique_ptr<P3D::Geometry>>& GetMeshes() const = 0;
	virtual const std::vector<std::unique_ptr<P3D::Shader>>& GetShaders() const = 0;
	virtual const std::vector<std::unique_ptr<P3D::Texture>>& GetTextures() const = 0;
	// The animations that move the skeleton's joints (gags, doors); none by default.
	virtual const std::vector<std::unique_ptr<P3D::Animation>>& GetAnimations() const
	{
		static const std::vector<std::unique_ptr<P3D::Animation>> none;
		return none;
	}
};

class CompositeModel_AnimObjectWrapper: public ICompositeModel
{
public:
	CompositeModel_AnimObjectWrapper(const P3D::AnimObjectWrapper& animObjectWrapper): _animObjectWrapper(animObjectWrapper) {}

	const std::vector<std::unique_ptr<P3D::CompositeDrawable>>& GetDrawables() const override
	{
		return _animObjectWrapper.GetCompositeDrawables();
	}
	const std::vector<std::unique_ptr<P3D::Skeleton>>& GetSkeletons() const override
	{
		return _animObjectWrapper.GetSkeletons();
	}
	const std::vector<std::unique_ptr<P3D::Geometry>>& GetMeshes() const override { return _animObjectWrapper.GetGeometries(); }
	const std::vector<std::unique_ptr<P3D::Shader>>& GetShaders() const override { return _shaders; }
	const std::vector<std::unique_ptr<P3D::Texture>>& GetTextures() const override { return _textures; }
	const std::vector<std::unique_ptr<P3D::Animation>>& GetAnimations() const override
	{
		return _animObjectWrapper.GetAnimations();
	}

private:
	const P3D::AnimObjectWrapper& _animObjectWrapper;
	std::vector<std::unique_ptr<P3D::Shader>> _shaders;
	std::vector<std::unique_ptr<P3D::Texture>> _textures;
};

class CompositeModel_Chunk: public ICompositeModel
{
public:
	CompositeModel_Chunk(const P3D::P3DChunk&);

	const std::vector<std::unique_ptr<P3D::CompositeDrawable>>& GetDrawables() const override { return _drawables; }
	const std::vector<std::unique_ptr<P3D::Skeleton>>& GetSkeletons() const override { return _skeletons; }
	const std::vector<std::unique_ptr<P3D::Geometry>>& GetMeshes() const override { return _meshes; }
	const std::vector<std::unique_ptr<P3D::Shader>>& GetShaders() const override { return _shaders; }
	const std::vector<std::unique_ptr<P3D::Texture>>& GetTextures() const override { return _textures; }
	const std::vector<std::unique_ptr<P3D::Animation>>& GetAnimations() const override { return _animations; }

private:
	std::vector<std::unique_ptr<P3D::Animation>> _animations;
	std::vector<std::unique_ptr<P3D::CompositeDrawable>> _drawables;
	std::vector<std::unique_ptr<P3D::Skeleton>> _skeletons;
	std::vector<std::unique_ptr<P3D::Geometry>> _meshes;
	std::vector<std::unique_ptr<P3D::BillboardQuadGroup>> _quadGroups;
	std::vector<std::unique_ptr<P3D::Shader>> _shaders;
	std::vector<std::unique_ptr<P3D::Texture>> _textures;
};

class CompositeModel
{
public:
	CompositeModel(const ICompositeModel&);

	static std::unique_ptr<CompositeModel> LoadP3D(const std::string&);

	void Draw(GL::ShaderProgram&, const Matrix4x4&, const Matrix4x4&, bool);

	void SetTransform(const Matrix4x4& transform) { _transform = transform; }
	// Hidden, it draws nothing (a breakable's breaking, waiting to be played).
	void SetVisible(bool visible) { _visible = visible; }
	bool IsVisible() const { return _visible; }
	const Matrix4x4& GetTransform() const { return _transform; }

	// The model's first animation, if it has one: its length in frames and its
	// rate. Pose() moves the props to `frame` of it; with `loop`, a frame past
	// the end wraps round, otherwise it holds the last.
	bool IsAnimated() const { return _animation != nullptr; }
	float GetAnimationFrames() const;
	float GetAnimationFrameRate() const;
	void Pose(float frame, bool loop);

	// A joint of the model's skeleton by name ("w0", "DoorDRot"), or -1; where
	// it stands at rest, in the model's space; and a rotation of it, in its own
	// frame, on top of its rest or animation (a wheel turning, a door opening).
	int FindJoint(const std::string& name) const;
	Vector3 GetJointRestPosition(int joint) const;
	void SetJointRotation(int joint, const Quaternion& rotation);
	// The same, moved too, by `offset` in the joint's own frame (a wheel riding
	// up on its suspension).
	void SetJointRotation(int joint, const Quaternion& rotation, const Vector3& offset);
	// How far its props on that joint reach from it, half their height: a
	// wheel's radius. 0 when none hangs from it.
	float GetJointPropRadius(int joint) const;
	// The lowest point of its props at rest, in the model's space.
	float GetRestBottom() const;
	// The joint a prop hangs from, by the prop's name (a mesh's), or -1; the
	// first posable prop's; and a joint's transform as posed now, in the model's
	// space.
	int FindPropJoint(const std::string& prop) const;
	// One of its meshes, by name; null when it has no such.
	Mesh* FindMesh(const std::string& name) const;
	// Shows or hides the props of that name (a state prop's: a wasp's head
	// gives way to its exploding one).
	void SetPropVisible(const std::string& prop, bool visible);
	// A visibility animation ("PVIS": a breaking's pieces, gone one by one),
	// which Pose plays too: each prop, shown or not at the start, turning at
	// these frames.
	struct VisibilityTrack
	{
		std::string prop;
		bool startsVisible = true;
		std::vector<float> toggles;
	};
	void SetVisibilityAnimation(std::vector<VisibilityTrack> tracks) { _visibilityTracks = std::move(tracks); }
	int FirstPropJoint() const;
	Matrix4x4 GetJointTransform(int joint) const;
	// Where an effect at that joint stands (a particle system): as the joint,
	// but with the root's own rest -- which the props ignore, and which places
	// an effect-only skeleton (a vent's steam) in the world.
	Matrix4x4 GetEffectTransform(int joint) const;

private:
	struct DrawableProp
	{
		size_t meshIndex;
		Matrix4x4 transform;
		// The skeleton joint it hangs from, for Pose.
		size_t joint = 0;
		// Whether that joint is of the posed skeleton.
		bool posable = false;
		// Its name (a mesh's), and whether it is drawn (SetPropVisible).
		std::string name;
		bool visible = true;
	};

	void updateJoints(float frame, bool loop);
	Matrix4x4 _restGlobal(int joint) const;

	// The drawable's skeleton, kept for posing.
	std::vector<std::string> _jointNames;
	std::vector<Matrix4x4> _jointRests;
	std::vector<int> _jointParents;
	std::vector<Matrix4x4> _jointGlobals;
	std::vector<Matrix4x4> _jointRotations;
	std::map<std::string, int> _propJoints;
	std::unique_ptr<SkinAnimation> _animation;

	Matrix4x4 _transform = Matrix4x4::Identity;
	bool _visible = true;
	std::vector<VisibilityTrack> _visibilityTracks;
	std::vector<std::unique_ptr<Mesh>> _meshes;
	std::vector<std::unique_ptr<BillboardBatch>> _billboards;
	std::vector<DrawableProp> _props;
};

} // namespace Donut
