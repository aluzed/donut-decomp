// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "WorldSphere.h"

#include "Game.h"
#include "Render/LineRenderer.h"
#include "Render/SkinAnimation.h"
#include "Skeleton.h"

#include <cstdint>

namespace Donut
{

WorldSphere::WorldSphere(const P3D::WorldSphere& worldSphere): _name(worldSphere.GetName())
{
	std::unordered_map<std::string, const P3D::Geometry*> meshes;
	for (auto const& p3dmesh : worldSphere.GetGeometries())
		meshes[p3dmesh->GetName()] = p3dmesh.get();

	for (auto const& billboardGroup : worldSphere.GetBillboards())
	{
		auto batch = std::make_unique<BillboardBatch>(*billboardGroup);
		_billboardBatches.push_back(std::move(batch));
	}

	auto const& compositeDrawableList = worldSphere.GetCompositeDrawable()->GetPropList()->GetProps();
	for (auto const& p3dprop : compositeDrawableList)
	{
		const auto geometry = meshes.find(p3dprop->GetName());
		if (geometry == meshes.end())
			continue;

		// A mesh of its own for each prop: level 5's sky has two props of one
		// name, and the second used to get the first's, already moved out --
		// a null mesh, drawn every frame.
		auto mesh = std::make_unique<Mesh>(*geometry->second);
		mesh->Commit();
		_props.emplace_back(std::move(mesh), p3dprop->GetSkeletonJoint());
	}

	// should be horizon1
	_skeleton = std::make_unique<Skeleton>(*worldSphere.GetSkeletons()[0]);

	// load animation
	auto const& p3dAnim = *worldSphere.GetAnimation();
	// The same conversion as everything else's: it read uncompressed
	// quaternions in file order, w first, and a Vector2 channel as if its held
	// axis were always Y.
	std::vector<std::string> names;
	std::vector<Matrix4x4> rests;
	for (const auto& joint : _skeleton->GetJoints())
	{
		names.push_back(joint.name);
		rests.push_back(joint.rest);
	}
	_animation = SkinAnimation::FromP3D(p3dAnim, names, rests);

	_animTime = 0.0;

	// worldSphere.GetCompositeDrawable

	// Animation
	// Skeleton
	// OldBillboardQuadGroup
	// Mesh
	// Composite Drawable
	// Old Frame Controller
	// Multi Controller
	// Skeleton
	// Lens Flare
}

void WorldSphere::Draw(GL::ShaderProgram& shader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj,
                       bool opaque) const
{
	for (auto const& prop : _props)
	{
		const auto& joint = _skeleton->GetJoint(prop.skeleton_joint);
		shader.SetUniformValue("viewProj", viewProj * joint.finalGlobal);
		prop.mesh->Draw(shader, opaque);
	}

	shader.SetUniformValue("viewProj", viewProj);

	// Drawn with the world's shader, whose vertex layout is not theirs, they
	// were scattered translucent panels across the sky.
	billboardShader.Bind();
	billboardShader.SetUniformValue("viewProj", viewProj);
	for (auto const& billboard : _billboardBatches)
		billboard->Draw(billboardShader, opaque);
	shader.Bind();
}

void WorldSphere::Update(double deltatime)
{
	_animTime += deltatime;

	if (_animation != nullptr)
		_skeleton->UpdatePose(*_animation, _animTime);

	Game::GetInstance().GetLineRenderer().DrawSkeleton(Vector3::Zero, *_skeleton);
}

} // namespace Donut
