// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "WorldSphere.h"

#include "Game.h"
#include "Core/Log.h"
#include "Render/LineRenderer.h"
#include "Render/SkinAnimation.h"
#include "Skeleton.h"

#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/glad/glad.h"

#include <algorithm>
#include <cmath>
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

	// Its lens flare: the glares (billboard groups) on the joints of its own
	// skeleton ("sunshine"), as its composite drawable places them.
	if (const auto& lensFlare = worldSphere.GetLensFlare(); lensFlare && lensFlare->GetCompositeDrawable())
	{
		const auto& drawable = *lensFlare->GetCompositeDrawable();
		for (const auto& skeleton : worldSphere.GetSkeletons())
			if (skeleton->GetName() == drawable.GetSkeletonName())
				_flareSkeleton = std::make_unique<Skeleton>(*skeleton);
		if (_flareSkeleton)
		{
			_flareSkeleton->ResetPose();
			for (const auto& group : lensFlare->GetBillboards())
			{
				Flare flare;
				flare.batch = std::make_unique<BillboardBatch>(*group);
				for (const auto& prop : drawable.GetPropList()->GetProps())
					if (prop->GetName() == group->GetName())
						flare.joint = static_cast<int>(prop->GetSkeletonJoint());
				// Where its joint stands: its bind pose, the inverse of the
				// skinning's (finalGlobal at rest is that inverse, the sun
				// below the ground).
				flare.at = _flareSkeleton->GetJoint(flare.joint).restGlobalInverse.Inverse();
				for (const auto& quad : group->GetQuads())
				{
					flare.colors.push_back(P3D::P3DUtil::ConvertColor(quad->GetColor()));
					if (const auto& display = quad->GetDisplayInfo())
					{
						flare.sourceRange = display->GetSourceRange();
						flare.edgeRange = std::max(0.01f, display->GetEdgeRange());
					}
				}
				_flares.push_back(std::move(flare));
			}
			const Vector3 sun = (_flares.empty() ? Matrix4x4::Identity : _flares.front().at).Translation();
			Log::Info("WorldSphere: '{}' has a lens flare of {} glares, its sun at ({:.0f}, {:.0f}, {:.0f})", _name,
			          _flares.size(), sun.X, sun.Y, sun.Z);
		}
	}

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

WorldSphere::~WorldSphere()
{
	if (_queries[0])
		glDeleteQueries(2, _queries);
}

void WorldSphere::DrawLensFlare(GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj)
{
	if (_flares.empty())
		return;
	// Where the camera is and looks, from the matrix it draws with.
	const Matrix4x4 inverse = viewProj.Inverse();
	const auto unproject = [&](float z) {
		const Vector4 p = inverse * Vector4(0.0f, 0.0f, z, 1.0f);
		return Vector3(p.X / p.W, p.Y / p.W, p.Z / p.W);
	};
	const Vector3 eye = unproject(-1.0f);
	Vector3 forward = (unproject(1.0f) - eye).Normalized();

	// How much of the sun showed last frame: the glare tested against the
	// world's depth, and on its own.
	if (!_queries[0])
		glGenQueries(2, _queries);
	if (_asked)
	{
		GLuint ready = 0;
		glGetQueryObjectuiv(_queries[1], GL_QUERY_RESULT_AVAILABLE, &ready);
		if (ready)
		{
			GLuint shown = 0, all = 0;
			glGetQueryObjectuiv(_queries[0], GL_QUERY_RESULT, &shown);
			glGetQueryObjectuiv(_queries[1], GL_QUERY_RESULT, &all);
			_sunShown = all > 0 ? std::min(1.0f, static_cast<float>(shown) / static_cast<float>(all)) : 0.0f;
			_asked = false;
		}
	}

	billboardShader.Bind();
	const GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
	GLboolean depthWrites = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrites);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	const Flare& smallest = _flares.front();
	if (!_asked)
	{
		// Unseen: the colour left alone, the depth tested, then not.
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		billboardShader.SetUniformValue("viewProj", viewProj * smallest.at);
		for (int pass = 0; pass < 2; ++pass)
		{
			if (pass == 0)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			glBeginQuery(GL_SAMPLES_PASSED, _queries[pass]);
			smallest.batch->Draw(billboardShader, false);
			glEndQuery(GL_SAMPLES_PASSED);
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		_asked = true;
	}

	// The glares, over everything, as near the sun as the camera looks.
	glDisable(GL_DEPTH_TEST);
	for (Flare& flare : _flares)
	{
		const Matrix4x4& at = flare.at;
		Vector3 toward = (at.Translation() - eye).Normalized();
		const float angle = std::acos(std::clamp(toward.Dot(forward), -1.0f, 1.0f));
		const float seen =
		    std::clamp((flare.sourceRange + flare.edgeRange - angle) / flare.edgeRange, 0.0f, 1.0f) * _sunShown;
		if (seen <= 0.0f)
			continue;
		for (std::size_t quad = 0; quad < flare.colors.size(); ++quad)
		{
			const Vector4& color = flare.colors[quad];
			flare.batch->SetQuadColor(static_cast<int>(quad),
			                          Vector4(color.X * seen, color.Y * seen, color.Z * seen, color.W * seen));
		}
		billboardShader.SetUniformValue("viewProj", viewProj * at);
		flare.batch->Draw(billboardShader, false);
	}
	billboardShader.SetUniformValue("viewProj", viewProj);
	glDepthMask(depthWrites);
	if (depthTest)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void WorldSphere::Update(double deltatime)
{
	_animTime += deltatime;

	if (_animation != nullptr)
		_skeleton->UpdatePose(*_animation, _animTime);

	Game::GetInstance().GetLineRenderer().DrawSkeleton(Vector3::Zero, *_skeleton);
}

} // namespace Donut
