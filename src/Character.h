// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"
#include "Render/TextureAnimation.h"
#include "Skeleton.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Donut
{
namespace GL
{
class ShaderProgram;
class TextureBuffer;
class GLTexture2D;
} // namespace GL

namespace P3D
{
class Animation;
class P3DChunk;
class PolySkin;
class Shader;
class Skeleton;
class Texture;
} // namespace P3D

class CharacterController;
class SkinModel;
class SkinAnimation;
class ResourceManager;
class Skeleton;

struct SkeletonJoint
{
	std::string name;
	uint32_t parent;
	Matrix4x4 restPose;

	SkeletonJoint(std::string name, const uint32_t parent, const Matrix4x4 restPose)
	    : name(std::move(name)), parent(parent), restPose(restPose)
	{
	}
};

class Character
{
public:
	Character(std::string name);
	~Character();

	void LoadModel(const std::string& name);
	void LoadAnimations(const std::string& name);

	const std::string& GetName() const { return _name; }
	const std::string& GetModelName() const { return _modelName; }
	const std::string& GetAnimName() const { return _animName; }
	// Puts the character there, its controller standing it on the ground.
	void SetPosition(const Vector3& position);
	// Where its controller has moved it, this step; no warp, which would set it
	// down and stop any jump or fall.
	void FollowController(const Vector3& position) { _position = position; }
	void SetRotation(const Quaternion& rotation);
	const Vector3& GetPosition() const { return _position; }
	const Quaternion& GetRotation() const { return _rotation; }
	// `loop` false plays it once and holds its last frame: a jump's take-off,
	// a victory. IsAnimationFinished says when it has played through.
	void SetAnimation(const std::string&, bool loop = true);
	bool IsAnimationFinished() const;
	// How far through the current animation, 0 to 1.
	float GetAnimationProgress() const;
	CharacterController& GetCharacterController() const { return *_characterController; }
	Skeleton& GetSkeleton() const { return *_skeleton; }
	// A joint of its skeleton, as posed now; null when it has no such joint.
	const Skeleton::Joint* FindJoint(const std::string& name) const;

	void Update(double deltatime);
	// How open its mouth is, 0 to 1 (its Jaw joint turned): a line being said.
	void SetMouthOpen(float open) { _mouthOpen = open; }
	void Draw(const Matrix4x4& viewProjection, GL::ShaderProgram&, const ResourceManager&);

	// maybe change this to just anim names
	const std::unordered_map<std::string, std::unique_ptr<SkinAnimation>>& GetAnimations() const { return _animations; }
	// False for a character file that is a skeleton alone (npd_m.p3d).
	bool HasMesh() const;

private:
	void addAnimation(const P3D::Animation&);

	double _animTime = 0.0;
	float _mouthOpen = 0.0f;
	// Its Jaw joint, -2 until looked for, -1 when it has none.
	int _jaw = -2;
	bool _animLoops = true;

	// only used as an entity name for now
	std::string _name;

	std::string _modelName; // just for debug select
	std::string _animName;  // just for debug select

	Vector3 _position;
	Quaternion _rotation;

	std::unique_ptr<CharacterController> _characterController;

	std::unique_ptr<SkinModel> _skinModel; // use std::weak_ptr when SkinModels are created as a shared_ptr
	std::unique_ptr<Skeleton> _skeleton;

	std::unique_ptr<GL::TextureBuffer> _boneBuffer;

	// animations / bone buffers
	std::unordered_map<std::string, std::unique_ptr<SkinAnimation>> _animations;
	// Null until SetAnimation; it was left uninitialised, and a character with
	// no animation file (most pedestrians) posed its skeleton from garbage.
	SkinAnimation* _currentAnimation = nullptr;

	// The eyes' texture animation (TEX_eyeball_m): the shader it retextures,
	// the texture at each key frame, its rate; and when the next blink is due.
	std::optional<TextureAnimation> _blink;
	double _blinkAt = 0.0;
	double _blinkTime = -1.0;
};
} // namespace Donut
