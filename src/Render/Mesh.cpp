// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include <Game.h>
#include <Render/Mesh.h>
#include <Render/Shader.h>
#include <Render/SkinModel.h>
#include <Core/Log.h>
#include <set>
#include <cstdint>
#include <vector>

namespace Donut
{

Mesh::Mesh(const P3D::Geometry& geometry): _name(geometry.GetName())
{
	CreateMeshBuffers(geometry);
}

void Mesh::Commit()
{
	CreateVertexBinding();
}

void Mesh::CreateVertexBinding()
{
	static const size_t vertStride = sizeof(Vertex);

	GL::ArrayElement vertexLayout[] = {
	    GL::ArrayElement(_vertexBuffer.get(), 0, 3, GL::AE_FLOAT, vertStride, 0),
	    GL::ArrayElement(_vertexBuffer.get(), 1, 2, GL::AE_FLOAT, vertStride, 3 * sizeof(float)),
	    GL::ArrayElement(_vertexBuffer.get(), 2, 2, GL::AE_FLOAT, vertStride, 5 * sizeof(float)),
	    GL::ArrayElement(_vertexBuffer.get(), 3, 4, GL::AE_FLOAT, vertStride, 7 * sizeof(float)),
	    GL::ArrayElement(_vertexBuffer.get(), 4, 3, GL::AE_FLOAT, vertStride, 11 * sizeof(float)),
	};

	_vertexBinding = std::make_shared<GL::VertexBinding>();
	_vertexBinding->Create(vertexLayout, 5, *_indexBuffer, GL::ElementType::AE_UINT);
}

void Mesh::CreateMeshBuffers(const P3D::Geometry& geometry)
{
	std::vector<Vertex> allVerts;
	std::vector<uint32_t> allIndices;

	size_t vertOffset = 0;
	size_t idxOffset = 0;
	for (auto const& prim : geometry.GetPrimitiveGroups())
	{
		auto verts = prim->GetVertices();
		auto uvs = prim->GetUvs(0);
		auto colors = prim->GetColors();
		auto indices = prim->GetIndices();
		// A group with no index list (the frontend's living room: 280 of them)
		// draws its vertices in order; with none, it drew nothing.
		if (indices.empty())
		{
			indices.resize(verts.size());
			for (std::size_t i = 0; i < indices.size(); ++i)
				indices[i] = static_cast<uint32_t>(i);
		}
		bool hasColors = !colors.empty();
		const auto& normals = prim->GetNormals();
		const bool hasNormals = normals.size() == verts.size();

		// A second UV channel would be a lightmap's. None of the game's 655
		// files with UV lists has one (19866 lists, all channel 0): its world
		// is lit by its vertex colours.
		std::vector<Vector2> lightmapUVs;
		if (prim->GetUvChannelCount() > 1)
			lightmapUVs = prim->GetUvs(1);
		bool hasLightmap = !lightmapUVs.empty();

		for (uint32_t i = 0; i < verts.size(); i++)
		{
			Vector2 lmUV = hasLightmap ? Vector2(lightmapUVs[i].X, 1.0f - lightmapUVs[i].Y)
			                           : Vector2(uvs[i].X, 1.0f - uvs[i].Y);
			allVerts.push_back(Vertex {
			    verts[i],
			    Vector2(uvs[i].X, 1.0f - uvs[i].Y),
			    lmUV,
			    hasColors ? P3D::P3DUtil::ConvertColor(colors[i]) : Vector4(1.0f, 1.0f, 1.0f, 1.0f),
			    hasNormals ? normals[i] : Vector3(0.0f, 1.0f, 0.0f),
			});
		}

		_groupVertexStart.push_back(vertOffset);
		for (auto const& idx : indices) { allIndices.push_back(idx + static_cast<uint32_t>(vertOffset)); }

		vertOffset += verts.size();

		GLenum mode = GL_TRIANGLE_STRIP;
		switch ((P3D::PrimitiveType)prim->GetPrimType())
		{
		case P3D::PrimitiveType::TriangleStrip: mode = GL_TRIANGLE_STRIP; break;
		case P3D::PrimitiveType::TriangleList: mode = GL_TRIANGLES; break;
		case P3D::PrimitiveType::LineStrip: mode = GL_LINE_STRIP; break;
		case P3D::PrimitiveType::LineList: mode = GL_LINES; break;
		}

		_primGroups.emplace_back(PrimGroup {prim->GetShaderName(), mode, idxOffset, indices.size()});
		idxOffset += indices.size();
	}

	if (!allVerts.empty())
	{
		_bounds = BoundingBox(allVerts[0].pos, allVerts[0].pos);
		for (const auto& vertex : allVerts)
			_bounds.Merge(vertex.pos);
	}

	_vertexBuffer = std::make_shared<GL::VertexBuffer>(allVerts.data(), allVerts.size(), sizeof(Vertex));
	_vertices = std::move(allVerts);
	_indexBuffer = std::make_shared<GL::IndexBuffer>(allIndices.data(), allIndices.size(), GL_UNSIGNED_INT);
}

void Mesh::Draw(GL::ShaderProgram& shader, bool opaque)
{
	_vertexBinding->Bind();

	for (auto& prim : _primGroups)
	{
		if (prim.cacheShader == nullptr)
			prim.cacheShader = Game::GetInstance().GetResourceManager().GetShader(prim.shaderName);
		// A shader no file loaded (some of level 4's and 6's): the group is
		// left undrawn rather than drawn through a null shader.
		if (prim.cacheShader == nullptr)
		{
			static std::set<std::string> warned;
			if (warned.insert(prim.shaderName).second)
				Log::Warn("Mesh: no shader '{}', not drawn", prim.shaderName);
			continue;
		}

		bool trans = (!prim.cacheShader->IsAlphaTested() && prim.cacheShader->IsTranslucent());

		if (trans && opaque)
		{
			continue;
		}

		if (!trans && !opaque)
		{
			continue;
		}

		if (!opaque)
			ApplyBlendMode(prim.cacheShader->GetBlendMode());

		// How the level's lights light it, when they do (the cars).
		if (shader.HasUniform("materialLit"))
		{
			shader.SetUniformValue("materialLit", prim.cacheShader->IsLit() ? 1 : 0);
			shader.SetUniformValue("materialAmbient", prim.cacheShader->GetAmbient());
			shader.SetUniformValue("materialDiffuse", prim.cacheShader->GetDiffuse());
			shader.SetUniformValue("materialEmissive", prim.cacheShader->GetEmissive());
		}
		// The cars' paint: its sphere map's reflection, added.
		if (shader.HasUniform("envColor"))
		{
			const Texture* env = prim.cacheShader->GetEnvTexture();
			shader.SetUniformValue("envColor", env ? prim.cacheShader->GetEnvColor() : Vector3::Zero);
			if (env)
			{
				env->Bind(2);
				glBindSampler(2, 0);
				shader.SetUniformValue("envTex", 2);
			}
		}
		if (shader.HasUniform("alphaMask"))
			shader.SetUniformValue("alphaMask", (prim.cacheShader->IsAlphaTested()) ? 0.5f : 0.0f);

		if (prim.cacheShader->IsTwoSided())
			glDisable(GL_CULL_FACE);
		prim.cacheShader->Bind(0);

		DrawPrimGroup(prim);

		if (prim.cacheShader->IsTwoSided())
			glEnable(GL_CULL_FACE);
	}
	if (!opaque)
		ResetBlendEquation();

	_vertexBinding->Unbind();
}

void Mesh::DrawPrimGroup(const PrimGroup& primGroup)
{
	glDrawElements(primGroup.type, static_cast<GLsizei>(primGroup.indicesCount), _indexBuffer->GetType(),
	               reinterpret_cast<void*>(primGroup.indicesOffset * 4));
}

MeshInstanced::MeshInstanced(const P3D::Geometry& geometry, const std::vector<Matrix4x4>& transforms)
    : Mesh(geometry), _transforms(std::move(transforms))
{
}

void MeshInstanced::SetTransform(std::size_t instance, const Matrix4x4& transform)
{
	if (instance >= _transforms.size())
		return;
	_transforms[instance] = transform;
	_instancesBoundsStale = true;
	if (_instanceBuffer)
		_instanceBuffer->UpdateBuffer(&_transforms[instance], instance * sizeof(Matrix4x4), sizeof(Matrix4x4));
}

const BoundingBox& MeshInstanced::GetInstancesBounds()
{
	if (!_instancesBoundsStale)
		return _instancesBounds;
	_instancesBoundsStale = false;
	bool first = true;
	for (const auto& transform : _transforms)
	{
		if (transform.M[0][0] == 0.0f && transform.M[0][1] == 0.0f && transform.M[0][2] == 0.0f)
			continue;
		const BoundingBox box = _bounds.Transform(transform);
		if (first)
			_instancesBounds = box;
		else
		{
			_instancesBounds.Merge(box.GetMin());
			_instancesBounds.Merge(box.GetMax());
		}
		first = false;
	}
	if (first)
		_instancesBounds = BoundingBox();
	return _instancesBounds;
}

void MeshInstanced::CreateVertexBinding()
{
	static const size_t vertStride = sizeof(Mesh::Vertex);
	static const size_t instanceStride = sizeof(Matrix4x4);

	_instanceBuffer = std::make_shared<GL::VertexBuffer>(_transforms.data(), _transforms.size(), instanceStride);

	GL::ArrayElement vertexLayout[] = {
	    GL::ArrayElement(_vertexBuffer.get(), 0, 3, GL::AE_FLOAT, vertStride, 0),
	    GL::ArrayElement(_vertexBuffer.get(), 1, 2, GL::AE_FLOAT, vertStride, 3 * sizeof(float)),
	    GL::ArrayElement(_vertexBuffer.get(), 2, 4, GL::AE_FLOAT, vertStride, 5 * sizeof(float)),

	    GL::ArrayElement(_instanceBuffer.get(), 3, 4, GL::AE_FLOAT, instanceStride, 0, 1),
	    GL::ArrayElement(_instanceBuffer.get(), 4, 4, GL::AE_FLOAT, instanceStride, 4 * sizeof(float), 1),
	    GL::ArrayElement(_instanceBuffer.get(), 5, 4, GL::AE_FLOAT, instanceStride, 8 * sizeof(float), 1),
	    GL::ArrayElement(_instanceBuffer.get(), 6, 4, GL::AE_FLOAT, instanceStride, 12 * sizeof(float), 1),
	};

	_vertexBinding = std::make_shared<GL::VertexBinding>();
	_vertexBinding->Create(vertexLayout, 7, *_indexBuffer, GL::ElementType::AE_UINT);
}

void MeshInstanced::DrawPrimGroup(const PrimGroup& primGroup)
{
	glDrawElementsInstanced(primGroup.type, static_cast<GLsizei>(primGroup.indicesCount), _indexBuffer->GetType(),
	                        reinterpret_cast<void*>(primGroup.indicesOffset * 4), (GLsizei)_transforms.size());
}

void Mesh::OffsetGroup(std::size_t group, const std::vector<Vector3>& offsets)
{
	if (group >= _groupVertexStart.size() || !_vertexBuffer)
		return;
	const std::size_t start = _groupVertexStart[group];
	const std::size_t end = group + 1 < _groupVertexStart.size() ? _groupVertexStart[group + 1] : _vertices.size();
	std::vector<Vertex> moved(_vertices.begin() + start, _vertices.begin() + end);
	for (std::size_t i = 0; i < moved.size() && i < offsets.size(); ++i)
	{
		moved[i].pos += offsets[i];
		_bounds.Merge(moved[i].pos);
	}
	_vertexBuffer->UpdateBuffer(moved.data(), start * sizeof(Vertex), moved.size() * sizeof(Vertex));
}

} // namespace Donut
