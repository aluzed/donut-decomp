// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <P3D/P3D.generated.h>
#include <Render/BillboardBatch.h>
#include <Core/Math/Vector4.h>
#include <Render/CompositeModel.h>
#include <Render/Mesh.h>
#include <ResourceManager.h>
#include <Skeleton.h>
#include <string>
#include <vector>

namespace Donut
{

// class Skeleton;

class WorldSphere
{
	struct Prop
	{
		int skeleton_joint;
		bool translucent;
		std::unique_ptr<Mesh> mesh;

		Prop(std::unique_ptr<Mesh> pMesh, int pJoint): mesh(std::move(pMesh)), skeleton_joint(pJoint) {}
	};

public:
	WorldSphere(const P3D::WorldSphere&);

	// Its meshes with `shader`, its billboards (clouds, the sun's halo) with
	// `billboardShader`, whose vertex layout they have.
	void Draw(GL::ShaderProgram& shader, GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj, bool opaque) const;
	void Update(double deltatime);
	// Its lens flare (LensFlare, 0x3F0000D: the sun's glares, on its own
	// skeleton's joints), over the world drawn by then: each glare seen as the
	// camera looks within its range of the sun (its display info's source
	// range, fading over its edge range), dimmed as the sun is hidden. How
	// much of the sun showed is asked of the GPU (an occlusion query on the
	// smallest glare), its answer read a frame later.
	void DrawLensFlare(GL::ShaderProgram& billboardShader, const Matrix4x4& viewProj);
	~WorldSphere();

private:
	std::string _name;
	std::vector<std::unique_ptr<BillboardBatch>> _billboardBatches;
	std::vector<Prop> _props;
	std::unique_ptr<CompositeModel> _compositeModel;

	std::unique_ptr<Skeleton> _skeleton;
	std::unique_ptr<SkinAnimation> _animation;

	double _animTime;

	struct Flare
	{
		std::unique_ptr<BillboardBatch> batch;
		int joint = 0;
		Matrix4x4 at = Matrix4x4::Identity;
		float sourceRange = 0.5f, edgeRange = 0.1f;
		std::vector<Vector4> colors;
	};
	std::vector<Flare> _flares;
	std::unique_ptr<Skeleton> _flareSkeleton;
	// The occlusion queries (the sun tested against the world, and alone),
	// whether they have been asked, and how much of it showed.
	unsigned int _queries[2] = {0, 0};
	bool _asked = false;
	float _sunShown = 0.0f;
};

} // namespace Donut
