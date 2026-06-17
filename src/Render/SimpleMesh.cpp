// Copyright 2020 the donut authors. See AUTHORS.md

#include "Render/SimpleMesh.h"
#include "Render/OpenGL/glad/glad.h"

namespace Donut
{

std::unique_ptr<SimpleMesh> SimpleMesh::CreateBox(const Vector3& halfExtents, const Vector4& color)
{
	auto mesh = std::unique_ptr<SimpleMesh>(new SimpleMesh());
	mesh->_color = color;

	float x = halfExtents.X, y = halfExtents.Y, z = halfExtents.Z;

	std::vector<Vertex> verts = {
		{{-x,-y,-z}, { 0,-1, 0}, {0,0}}, {{ x,-y,-z}, { 0,-1, 0}, {1,0}}, {{ x,-y, z}, { 0,-1, 0}, {1,1}}, {{-x,-y, z}, { 0,-1, 0}, {0,1}},
		{{-x, y,-z}, { 0, 1, 0}, {0,0}}, {{ x, y,-z}, { 0, 1, 0}, {1,0}}, {{ x, y, z}, { 0, 1, 0}, {1,1}}, {{-x, y, z}, { 0, 1, 0}, {0,1}},
		{{-x,-y,-z}, {-1, 0, 0}, {0,0}}, {{-x, y,-z}, {-1, 0, 0}, {1,0}}, {{-x, y, z}, {-1, 0, 0}, {1,1}}, {{-x,-y, z}, {-1, 0, 0}, {0,1}},
		{{ x,-y,-z}, { 1, 0, 0}, {0,0}}, {{ x, y,-z}, { 1, 0, 0}, {1,0}}, {{ x, y, z}, { 1, 0, 0}, {1,1}}, {{ x,-y, z}, { 1, 0, 0}, {0,1}},
		{{-x,-y, z}, { 0, 0, 1}, {0,0}}, {{ x,-y, z}, { 0, 0, 1}, {1,0}}, {{ x, y, z}, { 0, 0, 1}, {1,1}}, {{-x, y, z}, { 0, 0, 1}, {0,1}},
		{{-x,-y,-z}, { 0, 0,-1}, {0,0}}, {{ x,-y,-z}, { 0, 0,-1}, {1,0}}, {{ x, y,-z}, { 0, 0,-1}, {1,1}}, {{-x, y,-z}, { 0, 0,-1}, {0,1}},
	};

	std::vector<uint32_t> indices = {
		0,1,2, 0,2,3,  4,6,5, 4,7,6,
		8,9,10, 8,10,11,  12,14,13, 12,15,14,
		16,17,18, 16,18,19,  20,22,21, 20,23,22,
	};

	mesh->_indexCount = indices.size();
	mesh->_vertexBuffer = std::make_unique<GL::VertexBuffer>(verts.data(), verts.size(), sizeof(Vertex));
	mesh->_indexBuffer = std::make_unique<GL::IndexBuffer>(indices.data(), indices.size(), GL_UNSIGNED_INT);

	GL::ArrayElement layout[] = {
		GL::ArrayElement(mesh->_vertexBuffer.get(), 0, 3, GL::AE_FLOAT, sizeof(Vertex), 0),
		GL::ArrayElement(mesh->_vertexBuffer.get(), 1, 3, GL::AE_FLOAT, sizeof(Vertex), 3 * sizeof(float)),
		GL::ArrayElement(mesh->_vertexBuffer.get(), 2, 2, GL::AE_FLOAT, sizeof(Vertex), 6 * sizeof(float)),
	};

	mesh->_vertexBinding = std::make_unique<GL::VertexBinding>();
	mesh->_vertexBinding->Create(layout, 3, *mesh->_indexBuffer, GL::AE_UINT);

	return mesh;
}

std::unique_ptr<SimpleMesh> SimpleMesh::CreateCapsule(float radius, float height, const Vector4& color, int segments)
{
	auto mesh = std::unique_ptr<SimpleMesh>(new SimpleMesh());
	mesh->_color = color;

	std::vector<Vertex> verts;
	std::vector<uint32_t> indices;

	float halfH = height * 0.5f;

	for (int i = 0; i <= segments; ++i)
	{
		float angle = (float)i / segments * 3.14159f * 2.0f;
		float cx = cosf(angle), sz = sinf(angle);

		verts.push_back({{cx * radius,  halfH, sz * radius}, {cx, 0, sz}, {0, 0}});
		verts.push_back({{cx * radius, -halfH, sz * radius}, {cx, 0, sz}, {0, 1}});

		if (i < segments)
		{
			uint32_t a = i * 2, b = a + 1, c = a + 2, d = a + 3;
			indices.insert(indices.end(), {a, b, c, c, b, d});
		}
	}

	mesh->_indexCount = indices.size();
	mesh->_vertexBuffer = std::make_unique<GL::VertexBuffer>(verts.data(), verts.size(), sizeof(Vertex));
	mesh->_indexBuffer = std::make_unique<GL::IndexBuffer>(indices.data(), indices.size(), GL_UNSIGNED_INT);

	GL::ArrayElement layout[] = {
		GL::ArrayElement(mesh->_vertexBuffer.get(), 0, 3, GL::AE_FLOAT, sizeof(Vertex), 0),
		GL::ArrayElement(mesh->_vertexBuffer.get(), 1, 3, GL::AE_FLOAT, sizeof(Vertex), 3 * sizeof(float)),
		GL::ArrayElement(mesh->_vertexBuffer.get(), 2, 2, GL::AE_FLOAT, sizeof(Vertex), 6 * sizeof(float)),
	};

	mesh->_vertexBinding = std::make_unique<GL::VertexBinding>();
	mesh->_vertexBinding->Create(layout, 3, *mesh->_indexBuffer, GL::AE_UINT);

	return mesh;
}

void SimpleMesh::Draw(GL::ShaderProgram& shader, const Matrix4x4& model, const Matrix4x4& viewProj,
                      const Vector4& color)
{
	shader.SetUniformValue("viewProj", viewProj * model);
	shader.SetUniformValue("meshColor", color);
	_vertexBinding->Bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indexCount), GL_UNSIGNED_INT, nullptr);
	_vertexBinding->Unbind();
}

} // namespace Donut
