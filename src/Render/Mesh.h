// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/BoundingBox.h"
#include "Core/Math/Fwd.h"
#include "P3D/P3D.generated.h"
#include "Render/OpenGL/IndexBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/SkinAnimation.h"

#include <string>

namespace Donut
{

class Shader;

class Mesh
{
public:
	Mesh(const P3D::Geometry& geometry);

	void Commit();
	void Draw(GL::ShaderProgram&, bool opaque);

	const std::string& GetName() const { return _name; }
	// Moves the vertices of primitive group `group` from where they were built
	// by `offsets`, one per vertex (a morph target: an inflating doll). Fewer
	// offsets than vertices leave the rest as built.
	void OffsetGroup(std::size_t group, const std::vector<Vector3>& offsets);
	// The box its vertices fill, in its own space (grown by OffsetGroup).
	const BoundingBox& GetBounds() const { return _bounds; }
	// What it takes: its vertices kept here, and its buffers on the graphics
	// card.
	std::size_t GetCpuBytes() const { return _vertices.capacity() * sizeof(Vertex); }
	std::size_t GetGpuBytes() const { return _gpuBytes; }

protected:
	struct PrimGroup
	{
		std::string shaderName;
		GLenum type;
		std::size_t indicesOffset;
		std::size_t indicesCount;
		Shader* cacheShader;
	};

	struct Vertex
	{
		Vector3 pos;
		Vector2 uv;
		Vector2 lightmapUV;
		Vector4 color;
		// Its normal, for a mesh lit by the level's lights (a car's); up when
		// it has none.
		Vector3 normal;
	};

	void CreateMeshBuffers(const P3D::Geometry& geometry);
	virtual void CreateVertexBinding();

	virtual void DrawPrimGroup(const PrimGroup& primGroup);

	std::string _name;
	// The resources' load index when it was made: its shaders are those of
	// its own file (ResourceManager::GetShader).
	uint64_t _loadIndex = 0;
	std::vector<PrimGroup> _primGroups;
	// Kept for OffsetGroup: every vertex as built, and where each group's start.
	std::vector<Vertex> _vertices;
	std::vector<std::size_t> _groupVertexStart;
	std::size_t _gpuBytes = 0;

	std::shared_ptr<GL::VertexBuffer> _vertexBuffer;
	std::shared_ptr<GL::IndexBuffer> _indexBuffer;
	std::shared_ptr<GL::VertexBinding> _vertexBinding;

	BoundingBox _bounds;
};

class MeshInstanced: public Mesh
{
public:
	MeshInstanced(const P3D::Geometry& geometry, const std::vector<Matrix4x4>& transforms);

	// Moves one of its instances (a prop knocked about).
	void SetTransform(std::size_t instance, const Matrix4x4& transform);
	// The box all its instances fill, in the world; one gone (at zero scale)
	// left out.
	const BoundingBox& GetInstancesBounds();

protected:
	virtual void CreateVertexBinding() override;

	virtual void DrawPrimGroup(const PrimGroup& primGroup) override;

	std::vector<Matrix4x4> _transforms;
	std::shared_ptr<GL::VertexBuffer> _instanceBuffer;
	BoundingBox _instancesBounds;
	bool _instancesBoundsStale = true;
};

} // namespace Donut
