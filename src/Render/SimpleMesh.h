// Copyright 2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Render/OpenGL/IndexBuffer.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"

#include <memory>
#include <vector>

namespace Donut
{
namespace GL { class ShaderProgram; }

class SimpleMesh
{
public:
	struct Vertex
	{
		Vector3 pos;
		Vector3 normal;
		Vector2 uv;
	};

	static std::unique_ptr<SimpleMesh> CreateBox(const Vector3& halfExtents, const Vector4& color);
	static std::unique_ptr<SimpleMesh> CreateCapsule(float radius, float height, const Vector4& color, int segments = 8);

	void Draw(GL::ShaderProgram& shader, const Matrix4x4& model, const Matrix4x4& viewProj,
	          const Vector4& color);

private:
	SimpleMesh() = default;

	std::unique_ptr<GL::VertexBuffer> _vertexBuffer;
	std::unique_ptr<GL::IndexBuffer> _indexBuffer;
	std::unique_ptr<GL::VertexBinding> _vertexBinding;

	Vector4 _color;
	std::size_t _indexCount = 0;
};

} // namespace Donut
