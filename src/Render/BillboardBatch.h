// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Donut
{
namespace GL
{
class ShaderProgram;
class VertexBuffer;
class IndexBuffer;
class VertexBinding;
} // namespace GL

namespace P3D
{
class BillboardQuadGroup;
}

class Shader;

class BillboardBatch
{
public:
	BillboardBatch(const P3D::BillboardQuadGroup& billboardQuadGroup);

	void Draw(GL::ShaderProgram& shader, bool opaque);

	// The quads by name ("l2z4_sirensShape19"), and a new colour for one: a
	// siren flashing. -1 when there is no such quad.
	int FindQuad(const std::string& name) const;
	void SetQuadColor(int quad, const struct Vector4& color);

private:
	std::shared_ptr<GL::VertexBuffer> _vertexBuffer;
	std::shared_ptr<GL::IndexBuffer> _indexBuffer;
	std::shared_ptr<GL::VertexBuffer> _instanceBuffer;
	std::shared_ptr<GL::VertexBinding> _vertexBinding;

	uint32_t _numQuads;
	std::vector<std::string> _quadNames;
	// Each quad's colour, where it sits in the instance buffer.
	std::vector<std::size_t> _colorOffsets;

	std::string _shader;
	bool _zTest;
	bool _zWrite;
};
} // namespace Donut
